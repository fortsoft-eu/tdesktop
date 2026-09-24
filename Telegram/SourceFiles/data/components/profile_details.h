/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "data/components/profile_details_record.h"
#include "data/data_peer_id.h"
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <rpl/event_stream.h>
#include <deque>
#include <set>

class QNetworkAccessManager;
class QNetworkReply;

namespace Main {
class Session;
} // namespace Main

namespace Data {

enum class ProfileArchiveStatus {
	NotConfigured,
	InvalidConfiguration,
	Pending,
	Synced,
	Failed,
};

class ProfileDetails final : public QObject {
public:
	explicit ProfileDetails(not_null<Main::Session*> session);
	~ProfileDetails();

	[[nodiscard]] ProfileDetailsRecord lookup(UserId userId) const;
	void observe(UserId userId, ProfileDetail field, const QString &value);
	void observeSettings(UserId userId, const MTPDpeerSettings &settings);
	void refresh(UserId userId);
	[[nodiscard]] ProfileArchiveStatus status(UserId userId) const;
	[[nodiscard]] rpl::producer<UserId> changes() const;
	[[nodiscard]] static QString ConfigurationPath();

private:
	void save(UserId userId, const ProfileDetailsRecord &record);
	void savePending();
	void enqueue(UserId userId);
	void pump();
	void reloadConfiguration();
	void finish(UserId userId, not_null<QNetworkReply*> reply, const ProfileDetailsRecord &sent);

	const not_null<Main::Session*> _session;
	std::unique_ptr<QNetworkAccessManager> _network;
	QPointer<QNetworkReply> _reply;
	QByteArray _response;
	QUrl _endpoint;
	std::set<UserId> _pending;
	std::deque<UserId> _queue;
	base::flat_map<UserId, ProfileArchiveStatus> _statuses;
	ProfileArchiveStatus _configurationStatus = ProfileArchiveStatus::NotConfigured;
	crl::time _retryNotBefore = 0;
	rpl::event_stream<UserId> _changes;
	base::Timer _retryTimer;

};

} // namespace Data
