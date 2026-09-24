/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/personal_defaults.h"

#include "base/options.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>

namespace Core {

void ApplyPersonalExperimentalDefaults(const QString &directory) {
	const auto marker = directory + u"personal-experimental-defaults-v1"_q;
	if (QFile::exists(marker)) {
		return;
	}
	auto existing = QFile(directory + u"experimental_options.json"_q);
	if (existing.open(QIODevice::ReadOnly)
		&& !base::options::deserialize(QString::fromUtf8(existing.readAll()))) {
		LOG(("Personal defaults: Invalid existing experimental settings."));
	}
	const auto defaults = std::initializer_list<std::pair<const char*, bool>>{
		{ "hide-ai-button", true },
		{ "force-compose-search-one-column", false },
		{ "view-profile-in-chats-list-context-menu", true },
		{ "show-peer-id-below-about", true },
		{ "show-channel-joined-below-about", true },
		{ "profile-media-tabs", false },
		{ "profile-media-tabs-expanded", false },
		{ "tabbed-panel-show-on-click", true },
		{ "unlimited-recent-stickers", false },
		{ "disable-autoplay-next", true },
		{ "external-media-viewer", false },
		{ "hide-reply-button", false },
		{ "custom-notification", false },
		{ "gnotification", false },
		{ "mac-modern-notifications", false },
		{ "fractional-scaling-enabled", true },
		{ "high-dpi-downscale", false },
		{ "use-qt-rhi", true },
		{ "enable-vulkan-rhi", false },
		{ "freetype", true },
		{ "qscroller", false },
		{ "touchbar-disabled", false },
		{ "new-windows-size-as-first", false },
		{ "prefer-ipv6", false },
		{ "skip-url-scheme-register", false },
		{ "deadlock-detector", false },
		{ "webview-debug-enabled", false },
		{ "webview-legacy-edge", false },
		{ "ffmpeg-multithread", true },
	};
	for (const auto &[id, value] : defaults) {
		base::options::lookup<bool>(id).set(value);
	}
	QDir().mkpath(directory);
	auto file = QSaveFile(directory + u"experimental_options.json"_q);
	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}
	const auto data = base::options::serialize().toUtf8();
	if (file.write(data) != data.size() || !file.commit()) {
		return;
	}
	auto completed = QSaveFile(marker);
	if (completed.open(QIODevice::WriteOnly)) {
		if (completed.write("1") == 1) {
			completed.commit();
		}
	}
}

} // namespace Core
