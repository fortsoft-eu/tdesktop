/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tray_accounts_menu.h"

#include "core/application.h"
#include "main/main_account.h"
#include "main/main_domain.h"

namespace Core::TrayAccountsMenu {

void SetupChangesSubscription(Fn<void()> callback, rpl::lifetime &lifetime) {
	const auto accountSessionsLifetime = lifetime.make_state<rpl::lifetime>();
	const auto watchAccountSessions = [=] {
		accountSessionsLifetime->destroy();
		for (const auto &[index, account] : Core::App().domain().accounts()) {
			account->sessionChanges() | rpl::on_next([=](Main::Session*) {
				callback();
			}, *accountSessionsLifetime);
		}
	};
	Core::App().domain().accountsChanges() | rpl::on_next([=] {
		watchAccountSessions();
		callback();
	}, lifetime);
	watchAccountSessions();
}

void Fill([[maybe_unused]] Platform::Tray &tray) {
}

} // namespace Core::TrayAccountsMenu
