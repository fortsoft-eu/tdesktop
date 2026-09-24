/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/personal_defaults.h"

#include "api/api_authorizations.h"
#include "api/api_global_privacy.h"
#include "api/api_reactions_notify_settings.h"
#include "api/api_self_destruct.h"
#include "api/api_sensitive_content.h"
#include "api/api_user_privacy.h"
#include "apiwrap.h"
#include "base/unixtime.h"
#include "base/weak_ptr.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/components/top_peers.h"
#include "data/data_auto_download.h"
#include "data/notify/data_notify_settings.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "storage/localstorage.h"

namespace Core {
namespace {

constexpr auto kSessionsTtlId = "sessions-ttl";
constexpr auto kFreshAuthorizationError = "FRESH_RESET_AUTHORISATION_FORBIDDEN";
constexpr auto kRetryAfterPrefix = "retry-after:";
constexpr auto kFreshAuthorizationRetryDelay = 24 * 60 * 60;

class AccountDefaults final : public base::has_weak_ptr {
public:
	explicit AccountDefaults(not_null<Main::Session*> session);
	void start();

private:
	bool pending(const std::string &id);
	void finish(const std::string &id, const QString &error = QString());
	void privacy(const std::string &id, Api::UserPrivacy::Key key, MTPInputPrivacyKey input, Api::UserPrivacy::Option option);

	template <typename Request, typename Callback>
	void send(const std::string &id, Request request, Callback callback) {
		_api.request(std::move(request)).done([=](const typename Request::ResponseType &result) {
			if constexpr (std::is_same_v<typename Request::ResponseType, MTPBool>) {
				if (!mtpIsTrue(result)) {
					finish(id, u"SERVER_RETURNED_FALSE"_q);
					return;
				}
			}
			callback(result);
		}).fail([=](const MTP::Error &error) {
			finish(id, error.type());
		}).handleFloodErrors().send();
	}

	template <typename Request>
	void set(const std::string &id, Request request) {
		if (pending(id)) {
			send(id, std::move(request), [=](const auto &) { finish(id); });
		}
	}

	const not_null<Main::Session*> _session;
	const std::string _prefix;
	MTP::Sender _api;

};

AccountDefaults::AccountDefaults(not_null<Main::Session*> session)
: _session(session)
, _prefix("personal-account-defaults-v1/"
	+ std::to_string(session->uniqueId()) + "/")
, _api(&session->mtp()) {
}

bool AccountDefaults::pending(const std::string &id) {
	auto status = App().settings().readPref<QByteArray>(_prefix + id);
	if (id == kSessionsTtlId) {
		const auto retryAfterPrefix = QByteArray(kRetryAfterPrefix);
		const auto oldFreshRetry = QByteArray("retry:") + kFreshAuthorizationError;
		if (status.isEmpty() || status == oldFreshRetry) {
			status = retryAfterPrefix + QByteArray::number(base::unixtime::now() + kFreshAuthorizationRetryDelay);
			App().settings().writePref<QByteArray>(_prefix + id, status);
			Local::writeSettings();
			return false;
		} else if (status.startsWith(retryAfterPrefix)) {
			return status.mid(retryAfterPrefix.size()).toLongLong() <= base::unixtime::now();
		}
	}
	return status.isEmpty() || status.startsWith("retry:");
}

void AccountDefaults::finish(const std::string &id, const QString &error) {
	const auto freshAuthorization = (id == kSessionsTtlId)
		&& (error == QString::fromLatin1(kFreshAuthorizationError));
	const auto permanent = error.contains(u"PREMIUM"_q)
		|| error.endsWith(u"_INVALID"_q)
		|| error.endsWith(u"_NOT_ALLOWED"_q)
		|| error == u"NOT_AVAILABLE"_q;
	const auto status = error.isEmpty()
		? u"applied"_q
		: freshAuthorization
		? u"%1%2"_q.arg(
			QString::fromLatin1(kRetryAfterPrefix),
			QString::number(base::unixtime::now() + kFreshAuthorizationRetryDelay))
		: (permanent ? u"unavailable:"_q : u"retry:"_q) + error;
	App().settings().writePref<QByteArray>(_prefix + id, status.toUtf8());
	Local::writeSettings();
	LOG(("Personal defaults: %1: %2").arg(QString::fromStdString(id), status));
	if (error.isEmpty()) {
		if (id == "global-privacy") {
			_session->api().globalPrivacy().reload();
		} else if (id == "sensitive-content") {
			_session->api().sensitiveContent().reload(true);
		} else if (id == "reaction-notifications") {
			_session->api().reactionsNotifySettings().reload();
		} else if (id == "account-ttl") {
			_session->api().selfDestruct().reload();
		} else if (id == "sessions-ttl" || id == "calls-here") {
			_session->api().authorizations().reload();
		}
	}
}

void AccountDefaults::privacy(
		const std::string &id,
		Api::UserPrivacy::Key key,
		MTPInputPrivacyKey input,
		Api::UserPrivacy::Option option) {
	if (!pending(id)) {
		return;
	}
	if (key == Api::UserPrivacy::Key::Voices && !_session->premium()) {
		finish(id, u"PREMIUM_ACCOUNT_REQUIRED"_q);
		return;
	}
	const auto type = input.type();
	send(id, MTPaccount_GetPrivacy(input), [=](const MTPaccount_PrivacyRules &result) {
		const auto &data = result.data();
		_session->data().processUsers(data.vusers());
		_session->data().processChats(data.vchats());
		auto &privacy = _session->api().userPrivacy();
		privacy.apply(type, data.vrules(), true);
		privacy.value(key) | rpl::take(1) | rpl::on_next([=](Api::UserPrivacy::Rule rule) {
			rule.option = option;
			_session->api().userPrivacy().save(key, rule, crl::guard(this, [=](const QString &error) { finish(id, error); }));
		}, _session->lifetime());
	});
}

void AccountDefaults::start() {
	using Key = Api::UserPrivacy::Key;
	using Option = Api::UserPrivacy::Option;
	privacy("phone", Key::PhoneNumber, MTP_inputPrivacyKeyPhoneNumber(), Option::Nobody);
	privacy("last-seen", Key::LastSeen, MTP_inputPrivacyKeyStatusTimestamp(), Option::Everyone);
	privacy("photo", Key::ProfilePhoto, MTP_inputPrivacyKeyProfilePhoto(), Option::Everyone);
	privacy("forwards", Key::Forwards, MTP_inputPrivacyKeyForwards(), Option::Nobody);
	privacy("calls", Key::Calls, MTP_inputPrivacyKeyPhoneCall(), Option::Nobody);
	privacy("voice", Key::Voices, MTP_inputPrivacyKeyVoiceMessages(), Option::Nobody);
	privacy("birthday", Key::Birthday, MTP_inputPrivacyKeyBirthday(), Option::Nobody);
	privacy("gifts", Key::GiftsAutoSave, MTP_inputPrivacyKeyStarGiftsAutoSave(), Option::Nobody);
	privacy("bio", Key::About, MTP_inputPrivacyKeyAbout(), Option::Everyone);
	privacy("music", Key::SavedMusic, MTP_inputPrivacyKeySavedMusic(), Option::Nobody);
	privacy("invites", Key::Invites, MTP_inputPrivacyKeyChatInvite(), Option::Nobody);

	set("account-ttl", MTPaccount_SetAccountTTL(MTP_accountDaysTTL(MTP_int(720))));
	set(kSessionsTtlId, MTPaccount_SetAuthorizationTTL(MTP_int(365)));
	set("calls-here", MTPaccount_ChangeAuthorizationSettings(
		MTP_flags(MTPaccount_ChangeAuthorizationSettings::Flag::f_call_requests_disabled),
		MTP_long(0), MTPBool(), MTP_bool(true)));
	set("contacts-joined", MTPaccount_SetContactSignUpNotification(MTP_bool(false)));
	if (pending("frequent-contacts")) {
		send("frequent-contacts", MTPcontacts_ToggleTopPeers(MTP_bool(false)), [=](const MTPBool &) {
			finish("frequent-contacts");
			_session->topPeers().reload();
		});
	}
	if (pending("global-privacy")) {
		send("global-privacy", MTPaccount_GetGlobalPrivacySettings(), [=](const MTPGlobalPrivacySettings &result) {
			const auto &data = result.data();
			using Flag = MTPDglobalPrivacySettings::Flag;
			const auto flags = (data.vflags().v
				& ~(Flag::f_archive_and_mute_new_noncontact_peers
					| Flag::f_new_noncontact_peers_require_premium))
				| Flag::f_noncontact_peers_paid_stars;
			const auto disallowed = data.vdisallowed_gifts();
			set("global-privacy", MTPaccount_SetGlobalPrivacySettings(
				MTP_globalPrivacySettings(MTP_flags(flags), MTP_long(0),
					disallowed ? *disallowed : MTPDisallowedGiftsSettings())));
		});
	}
	if (pending("sensitive-content")) {
		send("sensitive-content", MTPaccount_GetContentSettings(), [=](const MTPaccount_ContentSettings &result) {
			if (result.data().is_sensitive_enabled()) {
				finish("sensitive-content");
			} else if (result.data().is_sensitive_can_change()) {
				set("sensitive-content", MTPaccount_SetContentSettings(
					MTP_flags(MTPaccount_SetContentSettings::Flag::f_sensitive_enabled)));
			} else {
				finish("sensitive-content", u"NOT_AVAILABLE"_q);
			}
		});
	}
	if (pending("reaction-notifications")) {
		send("reaction-notifications", MTPaccount_GetReactionsNotifySettings(), [=](const MTPReactionsNotifySettings &result) {
			const auto &data = result.data();
			using Flag = MTPDreactionsNotifySettings::Flag;
			const auto messages = data.vmessages_notify_from();
			const auto stories = data.vstories_notify_from();
			const auto polls = data.vpoll_votes_notify_from();
			set("reaction-notifications", MTPaccount_SetReactionsNotifySettings(
				MTP_reactionsNotifySettings(
					MTP_flags(data.vflags().v | Flag::f_messages_notify_from | Flag::f_poll_votes_notify_from),
					messages ? *messages : MTPReactionNotificationsFrom(MTP_reactionNotificationsFromAll()),
					stories ? *stories : MTPReactionNotificationsFrom(),
					polls ? *polls : MTPReactionNotificationsFrom(MTP_reactionNotificationsFromAll()),
					data.vsound(),
					data.vshow_previews())));
		});
	}
	const auto notifications = [&](const std::string &id, MTPInputNotifyPeer peer, bool muted) {
		if (!pending(id)) {
			return;
		}
		send(id, MTPaccount_UpdateNotifySettings(peer, MTP_inputPeerNotifySettings(
			MTP_flags(MTPDinputPeerNotifySettings::Flag::f_mute_until),
			MTPBool(), MTPBool(), MTP_int(muted ? 0x7FFFFFFF : 0),
			MTPNotificationSound(), MTPBool(), MTPBool(), MTPNotificationSound())),
			[=](const MTPBool &) {
				send(id, MTPaccount_GetNotifySettings(peer), [=](const MTPPeerNotifySettings &result) {
					_session->data().notifySettings().apply(peer, result);
					finish(id);
				});
			});
	};
	notifications("notify-private", MTP_inputNotifyUsers(), false);
	notifications("notify-groups", MTP_inputNotifyChats(), true);
	notifications("notify-channels", MTP_inputNotifyBroadcasts(), true);

	if (pending("downloads")) {
		using namespace Data::AutoDownload;
		auto &settings = _session->settings().autoDownload();
		for (const auto source : { Source::User, Source::Group, Source::Channel }) {
			for (const auto type : { Type::Photo, Type::File, Type::AutoPlayVideo,
					Type::AutoPlayVideoMessage, Type::AutoPlayGIF }) {
				settings.setBytesLimit(source, type, kMaxBytesLimit);
			}
		}
		_session->saveSettings();
		finish("downloads");
	}
}

} // namespace

void ApplyPersonalAccountDefaults(not_null<Main::Session*> session) {
	session->lifetime().make_state<AccountDefaults>(session)->start();
}

} // namespace Core
