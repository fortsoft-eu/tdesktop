/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Core {
class Settings;

void ApplyPersonalExperimentalDefaults(const QString &directory);
void SetPersonalLocalDefaults(Settings &settings);
void ApplyPersonalLocalDefaults();
void ApplyPersonalChatBackgroundDefault();
void ApplyPersonalAccountDefaults(not_null<Main::Session*> session);

} // namespace Core
