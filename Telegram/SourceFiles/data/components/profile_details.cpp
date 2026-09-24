/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/components/profile_details.h"

#include "base/unixtime.h"
#include "main/main_session.h"
#include "storage/storage_account.h"
#include "settings.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

namespace Data {
namespace {

constexpr auto kRetryInterval = crl::time(60000);
constexpr auto kRequestTimeout = 15000;
constexpr auto kMaxResponseSize = 65536;
constexpr auto kPendingKey = "profile-details-pending-v1";

std::string RecordKey(UserId userId) {
	return "profile-details-v1/" + std::to_string(userId.bare);
}

} // namespace

ProfileDetails::ProfileDetails(not_null<Main::Session*> session)
: _session(session)
, _retryTimer([=] {
	reloadConfiguration();
	for (const auto userId : _pending) {
		enqueue(userId);
	}
	pump();
}) {
	const auto bytes = _session->local().readPref<QByteArray>(kPendingKey);
	const auto pending = QJsonDocument::fromJson(bytes).array();
	for (const auto &entry : pending) {
		const auto id = entry.toString().toULongLong();
		if (id && id <= uint64(std::numeric_limits<int64>::max())) {
			_pending.emplace(UserId(id));
		}
	}
	_retryTimer.callEach(kRetryInterval);
	QTimer::singleShot(0, this, [=] {
		reloadConfiguration();
		for (const auto userId : _pending) {
			enqueue(userId);
		}
		pump();
	});
}

ProfileDetails::~ProfileDetails() {
	if (_reply) {
		_reply->disconnect(this);
		_reply->abort();
	}
}

QString ProfileDetails::ConfigurationPath() {
	const auto filename = u"profile-details-server.local.json"_q;
	const auto executable = QDir(cExeDir());
	const auto project = QDir(executable.absoluteFilePath(u"../.."_q));
	return project.exists(u"CMakeLists.txt"_q) && project.exists(u"Telegram/SourceFiles"_q)
		? project.absoluteFilePath(filename)
		: executable.absoluteFilePath(filename);
}

void ProfileDetails::reloadConfiguration() {
	_endpoint = QUrl();
	_configurationStatus = ProfileArchiveStatus::NotConfigured;
	auto file = QFile(ConfigurationPath());
	if (!file.exists()) {
		return;
	}
	_configurationStatus = ProfileArchiveStatus::InvalidConfiguration;
	if (!file.open(QIODevice::ReadOnly) || file.size() > 8192) {
		return;
	}
	const auto document = QJsonDocument::fromJson(file.readAll());
	if (!document.isObject()) {
		return;
	}
	const auto config = document.object();
	if (config.value(u"enabled"_q).isBool() && !config.value(u"enabled"_q).toBool()) {
		_configurationStatus = ProfileArchiveStatus::NotConfigured;
		return;
	}
	if (!config.value(u"enabled"_q).toBool()) {
		return;
	}
	const auto endpoint = QUrl(config.value(u"endpoint"_q).toString());
	if (!endpoint.isValid()
		|| endpoint.scheme() != u"https"_q
		|| endpoint.host().isEmpty()
		|| !endpoint.userInfo().isEmpty()
		|| endpoint.hasFragment()
		|| endpoint.hasQuery()) {
		return;
	}
	_endpoint = endpoint;
	_configurationStatus = ProfileArchiveStatus::Pending;
}

ProfileDetailsRecord ProfileDetails::lookup(UserId userId) const {
	const auto bytes = _session->local().readPref<QByteArray>(RecordKey(userId));
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject()) {
		return {};
	}
	return ParseProfileDetails(
		document.object(),
		base::unixtime::now()).value_or(ProfileDetailsRecord());
}

void ProfileDetails::save(UserId userId, const ProfileDetailsRecord &record) {
	_session->local().writePref<QByteArray>(
		RecordKey(userId),
		QJsonDocument(SerializeProfileDetails(record)).toJson(QJsonDocument::Compact));
}

void ProfileDetails::savePending() {
	auto array = QJsonArray();
	for (const auto userId : _pending) {
		array.push_back(QString::number(userId.bare));
	}
	_session->local().writePref<QByteArray>(
		kPendingKey,
		QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void ProfileDetails::observe(UserId userId, ProfileDetail field, const QString &value) {
	const auto now = base::unixtime::now();
	const auto observation = ProfileObservation{ value, now };
	if (!ValidProfileObservation(field, observation, now)) {
		return;
	}
	auto record = lookup(userId);
	auto received = ProfileDetailsRecord();
	received[size_t(field)] = observation;
	if (!MergeProfileDetails(record, received)) {
		return;
	}
	save(userId, record);
	_pending.emplace(userId);
	savePending();
	_statuses[userId] = ProfileArchiveStatus::Pending;
	_changes.fire_copy(userId);
	enqueue(userId);
	QTimer::singleShot(0, this, [=] { pump(); });
}

void ProfileDetails::observeSettings(UserId userId, const MTPDpeerSettings &settings) {
	observe(userId, ProfileDetail::Registration, ProfileRegistrationMonth(
		qs(settings.vregistration_month().value_or_empty())));
	observe(userId, ProfileDetail::PhoneCountry,
		qs(settings.vphone_country().value_or_empty()).toUpper());
	observe(userId, ProfileDetail::NameChange,
		QString::number(settings.vname_change_date().value_or_empty()));
	observe(userId, ProfileDetail::PhotoChange,
		QString::number(settings.vphoto_change_date().value_or_empty()));
}

void ProfileDetails::enqueue(UserId userId) {
	if (!ranges::contains(_queue, userId)) {
		_queue.push_back(userId);
	}
}

void ProfileDetails::refresh(UserId userId) {
	reloadConfiguration();
	_retryNotBefore = 0;
	_statuses[userId] = ProfileArchiveStatus::Pending;
	enqueue(userId);
	pump();
	_changes.fire_copy(userId);
}

ProfileArchiveStatus ProfileDetails::status(UserId userId) const {
	if (_endpoint.isEmpty()) {
		return _configurationStatus;
	}
	const auto i = _statuses.find(userId);
	return (i != end(_statuses)) ? i->second : ProfileArchiveStatus::Pending;
}

rpl::producer<UserId> ProfileDetails::changes() const {
	return _changes.events();
}

void ProfileDetails::pump() {
	if (_reply || _endpoint.isEmpty() || _queue.empty() || crl::now() < _retryNotBefore) {
		return;
	}
	const auto userId = _queue.front();
	_queue.pop_front();
	if (!_network) {
		_network = std::make_unique<QNetworkAccessManager>();
		_network->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
	}
	const auto sent = _pending.contains(userId)
		? lookup(userId)
		: ProfileDetailsRecord();
	const auto payload = QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"user_id"_q, QString::number(userId.bare) },
		{ u"fields"_q, SerializeProfileDetails(sent) },
	}).toJson(QJsonDocument::Compact);
	auto request = QNetworkRequest(_endpoint);
	request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);
	request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
		QNetworkRequest::Manual);
	request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
		QNetworkRequest::Manual);
	request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,
		QNetworkRequest::Manual);
	request.setTransferTimeout(kRequestTimeout);
	_response.clear();
	const auto reply = _network->post(request, payload);
	_reply = reply;
	QTimer::singleShot(kRequestTimeout, reply, [=] {
		if (!reply->isFinished()) {
			reply->abort();
		}
	});
	reply->setReadBufferSize(kMaxResponseSize + 1);
	connect(reply, &QNetworkReply::readyRead, this, [=] {
		_response += reply->read(kMaxResponseSize + 1 - _response.size());
		if (_response.size() > kMaxResponseSize) {
			reply->abort();
		}
	});
	connect(reply, &QNetworkReply::finished, this, [=] {
		finish(userId, reply, sent);
	});
}

void ProfileDetails::finish(UserId userId, not_null<QNetworkReply*> reply, const ProfileDetailsRecord &sent) {
	const auto http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const auto object = QJsonDocument::fromJson(_response).object();
	const auto valid = reply->error() == QNetworkReply::NoError
		&& http == 200
		&& object.value(u"version"_q).toInt() == 1
		&& object.value(u"user_id"_q).toString() == QString::number(userId.bare)
		&& object.value(u"fields"_q).isObject();
	auto received = valid ? ParseProfileDetails(
		object.value(u"fields"_q).toObject(),
		base::unixtime::now()) : std::nullopt;
	if (received) {
		auto acknowledged = *received;
		if (MergeProfileDetails(acknowledged, sent)) {
			received = std::nullopt;
		}
	}
	_reply = nullptr;
	reply->deleteLater();
	if (!received) {
		_retryNotBefore = crl::now() + kRetryInterval;
		_statuses[userId] = ProfileArchiveStatus::Failed;
		_changes.fire_copy(userId);
		_queue.clear();
		return;
	}
	auto record = lookup(userId);
	if (record == sent && _pending.erase(userId)) {
		savePending();
	}
	if (_pending.contains(userId)) {
		for (auto i = size_t(0); i != record.size(); ++i) {
			if (record[i] != sent[i]) {
				(*received)[i] = {};
			}
		}
	}
	if (MergeProfileDetails(record, *received)) {
		save(userId, record);
	}
	_statuses[userId] = _pending.contains(userId)
		? ProfileArchiveStatus::Pending
		: ProfileArchiveStatus::Synced;
	_changes.fire_copy(userId);
	QTimer::singleShot(0, this, [=] { pump(); });
}

} // namespace Data
