/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/media/history_view_save_document_action.h"
#include "history/view/media/history_view_local_copy.h"

#include "base/call_delayed.h"
#include "data/data_document.h"
#include "data/data_file_click_handler.h"
#include "data/data_file_origin.h"
#include "data/data_peer.h"
#include "data/data_saved_music.h"
#include "data/data_session.h"
#include "history/view/history_view_context_menu.h"
#include "history/view/history_view_list_widget.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "ui/toast/toast.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"

#include <QAction>

namespace HistoryView {

void AddSaveDocumentAction(
		const Ui::Menu::MenuCallback &addAction,
		not_null<HistoryItem*> item,
		not_null<DocumentData*> document,
		not_null<Window::SessionController*> controller) {
	const auto contextId = item->fullId();
	const auto fromSaved = item->history()->peer->isSelf();
	const auto savedMusic = &document->owner().savedMusic();
	const auto show = controller->uiShow();
	const auto inProfile = savedMusic->has(document);
	const auto &ripple = st::defaultDropdownMenu.menu.ripple;
	const auto duration = ripple.hideDuration;
	const auto localCopy = ReadLocalMediaCopy(document);
	const auto saveAs = base::fn_delayed(duration, controller, [=] {
		if (localCopy) {
			SaveLocalMediaCopy(*localCopy, &document->session(), show);
			return;
		}
		DocumentSaveClickHandler::SaveAndTrack(
			contextId,
			document,
			DocumentSaveClickHandler::Mode::ToNewFile);
	});
	if (!document->isMusicForProfile() || (fromSaved && inProfile)) {
		const auto text = document->isVideoFile()
			? tr::lng_context_save_video(tr::now)
			: document->isVoiceMessage()
			? tr::lng_context_save_audio(tr::now)
			: document->isAudioFile()
			? tr::lng_context_save_audio_file(tr::now)
			: document->sticker()
			? tr::lng_context_save_image(tr::now)
			: tr::lng_context_save_file(tr::now);
		addAction(text, saveAs, &st::menuIconDownload);
		return;
	}
	const auto fill = [&](not_null<Ui::PopupMenu*> menu) {
		if (!inProfile) {
			const auto saved = [=] {
				savedMusic->save(document, contextId);
				show->showToast({
					.text = { tr::lng_saved_music_added(tr::now) },
					.iconLottie = u"toast/save_to_music"_q,
					.iconLottieSize = st::toastLottieIconSize,
				});
			};
			menu->addAction(
				tr::lng_context_save_music_profile(tr::now),
				saved,
				&st::menuIconProfile);
		}
		if (!fromSaved) {
			menu->addAction(
				tr::lng_context_save_music_saved(tr::now),
				[=] { Window::ForwardToSelf(show, { { contextId } }); },
				&st::menuIconSavedMessages);
		}
		menu->addAction(
			tr::lng_context_save_music_folder(tr::now),
			saveAs,
			&st::menuIconDownload);

		menu->addSeparator(&st::expandedMenuSeparator);

		const auto action = new QAction(tr::lng_context_save_music_about(tr::now), menu->menu());
		action->setEnabled(false);
		menu->addAction(base::make_unique_q<Ui::Menu::Action>(menu->menu(), st::saveMusicInfoMenu, action, nullptr, nullptr));
	};
	addAction(Ui::Menu::MenuCallback::Args{
		.text = tr::lng_context_save_music_to(tr::now),
		.handler = nullptr,
		.icon = &st::menuIconSoundAdd,
		.fillSubmenu = fill,
		.submenuSt = &st::popupMenuWithIcons,
	});
}

void AddSaveDocumentAction(
		not_null<Ui::PopupMenu*> menu,
		HistoryItem *item,
		not_null<DocumentData*> document,
		not_null<ListWidget*> list) {
	if (!item || list->hasCopyMediaRestriction(item) || ItemHasTtl(item)) {
		if (const auto copy = ReadLocalMediaCopy(document)) {
			const auto show = list->controller()->uiShow();
			menu->addAction(tr::lng_context_save_file(tr::now), [=] {
				SaveLocalMediaCopy(*copy, &document->session(), show);
			}, &st::menuIconDownload);
		}
		return;
	}
	AddSaveDocumentAction(
		Ui::Menu::CreateAddActionCallback(menu),
		item,
		document,
		list->controller());
}

} // namespace HistoryView
