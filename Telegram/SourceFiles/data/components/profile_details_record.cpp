/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/components/profile_details_record.h"

#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QRegularExpression>

namespace Data {
namespace {

constexpr auto kFirstTelegramDate = qint64(1375315200);
constexpr auto kClockTolerance = qint64(300);

} // namespace

QString ProfileDetailKey(ProfileDetail field) {
	const auto keys = std::array{
		"registration_month",
		"phone_country",
		"name_change_date",
		"photo_change_date",
		"oldest_photo_date",
		"phone",
	};
	return QString::fromLatin1(keys[static_cast<size_t>(field)]);
}

bool ValidProfileObservation(ProfileDetail field, const ProfileObservation &observation, qint64 now) {
	if (observation.observedAt < kFirstTelegramDate || observation.observedAt > now + kClockTolerance) {
		return false;
	}
	const auto &value = observation.value;
	if (field == ProfileDetail::Registration) {
		const auto date = QDate::fromString(value, u"yyyy-MM"_q);
		return date.isValid()
			&& date.year() >= 2013
			&& date.toString(u"yyyy-MM"_q) == value
			&& date <= QDateTime::fromSecsSinceEpoch(now, Qt::UTC).date();
	} else if (field == ProfileDetail::PhoneCountry) {
		static const auto pattern = QRegularExpression(u"\\A[A-Z]{2}\\z"_q);
		return pattern.match(value).hasMatch();
	} else if (field == ProfileDetail::Phone) {
		static const auto pattern = QRegularExpression(u"\\A[0-9]{3,15}\\z"_q);
		return pattern.match(value).hasMatch();
	}
	const auto timestamp = value.toLongLong();
	return QString::number(timestamp) == value
		&& timestamp >= kFirstTelegramDate
		&& timestamp <= observation.observedAt + kClockTolerance;
}

QJsonObject SerializeProfileDetails(const ProfileDetailsRecord &record) {
	auto result = QJsonObject();
	for (auto i = size_t(0); i != record.size(); ++i) {
		const auto &entry = record[i];
		if (!entry.value.isEmpty()) {
			result.insert(ProfileDetailKey(ProfileDetail(i)), QJsonObject{
				{ u"value"_q, entry.value },
				{ u"observed_at"_q, double(entry.observedAt) },
			});
		}
	}
	return result;
}

std::optional<ProfileDetailsRecord> ParseProfileDetails(const QJsonObject &object, qint64 now) {
	auto result = ProfileDetailsRecord();
	for (auto i = object.begin(); i != object.end(); ++i) {
		auto index = size_t(0);
		while (index != result.size() && ProfileDetailKey(ProfileDetail(index)) != i.key()) {
			++index;
		}
		if (index == result.size() || !i.value().isObject()) {
			return std::nullopt;
		}
		const auto entry = i.value().toObject();
		const auto time = entry.value(u"observed_at"_q);
		const auto value = entry.value(u"value"_q);
		if (entry.size() != 2 || !time.isDouble() || !value.isString()) {
			return std::nullopt;
		}
		auto &observation = result[index];
		if (time.toDouble() < kFirstTelegramDate || time.toDouble() > double(now + kClockTolerance)) {
			return std::nullopt;
		}
		observation = { value.toString(), qint64(time.toDouble()) };
		if (time.toDouble() != double(observation.observedAt)
			|| !ValidProfileObservation(ProfileDetail(index), observation, now)) {
			return std::nullopt;
		}
	}
	return result;
}

bool MergeProfileDetails(ProfileDetailsRecord &record, const ProfileDetailsRecord &received) {
	auto changed = false;
	for (auto i = size_t(0); i != record.size(); ++i) {
		auto &current = record[i];
		const auto &next = received[i];
		if (next.value.isEmpty() || current == next) {
			continue;
		}
		const auto oldest = (i == size_t(ProfileDetail::OldestPhoto));
		const auto replace = current.value.isEmpty()
			|| oldest && next.value.toLongLong() < current.value.toLongLong()
			|| (!oldest || next.value == current.value) && next.observedAt >= current.observedAt;
		if (replace) {
			current = next;
			changed = true;
		}
	}
	return changed;
}

QString ProfileRegistrationMonth(const QString &telegramMonth) {
	const auto parts = telegramMonth.split('.');
	if (parts.size() != 2) {
		return {};
	}
	const auto date = QDate(parts[1].toInt(), parts[0].toInt(), 1);
	return date.isValid() ? date.toString(u"yyyy-MM"_q) : QString();
}

} // namespace Data
