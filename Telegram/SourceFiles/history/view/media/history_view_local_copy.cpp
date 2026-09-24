/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/media/history_view_local_copy.h"

#include "api/api_common.h"
#include "api/api_sending.h"
#include "apiwrap.h"
#include "boxes/share_box.h"
#include "boxes/peer_list_controllers.h"
#include "core/file_utilities.h"
#include "core/mime_type.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_document_media.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/data_chat_participant_status.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "storage/storage_account.h"
#include "storage/localimageloader.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/image/image.h"
#include "ui/layers/show.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>

namespace HistoryView {
namespace {

[[nodiscard]] QString LocalCopyName(not_null<DocumentData*> document) {
	auto name = QFileInfo(document->filename()).fileName();
	if (!name.isEmpty() && !name.contains('*') && !name.contains('?')) {
		return name;
	}
	const auto mime = document->mimeString();
	const auto mimeType = Core::MimeTypeForName(mime);
	const auto patterns = mimeType.globPatterns();
	auto extension = patterns.isEmpty() ? QString() : patterns.front();
	extension.replace('*', QString());
	const auto prefix = document->isVideoFile() || mime.startsWith(u"video/"_q)
		? u"video"_q
		: document->isAudioFile() || mime.startsWith(u"audio/"_q)
		? u"audio"_q
		: mime.startsWith(u"image/"_q)
		? u"photo"_q
		: u"file"_q;
	if (extension.isEmpty()) {
		extension = document->isVideoFile() ? u".mp4"_q : u".unknown"_q;
	}
	return filedialogDefaultName(prefix, extension, QString(), true);
}

[[nodiscard]] QString LocalCopyName(const QString &prefix, const QString &extension) {
	return filedialogDefaultName(prefix, extension, QString(), true);
}

} // namespace

bool HasLocalMediaCopy(not_null<DocumentData*> document) {
	const auto media = document->activeMediaView();
	return (media && !media->bytes().isEmpty()) || !document->filepath(true).isEmpty() || document->loadedInMediaCache();
}

bool HasLocalMediaCopy(not_null<PhotoData*> photo) {
	const auto media = photo->activeMediaView();
	return media && (photo->hasVideo() ? !media->videoContent(Data::PhotoSize::Large).isEmpty() : media->loaded());
}

std::optional<LocalMediaCopy> ReadLocalMediaCopy(not_null<DocumentData*> document) {
	auto result = LocalMediaCopy();
	if (const auto media = document->activeMediaView()) {
		result.bytes = media->bytes();
	}
	result.path = result.bytes.isEmpty() ? document->filepath(true) : QString();
	if (result.bytes.isEmpty() && result.path.isEmpty() && !document->loadedInMediaCache()) {
		return std::nullopt;
	}
	result.name = LocalCopyName(document);
	if (result.bytes.isEmpty() && result.path.isEmpty()) {
		result.document = document;
	}
	return result;
}

std::optional<LocalMediaCopy> ReadLocalMediaCopy(not_null<PhotoData*> photo) {
	const auto media = photo->activeMediaView();
	if (!media) {
		return std::nullopt;
	}
	constexpr auto large = Data::PhotoSize::Large;
	if (photo->hasVideo()) {
		const auto bytes = media->videoContent(large);
		return bytes.isEmpty()
			? std::nullopt
			: std::make_optional(LocalMediaCopy{
				.bytes = bytes,
				.name = LocalCopyName(u"video"_q, u".mp4"_q),
			});
	}
	auto result = LocalMediaCopy{
		.name = LocalCopyName(u"photo"_q, u".jpg"_q),
		.photo = true,
	};
	result.bytes = media->imageBytes(large);
	if (result.bytes.isEmpty()) {
		const auto image = media->image(large);
		if (!image) {
			return std::nullopt;
		}
		auto buffer = QBuffer(&result.bytes);
		if (!buffer.open(QIODevice::WriteOnly) || !image->original().save(&buffer, "JPG")) {
			return std::nullopt;
		}
	}
	return result;
}

std::optional<LocalMessageCopy> ReadLocalMessageCopy(not_null<HistoryItem*> item, bool allowDownload) {
	const auto &text = item->originalText();
	auto result = LocalMessageCopy{
		.text = { text.text, TextUtilities::ConvertEntitiesToTextTags(text.entities) },
	};
	if (const auto media = item->media()) {
		if (const auto document = media->document()) {
			result.media = ReadLocalMediaCopy(document);
			if (!result.media) {
				if (!allowDownload) {
					return std::nullopt;
				}
				result.media = LocalMediaCopy{
					.name = LocalCopyName(document),
					.document = document,
				};
			}
			if (allowDownload) {
				result.media->origin = item->fullId();
			}
		} else if (const auto photo = media->photo()) {
			result.media = allowDownload
				&& item->allowsForward()
				&& !photo->hasVideo()
				&& !photo->fileReference().isEmpty()
				? std::make_optional(LocalMediaCopy{
					.existingPhoto = photo,
					.photo = true,
					.origin = item->fullId(),
				})
				: ReadLocalMediaCopy(photo);
			if (!result.media) {
				return std::nullopt;
			}
		}
	}
	return result.text.text.isEmpty() && !result.media ? std::nullopt : std::make_optional(std::move(result));
}

bool WriteLocalMediaCopy(const LocalMediaCopy &copy, const QString &path) {
	auto source = QFile(copy.path);
	if (copy.bytes.isEmpty() && !source.open(QIODevice::ReadOnly)) {
		return false;
	}
	auto output = QSaveFile(path);
	if (!output.open(QIODevice::WriteOnly)) {
		return false;
	}
	if (!copy.bytes.isEmpty()) {
		if (output.write(copy.bytes) != copy.bytes.size()) {
			return false;
		}
	} else {
		while (!source.atEnd()) {
			const auto bytes = source.read(64 * 1024);
			if (bytes.isEmpty() || output.write(bytes) != bytes.size()) {
				return false;
			}
		}
	}
	return output.commit();
}

namespace {

void MaterializeLocalMediaCopy(LocalMediaCopy copy, const QString &path, not_null<Main::Session*> session, Fn<void(bool)> done) {
	if (!copy.document) {
		done(WriteLocalMediaCopy(copy, path));
		return;
	}
	const auto document = copy.document;
	if (const auto local = ReadLocalMediaCopy(document)) {
		if (!local->document) {
			done(WriteLocalMediaCopy(*local, path));
			return;
		}
	}
	struct State {
		std::shared_ptr<Data::DocumentMedia> media;
		rpl::lifetime lifetime;
		bool finished = false;
	};
	const auto state = std::make_shared<State>();
	state->media = document->createMediaView();
	const auto waiting = document->loading();
	const auto finish = [=] {
		if (state->finished || document->loading()) {
			return;
		}
		state->finished = true;
		state->lifetime.destroy();
		if (waiting) {
			MaterializeLocalMediaCopy(copy, path, session, done);
		} else {
			const auto file = QFileInfo(path);
			done(!document->cancelled()
				&& document->status != FileDownloadFailed
				&& file.isFile()
				&& file.size() == document->size);
		}
	};
	session->data().documentLoadProgress() | rpl::filter([=](not_null<DocumentData*> value) {
		return value == document && !document->loading();
	}) | rpl::on_next_done([=] {
		crl::on_main(session, finish);
	}, [=] {
		state->lifetime.destroy();
	}, state->lifetime);
	if (!waiting) {
		document->save(copy.origin, path, copy.origin ? LoadFromCloudOrLocal : LoadFromLocalOnly);
	}
	crl::on_main(session, finish);
}

} // namespace

void SaveLocalMediaCopy(LocalMediaCopy copy, not_null<Main::Session*> session, std::shared_ptr<Ui::Show> show, QWidget *parent) {
	FileDialog::GetWritePath(
		parent,
		tr::lng_save_file(tr::now),
		FileDialog::AllFilesFilter(),
		copy.name,
		crl::guard(session, [=](const QString &path) {
			if (!path.isEmpty()) {
				MaterializeLocalMediaCopy(copy, path, session, [=](bool success) {
					if (!success) {
						show->showToast(u"Could not save the local file."_q);
					}
				});
			}
		}));
}

namespace {

void ShowPreparedLocalCopies(not_null<Window::SessionController*> controller, std::vector<LocalMessageCopy> copies) {
	const auto session = &controller->session();
	const auto show = controller->uiShow();
	if (copies.empty()) {
		return;
	}
	for (auto index = size_t(0); index != copies.size(); ++index) {
		const auto &copy = copies[index];
		if (copy.media && copy.media->document) {
			const auto directory = session->local().tempDirectory();
			if (!QDir().mkpath(directory)) {
				show->showToast(u"Could not prepare the copy."_q);
				return;
			}
			const auto path = filedialogNextFilename(copy.media->name, QString(), directory);
			const auto mediaCopy = *copy.media;
			const auto prepared = std::make_shared<std::vector<LocalMessageCopy>>(std::move(copies));
			MaterializeLocalMediaCopy(
				mediaCopy,
				path,
				session,
				crl::guard(controller, [=](bool success) {
					if (success) {
						auto &media = *(*prepared)[index].media;
						media.document = nullptr;
						media.path = path;
						ShowPreparedLocalCopies(controller, base::take(*prepared));
					} else {
						show->showToast(u"Could not download or prepare the complete copy. Nothing was sent."_q);
					}
				}));
			return;
		}
	}
	const auto box = std::make_shared<base::weak_qptr<Ui::BoxContent>>();
	const auto sent = std::make_shared<bool>(false);
	const auto allowed = [=](not_null<Data::Thread*> thread) {
		if (!Data::CanSendTexts(thread)) {
			return false;
		}
		for (const auto &copy : copies) {
			if (copy.media && !Data::CanSend(thread, copy.media->photo
				? ChatRestriction::SendPhotos
				: ChatRestriction::SendFiles)) {
				return false;
			}
		}
		return true;
	};
	*box = show->show(Box<ShareBox>(ShareBox::Descriptor{
		.session = session,
		.countMessagesCallback = [=](const TextWithTags &comment) {
			return int(copies.size()) + (comment.text.isEmpty() ? 0 : 1);
		},
		.submitCallback = [=](
				std::vector<not_null<Data::Thread*>> &&threads,
				Fn<bool()> checkPaid,
				TextWithTags &&comment,
				Api::SendOptions options,
				Data::ForwardOptions) {
			if (*sent || threads.empty()) {
				return;
			}
			if (!ranges::all_of(threads, allowed)) {
				show->showToast(u"The recipient does not allow this message type."_q);
				return;
			}
			const auto error = GetErrorForSending(threads, {
				.text = &comment,
				.messagesCount = int(copies.size()) + (comment.text.isEmpty() ? 0 : 1),
			});
			if (error.error) {
				show->showBox(MakeSendErrorBox(error, threads.size() > 1));
				return;
			}
			for (const auto &copy : copies) {
				if (copy.media && !copy.media->existingPhoto && copy.media->bytes.isEmpty() && !QFileInfo(copy.media->path).isFile()) {
					show->showToast(u"The local file is no longer available. Nothing was sent."_q);
					return;
				}
			}
			if (!checkPaid()) {
				return;
			}
			*sent = true;
			for (const auto thread : threads) {
				auto action = Api::SendAction(thread, options);
				action.clearDraft = false;
				if (!comment.text.isEmpty()) {
					auto message = Api::MessageToSend(action);
					message.textWithTags = comment;
					session->api().sendMessage(std::move(message));
				}
				for (const auto &copy : copies) {
					if (copy.media && copy.media->existingPhoto) {
						auto message = Api::MessageToSend(action);
						message.textWithTags = copy.text;
						Api::SendExistingPhoto(std::move(message), copy.media->existingPhoto, std::nullopt, copy.media->origin);
					} else if (copy.media) {
						auto list = Ui::PreparedList();
						auto file = Ui::PreparedFile(copy.media->path);
						file.content = copy.media->bytes;
						file.displayName = copy.media->name;
						file.caption = copy.text;
						list.files.push_back(std::move(file));
						session->api().sendFiles(std::move(list), copy.media->photo ? SendMediaType::Photo : SendMediaType::File, nullptr, action);
					} else {
						auto message = Api::MessageToSend(action);
						message.textWithTags = copy.text;
						session->api().sendMessage(std::move(message));
					}
				}
			}
			if (*box) {
				(*box)->closeBox();
			}
		},
		.filterCallback = allowed,
		.titleOverride = rpl::single(u"Send a Copy"_q),
		.moneyRestrictionError = ShareMessageMoneyRestrictionError(),
	}));
}

} // namespace

void ShowSendLocalCopy(not_null<Window::SessionController*> controller, LocalMessageCopy copy) {
	auto copies = std::vector<LocalMessageCopy>();
	copies.push_back(std::move(copy));
	ShowPreparedLocalCopies(controller, std::move(copies));
}

void ShowSendLocalCopies(not_null<Window::SessionController*> controller, MessageIdsList ids) {
	const auto session = &controller->session();
	const auto show = controller->uiShow();
	auto copies = std::vector<LocalMessageCopy>();
	for (const auto id : ids) {
		const auto item = session->data().message(id);
		const auto copy = item ? ReadLocalMessageCopy(item, true) : std::nullopt;
		if (!copy) {
			show->showToast(u"A complete copy is not available locally. Nothing was downloaded or sent."_q);
			return;
		}
		copies.push_back(*copy);
	}
	ShowPreparedLocalCopies(controller, std::move(copies));
}

} // namespace HistoryView
