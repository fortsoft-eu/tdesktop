/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/boxes/peer_qr_box.h"

#include "core/application.h"
#include "core/file_utilities.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "qr/qr_generate.h"
#include "ui/dynamic_image.h"
#include "ui/dynamic_thumbnails.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/style/style_classic.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/box_content_divider.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_credits.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"

#include <QtCore/QMimeData>
#include <QtGui/QGuiApplication>

namespace Ui {
namespace {

[[nodiscard]] QImage ComposedQrImage(
		const QString &link,
		const QImage &photo,
		bool withPhoto,
		bool transparentBackground,
		int pixel,
		int photoSize,
		QMargins padding) {
	if (link.isEmpty()) {
		return QImage();
	}
	const auto qr = Qr::GenerateStrict(Qr::Encode(link), pixel);
	const auto photoHeight = withPhoto
		? photoSize + st::profileQrPhotoSkip
		: 0;
	const auto contentWidth = std::max(qr.width(), withPhoto ? photoSize : 0);
	const auto resultSize = QSize(
		contentWidth + padding.left() + padding.right(),
		qr.height() + photoHeight + padding.top() + padding.bottom());
	auto result = QImage(resultSize, QImage::Format_ARGB32_Premultiplied);
	result.fill(transparentBackground ? Qt::transparent : Qt::white);
	{
		auto p = QPainter(&result);
		auto top = padding.top();
		if (withPhoto) {
			p.drawImage(
				QRect(
					(result.width() - photoSize) / 2,
					top,
					photoSize,
					photoSize),
				photo);
			top += photoHeight;
		}
		p.drawImage((result.width() - qr.width()) / 2, top, qr);
	}
	return result;
}

struct QrBoxState {
	std::shared_ptr<DynamicImage> userpic;
	rpl::variable<bool> userpicToggled = true;
	rpl::variable<bool> backgroundTransparent = false;
	QString link;
	QImage preview;
	Fn<void()> refresh;
};

not_null<RpWidget*> PrepareQrWidget(
		not_null<VerticalLayout*> container,
		QrBoxState *state,
		rpl::producer<QString> about) {
	const auto divider = container->add(
		object_ptr<BoxContentDivider>(container));
	const auto result = CreateChild<RpWidget>(divider);
	const auto aboutLabel = CreateChild<FlatLabel>(
		divider,
		st::creditsBoxAboutDivider);

	const auto refresh = [=] {
		const auto photoSize = st::defaultUserpicButton.photoSize;
		const auto pixelSize = photoSize * style::DevicePixelRatio();
		state->preview = ComposedQrImage(
			state->link,
			state->userpic->image(pixelSize),
			state->userpicToggled.current(),
			state->backgroundTransparent.current(),
			st::profileQrPreviewPixel,
			photoSize,
			st::profileQrPreviewPadding);
		aboutLabel->resizeToWidth(container->width());
		const auto height = state->preview.height() + aboutLabel->height();
		result->resize(divider->width(), state->preview.height());
		divider->resize(container->width(), height);
		aboutLabel->moveToLeft(0, state->preview.height());
		result->update();
	};
	state->refresh = refresh;

	state->userpic->subscribeToUpdates(crl::guard(result, refresh));
	container->widthValue() | rpl::on_next([=](int width) {
		divider->resize(width, divider->height());
		result->resize(width, result->height());
		aboutLabel->resizeToWidth(width);
		refresh();
	}, result->lifetime());
	rpl::combine(
		state->userpicToggled.value(),
		state->backgroundTransparent.value()
	) | rpl::on_next([=](bool, bool) {
		refresh();
	}, result->lifetime());
	std::move(about) | rpl::on_next([=](const QString &text) {
		aboutLabel->setText(text);
		refresh();
	}, result->lifetime());
	result->paintRequest() | rpl::on_next([=] {
		if (!state->preview.isNull()) {
			QPainter(result).drawImage(
				(result->width() - state->preview.width()) / 2,
				0,
				state->preview);
		}
	}, result->lifetime());
	return result;
}

[[nodiscard]] QImage ExportQrImage(const QrBoxState &state) {
	return ComposedQrImage(
		state.link,
		state.userpic->image(st::profileQrExportPhotoSize),
		state.userpicToggled.current(),
		state.backgroundTransparent.current(),
		st::profileQrExportPixel,
		st::profileQrExportPhotoSize,
		st::profileQrExportPadding);
}

} // namespace

void FillPeerQrBox(
		not_null<GenericBox*> box,
		PeerData *peer,
		std::optional<QString> customLink,
		rpl::producer<QString> about) {
	const auto window = Core::App().findWindow(box);
	const auto controller = window ? window->sessionController() : nullptr;
	if (!controller) {
		return;
	}
	SetClassicSettingsStyle(box);
	const auto boxStyle = box->lifetime().make_state<style::Box>(
		st::defaultBox);
	boxStyle->title.textFg = st::classicMenuText;
	box->setStyle(*boxStyle);
	box->setNoContentMargin(true);
	box->setWidth(st::aboutWidth);
	box->setTitle(tr::lng_group_invite_context_qr());
	box->verticalLayout()->resizeToWidth(box->width());

	const auto state = box->lifetime().make_state<QrBoxState>();
	state->userpicToggled = !(customLink || !peer);
	state->userpic = MakeUserpicThumbnail(peer
		? peer
		: controller->session().user().get());

	const auto linkValue = [=] {
		return customLink
			? rpl::single(*customLink)
			: peer
			? Info::Profile::LinkValue(peer, true) | rpl::map(
				[](const auto &link) { return link.text; })
			: (rpl::single(QString()) | rpl::type_erased);
	};
	linkValue() | rpl::on_next([=](const QString &link) {
		state->link = link;
		if (link.isEmpty()) {
			box->closeBox();
		} else if (state->refresh) {
			state->refresh();
		}
	}, box->lifetime());

	PrepareQrWidget(
		box->verticalLayout(),
		state,
		about ? std::move(about) : rpl::single(QString()));

	Ui::AddSkip(box->verticalLayout());
	if (peer) {
		const auto userpic = box->verticalLayout()->add(
			object_ptr<SettingsButton>(
				box->verticalLayout(),
				(peer->isUser()
					? tr::lng_mediaview_profile_photo
					: (peer->isChannel() && !peer->isMegagroup())
					? tr::lng_mediaview_channel_photo
					: tr::lng_mediaview_group_photo)(),
				st::profileQrOptionButton));
		userpic->setProperty("classicCheckOnLeft", true);
		userpic->toggleOn(state->userpicToggled.value(), true);
		userpic->setClickedCallback([=] {
			state->userpicToggled = !state->userpicToggled.current();
		});
	}
	const auto transparent = box->verticalLayout()->add(
		object_ptr<SettingsButton>(
			box->verticalLayout(),
			tr::lng_qr_box_transparent_background(),
			st::profileQrOptionButton));
	transparent->setProperty("classicCheckOnLeft", true);
	transparent->toggleOn(state->backgroundTransparent.value(), true);
	transparent->setClickedCallback([=] {
		state->backgroundTransparent = !state->backgroundTransparent.current();
	});
	Ui::AddSkip(box->verticalLayout());

	const auto show = controller->uiShow();
	auto copyText = tr::lng_chat_link_copy(
	) | rpl::start_spawning(box->lifetime());
	auto saveText = tr::lng_settings_save(
	) | rpl::start_spawning(box->lifetime());
	auto cancelText = tr::lng_cancel(
	) | rpl::start_spawning(box->lifetime());
	const auto copy = box->addButton(rpl::duplicate(copyText), [=] {
		auto mime = std::make_unique<QMimeData>();
		mime->setImageData(ExportQrImage(*state));
		QGuiApplication::clipboard()->setMimeData(mime.release());
		show->showToast(tr::lng_group_invite_qr_copied(tr::now));
	});
	const auto save = box->addButton(rpl::duplicate(saveText), [=] {
		FileDialog::GetWritePath(
			QPointer<QWidget>(box.get()),
			tr::lng_save_photo(tr::now),
			u"PNG Image (*.png)"_q,
			u"Telegram QR.png"_q,
			crl::guard(box, [=](QString &&path) {
				if (!path.endsWith(u".png"_q, Qt::CaseInsensitive)) {
					path += u".png"_q;
				}
				ExportQrImage(*state).save(path, "PNG");
			}));
	});
	const auto cancel = box->addButton(rpl::duplicate(cancelText), [=] {
		box->closeBox();
	});
	rpl::combine(
		rpl::duplicate(copyText),
		rpl::duplicate(saveText),
		rpl::duplicate(cancelText)
	) | rpl::on_next([=](
			const QString &copyText,
			const QString &saveText,
			const QString &cancelText) {
		const auto &buttonStyle = copy->st();
		const auto fullWidth = std::max({
			buttonStyle.style.font->width(copyText),
			buttonStyle.style.font->width(saveText),
			buttonStyle.style.font->width(cancelText),
		}) - buttonStyle.width
			+ buttonStyle.padding.left()
			+ buttonStyle.padding.right();
		copy->setFullWidth(fullWidth);
		save->setFullWidth(fullWidth);
		cancel->setFullWidth(fullWidth);
	}, box->lifetime());
	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });
}

void DefaultShowFillPeerQrBoxCallback(
		std::shared_ptr<Show> show,
		PeerData *peer) {
	if (peer && !peer->username().isEmpty()) {
		show->show(Box(FillPeerQrBox, peer, std::nullopt, nullptr));
	}
}

} // namespace Ui
