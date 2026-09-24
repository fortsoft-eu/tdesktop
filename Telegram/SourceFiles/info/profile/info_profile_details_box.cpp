/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_details_box.h"

#include "base/unixtime.h"
#include "countries/countries_instance.h"
#include "data/components/profile_details.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "ui/layers/generic_box.h"
#include "ui/style/style_classic.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/labels.h"

#include "styles/style_layers.h"

namespace Info::Profile {
namespace {

struct DetailsState {
	std::unique_ptr<MTP::Sender> api;
	Data::ProfileDetailsRecord current;
	rpl::event_stream<> updates;
	bool loading = true;
	bool failed = false;
};

QString DetailTitle(Data::ProfileDetail field) {
	using Field = Data::ProfileDetail;
	switch (field) {
	case Field::Registration: return tr::lng_new_contact_registration(tr::now);
	case Field::PhoneCountry: return tr::lng_profile_details_country(tr::now);
	case Field::NameChange: return tr::lng_profile_details_name_change(tr::now);
	case Field::PhotoChange: return tr::lng_profile_details_photo_change(tr::now);
	case Field::OldestPhoto: return tr::lng_profile_details_oldest_photo(tr::now);
	case Field::Phone: return tr::lng_new_contact_phone_number(tr::now);
	}
	Unexpected("Invalid profile detail field.");
}

QString DetailValue(Data::ProfileDetail field, const QString &value) {
	using Field = Data::ProfileDetail;
	switch (field) {
	case Field::Registration: {
		const auto date = QDate::fromString(value, u"yyyy-MM"_q);
		return langMonthOfYearFull(date.month(), date.year());
	} break;
	case Field::PhoneCountry: {
		const auto name = Countries::Instance().countryNameByISO2(value);
		return name.isEmpty() ? value : name + u" ("_q + value + ')';
	} break;
	case Field::Phone: return '+' + value;
	case Field::NameChange:
	case Field::PhotoChange:
	case Field::OldestPhoto:
		return langDateTimeFull(base::unixtime::parse(value.toLongLong()));
	}
	Unexpected("Invalid profile detail field.");
}

TextWithEntities DetailsText(not_null<UserData*> user, not_null<DetailsState*> state) {
	using Field = Data::ProfileDetail;
	const auto id = peerToUser(user->id);
	const auto record = user->session().profileDetails().lookup(id);
	auto text = tr::bold(u"ID: "_q).append(QString::number(id.bare));
	const auto fields = {
		Field::Registration,
		Field::Phone,
		Field::PhoneCountry,
		Field::NameChange,
		Field::PhotoChange,
		Field::OldestPhoto,
	};
	for (const auto field : fields) {
		if (field == Field::OldestPhoto && !record[size_t(Field::Registration)].value.isEmpty()) {
			continue;
		}
		const auto &entry = record[size_t(field)];
		text.append(u"\n\n"_q).append(tr::bold(DetailTitle(field) + '\n'));
		if (entry.value.isEmpty()) {
			text.append(tr::lng_profile_details_unavailable(tr::now));
			continue;
		}
		text.append(DetailValue(field, entry.value)).append(u"\n"_q);
		const auto when = langDateTimeFull(base::unixtime::parse(entry.observedAt));
		const auto current = state->current[size_t(field)].value == entry.value;
		text.append(current
			? tr::lng_profile_details_received(tr::now, lt_when, when)
			: tr::lng_profile_details_saved(tr::now, lt_when, when));
	}
	return text;
}

QString StatusText(not_null<UserData*> user, not_null<DetailsState*> state) {
	using Status = Data::ProfileArchiveStatus;
	auto result = state->loading
		? tr::lng_profile_loading(tr::now) + '\n'
		: state->failed
		? tr::lng_profile_details_refresh_failed(tr::now) + '\n'
		: QString();
	switch (user->session().profileDetails().status(peerToUser(user->id))) {
	case Status::NotConfigured:
		return result + tr::lng_profile_details_local_only(tr::now);
	case Status::InvalidConfiguration:
		return result + tr::lng_profile_details_config_invalid(tr::now);
	case Status::Pending:
		return result + tr::lng_profile_details_sync_pending(tr::now);
	case Status::Synced:
		return result + tr::lng_profile_details_sync_done(tr::now);
	case Status::Failed:
		return result + tr::lng_profile_details_sync_failed(tr::now);
	}
	Unexpected("Invalid archive status.");
}

void RequestOldestPhoto(not_null<UserData*> user, not_null<DetailsState*> state, int offset = 0) {
	state->api->request(MTPphotos_GetUserPhotos(user->inputUser(), MTP_int(offset), MTP_long(0), MTP_int(1)))
			.done([=](const MTPphotos_Photos &result) {
		const auto count = result.match([](const MTPDphotos_photos &data) {
			return int(data.vphotos().v.size());
		}, [](const MTPDphotos_photosSlice &data) {
			return data.vcount().v;
		});
		if (!offset && count > 1) {
			RequestOldestPhoto(user, state, count - 1);
			return;
		}
		result.match([&](const auto &data) {
			user->owner().processUsers(data.vusers());
			for (const auto &photo : data.vphotos().v) {
				if (photo.type() == mtpc_photo) {
					const auto value = QString::number(photo.c_photo().vdate().v);
					user->session().profileDetails().observe(peerToUser(user->id), Data::ProfileDetail::OldestPhoto, value);
					state->current[size_t(Data::ProfileDetail::OldestPhoto)] = { value, base::unixtime::now() };
				}
			}
		});
		state->loading = false;
		state->updates.fire({});
	}).fail([=](const MTP::Error &error) {
		state->loading = false;
		state->failed = state->failed || !MTP::IgnoreError(error);
		state->updates.fire({});
	}).send();
}

void RequestDetails(not_null<UserData*> user, not_null<DetailsState*> state) {
	state->api->request(MTPmessages_GetPeerSettings(user->input()))
			.done([=](const MTPmessages_PeerSettings &result) {
		const auto &data = result.data();
		user->owner().processUsers(data.vusers());
		user->owner().processChats(data.vchats());
		user->setBarSettings(data.vsettings());
		using Field = Data::ProfileDetail;
		const auto &settings = data.vsettings().data();
		state->current[size_t(Field::Registration)].value = Data::ProfileRegistrationMonth(qs(settings.vregistration_month().value_or_empty()));
		state->current[size_t(Field::PhoneCountry)].value = qs(settings.vphone_country().value_or_empty()).toUpper();
		state->current[size_t(Field::NameChange)].value = QString::number(settings.vname_change_date().value_or_empty());
		state->current[size_t(Field::PhotoChange)].value = QString::number(settings.vphoto_change_date().value_or_empty());
		state->current[size_t(Field::Phone)].value = user->phone();
		const auto record = user->session().profileDetails().lookup(peerToUser(user->id));
		if (record[size_t(Field::Registration)].value.isEmpty()) {
			RequestOldestPhoto(user, state);
		} else {
			state->loading = false;
		}
		state->updates.fire({});
	}).fail([=](const MTP::Error &error) {
		state->failed = !MTP::IgnoreError(error);
		const auto record = user->session().profileDetails().lookup(
			peerToUser(user->id));
		if (record[size_t(Data::ProfileDetail::Registration)].value.isEmpty()) {
			RequestOldestPhoto(user, state);
		} else {
			state->loading = false;
		}
		state->updates.fire({});
	}).send();
}

} // namespace

void AccountDetailsBox(not_null<Ui::GenericBox*> box, not_null<UserData*> user) {
	Ui::SetClassicSettingsStyle(box);
	box->setTitle(tr::lng_profile_details_title());
	box->setWidth(st::boxWideWidth);
	const auto state = box->lifetime().make_state<DetailsState>();
	state->api = std::make_unique<MTP::Sender>(&user->session().mtp());
	const auto details = box->addRow(object_ptr<Ui::FlatLabel>(box, QString(), st::boxLabel));
	details->setSelectable(true);
	box->addSkip(st::boxLittleSkip);
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_profile_details_about(), st::boxLabel));
	box->addSkip(st::boxLittleSkip);
	const auto status = box->addRow(object_ptr<Ui::FlatLabel>(box, QString(), st::boxLabel));
	const auto update = [=] {
		details->setMarkedText(DetailsText(user, state));
		status->setText(StatusText(user, state));
	};
	rpl::merge(
		Lang::Updated(),
		state->updates.events()
	) | rpl::on_next(update, box->lifetime());
	user->session().profileDetails().changes() | rpl::filter([=](UserId id) {
		return id == peerToUser(user->id);
	}) | rpl::on_next(update, box->lifetime());
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	box->addLeftButton(tr::lng_profile_details_copy_id(), [=] {
		TextUtilities::SetClipboardText({
			QString::number(peerToUser(user->id).bare),
		});
	});
	update();
	user->session().profileDetails().refresh(peerToUser(user->id));
	RequestDetails(user, state);
}

} // namespace Info::Profile
