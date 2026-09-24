/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <array>
#include <optional>

namespace Data {

enum class ProfileDetail {
	Registration,
	PhoneCountry,
	NameChange,
	PhotoChange,
	OldestPhoto,
	Phone,
	Count,
};

struct ProfileObservation {
	QString value;
	qint64 observedAt = 0;

	friend bool operator==(const ProfileObservation &a, const ProfileObservation &b) = default;
};

using ProfileDetailsRecord = std::array<ProfileObservation, static_cast<size_t>(ProfileDetail::Count)>;

[[nodiscard]] QString ProfileDetailKey(ProfileDetail field);
[[nodiscard]] bool ValidProfileObservation(
	ProfileDetail field,
	const ProfileObservation &observation,
	qint64 now);
[[nodiscard]] QJsonObject SerializeProfileDetails(
	const ProfileDetailsRecord &record);
[[nodiscard]] std::optional<ProfileDetailsRecord> ParseProfileDetails(
	const QJsonObject &object,
	qint64 now);
[[nodiscard]] bool MergeProfileDetails(
	ProfileDetailsRecord &record,
	const ProfileDetailsRecord &received);
[[nodiscard]] QString ProfileRegistrationMonth(const QString &telegramMonth);

} // namespace Data
