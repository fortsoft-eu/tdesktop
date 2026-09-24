/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Info::Profile {

void AccountDetailsBox(not_null<Ui::GenericBox*> box, not_null<UserData*> user);

} // namespace Info::Profile
