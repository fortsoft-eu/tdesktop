/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/personal_defaults.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_auto_download.h"
#include "data/data_wall_paper.h"
#include "dialogs/ui/dialogs_quick_action.h"
#include "history/view/history_view_quick_action.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "storage/localstorage.h"
#include "ui/power_saving.h"
#include "ui/widgets/fields/input_field.h"
#include "window/themes/window_theme.h"
#include "window/window_saved_windows.h"

namespace Core {

void SetPersonalLocalDefaults(Settings &settings) {
	settings.setNotifyFromAll(false);
	settings.setDesktopNotify(false);
	settings.setSoundNotify(false);
	settings.setFlashBounceNotify(true);
	settings.setNotifyAboutPinned(true);
	settings.setIncludeMutedCounter(true);
	settings.setIncludeMutedCounterFolders(true);
	settings.setCountUnreadMessages(true);
	settings.setNativeNotifications(false);
	settings.setSkipToastsInFocus(false);
	settings.setAdaptiveForWide(true);
	settings.setQuickDialogAction(Dialogs::Ui::QuickDialogAction::Disabled);
	settings.setLargeEmoji(false);
	settings.setReplaceEmoji(true);
	settings.setSuggestEmoji(true);
	settings.setSuggestAnimatedEmoji(true);
	settings.setSuggestStickersByEmoji(false);
	settings.setLoopAnimatedStickers(true);
	settings.setSendSubmitWay(Ui::InputSubmitSettings::CtrlEnter);
	settings.setChatQuickAction(HistoryView::DoubleClickQuickAction::Reply);
	settings.setCornerReply(true);
	settings.setCornerReaction(false);
	settings.setPullToNextChannel(false);
	settings.setAskDownloadPath(false);
	settings.setDownloadPath(QString());
	settings.setDownloadPathBookmark(QByteArray());
	settings.setWindowTitleContent({
		.hideChatName = true,
		.hideAccountName = true,
		.hideTotalUnread = false,
	});
	settings.setNativeWindowFrame(true);
	settings.setWorkMode(Settings::WorkMode::WindowAndTray);
	settings.setTrayIconMonochrome(false);
	settings.setHardwareAcceleratedVideo(true);
	settings.setSpellcheckerEnabled(true);
}

void ApplyPersonalLocalDefaults() {
	constexpr auto replaceEmojiKey = "personal-replace-emoji-v1";
	constexpr auto key = "personal-local-defaults-screenshots-313-327-v1";
	auto &settings = App().settings();
	if (!settings.readPref<bool>(replaceEmojiKey)) {
		settings.setReplaceEmoji(true);
		settings.writePref<bool>(replaceEmojiKey, true);
		Local::writeSettings();
	}
	if (settings.readPref<bool>(key)) {
		return;
	}
	SetPersonalLocalDefaults(settings);
	Window::Theme::Background()->setTile(false);
	App().savedWindows()->setRestoreOnLaunch(false);
	PowerSaving::Set(PowerSaving::kAll & ~PowerSaving::Flags(PowerSaving::kStickersPanel | PowerSaving::kStickersChat));
	cSetAutoUpdate(false);
	cSetAutoStart(false);
	cSetSendToMenu(false);
	settings.writePref<bool>(key, true);
	Local::writeSettings();
}

void ApplyPersonalChatBackgroundDefault() {
	constexpr auto key = "personal-windows-chat-background-v4";
	auto &settings = App().settings();
	if (settings.readPref<bool>(key)) {
		return;
	}
	const auto background = Window::Theme::Background();
	const auto &paper = background->paper();
	if (Data::IsThemeWallPaper(paper)
		|| Data::IsDefaultWallPaper(paper)
		|| Data::IsLegacy1DefaultWallPaper(paper)
		|| Data::IsLegacy2DefaultWallPaper(paper)
		|| Data::IsLegacy3DefaultWallPaper(paper)
		|| Data::IsLegacy4DefaultWallPaper(paper)) {
		const auto workspace = Data::WallPaper::FromColorsSlug(u"808080"_q);
		Assert(workspace.has_value());
		background->set(*workspace);
		background->setTile(false);
	}
	settings.writePref<bool>(key, true);
	Local::writeSettings();
}

} // namespace Core
