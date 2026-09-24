/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/boxes/choose_date_time.h"

#include "base/unixtime.h"
#include "ui/boxes/calendar_box.h"
#include "ui/painter.h"
#include "ui/style/style_classic.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/shadow.h"
#include "ui/ui_utility.h"
#include "lang/lang_keys.h"
#include "styles/style_choose_date_time.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_passcode_box.h"

#include <QtWidgets/QTextEdit>

namespace Ui {
namespace {

constexpr auto kMinimalSchedule = TimeId(10);

QString DayString(const QDate &date) {
	const auto month = Lang::MonthDay(date.month())(tr::now);
	const auto day = QString::number(date.day());
	if (date.year() != QDate::currentDate().year()) {
		return tr::lng_month_day_year(
			tr::now,
			lt_month,
			month,
			lt_day,
			day,
			lt_year,
			QString::number(date.year()));
	}
	return tr::lng_month_day(tr::now, lt_month, month, lt_day, day);
}

QString TimeString(QTime time) {
	return QString("%1:%2"
	).arg(time.hour(), 2, 10, QLatin1Char('0')
	).arg(time.minute(), 2, 10, QLatin1Char('0'));
}

[[nodiscard]] std::optional<QTime> ParseTime(QStringView text) {
	text = text.trimmed();
	if (text.isEmpty() || text.front() == '+' || text.front() == '-') {
		return std::nullopt;
	}
	auto groups = std::vector<QString>();
	auto current = QString();
	for (const auto character : text) {
		if (character >= '0' && character <= '9') {
			current.append(character);
		} else if (character.isLetter() || character == '_') {
			return std::nullopt;
		} else if (!current.isEmpty()) {
			groups.push_back(base::take(current));
			if (groups.size() > 2) {
				return std::nullopt;
			}
		}
	}
	if (!current.isEmpty()) {
		groups.push_back(std::move(current));
	}
	if (groups.empty() || groups.size() > 2) {
		return std::nullopt;
	}
	if (groups.size() == 1) {
		const auto size = groups.front().size();
		if (size < 1 || size > 4) {
			return std::nullopt;
		}
		if (size > 2) {
			const auto compact = std::move(groups.front());
			groups = {
				compact.left(size - 2),
				compact.right(2),
			};
		}
	}
	auto ok = false;
	const auto hour = groups[0].toInt(&ok);
	if (!ok) {
		return std::nullopt;
	}
	const auto minute = (groups.size() == 2)
		? groups[1].toInt(&ok)
		: 0;
	if (!ok) {
		return std::nullopt;
	}
	const auto result = QTime(hour, minute);
	return result.isValid() ? std::optional(result) : std::nullopt;
}

class RepeatButton final : public Ui::AbstractButton {
public:
	explicit RepeatButton(not_null<QWidget*> parent);

	void setMarkedText(TextWithEntities text);

protected:
	void paintEvent(QPaintEvent *e) override;
	void onStateChanged(State, StateChangeSource) override;

private:
	void updateLabelPosition();

	const not_null<Ui::FlatLabel*> _label;

};

RepeatButton::RepeatButton(not_null<QWidget*> parent)
: AbstractButton(parent)
, _label(Ui::CreateChild<Ui::FlatLabel>(this, st::scheduleRepeatLabel)) {
	_label->setAttribute(Qt::WA_TransparentForMouseEvents);
	setPointerCursor(false);

	_label->naturalWidthValue() | rpl::on_next([=](int natural) {
		_label->resizeToWidth(natural);
	}, lifetime());

	_label->sizeValue() | rpl::on_next([=](QSize size) {
		const auto padding = st::scheduleRepeatTextPadding;
		const auto height = st::scheduleRepeatHeight;
		resize(size.width() + 2 * padding, height);
		updateLabelPosition();
	}, lifetime());
}

void RepeatButton::setMarkedText(TextWithEntities text) {
	_label->setMarkedText(std::move(text));
}

void RepeatButton::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	Ui::PaintClassicButton(p, rect(), this, isDown());
}

void RepeatButton::onStateChanged(State, StateChangeSource) {
	updateLabelPosition();
	update();
}

void RepeatButton::updateLabelPosition() {
	const auto offset = Ui::ClassicButtonContentOffset(this, isDown());
	const auto padding = st::scheduleRepeatTextPadding;
	_label->moveToLeft(
		padding + offset.x(),
		(st::scheduleRepeatHeight - _label->height()) / 2 + offset.y());
}

} // namespace

ChooseDateTimeStyleArgs::ChooseDateTimeStyleArgs()
: labelStyle(&st::scheduleDescriptionLabel)
, dateFieldStyle(&st::scheduleDateField)
, timeFieldStyle(&st::scheduleTimeField)
, separatorStyle(&st::scheduleTimeSeparator)
, atStyle(&st::scheduleAtLabel)
, calendarStyle(&st::defaultCalendarColors) {
}

ChooseDateTimeBoxDescriptor ChooseDateTimeBox(
		not_null<GenericBox*> box,
		ChooseDateTimeBoxArgs &&args) {
	struct State {
		rpl::variable<QDate> date;
		rpl::variable<int> width;
		not_null<LinkButton*> day;
		not_null<InputField*> time;
		not_null<FlatLabel*> at;
	};
	box->setTitle(std::move(args.title));
	box->setWidth(st::boxWideWidth);

	const auto content = box->addRow(
		object_ptr<FixedHeightWidget>(box, st::scheduleHeight),
		style::al_top);
	if (args.description) {
		box->addRow(object_ptr<FlatLabel>(
			box,
			std::move(args.description),
			*args.style.labelStyle));
	}
	const auto min = args.min ? args.min : [] {
		return base::unixtime::now() + kMinimalSchedule;
	};
	const auto max = args.max ? args.max : [] {
		return base::unixtime::serialize(
			QDateTime::currentDateTime().addYears(1)) - 1;
	};
	const auto parsed = base::unixtime::parse(
		std::clamp(args.time, min(), max()));
	const auto state = box->lifetime().make_state<State>(State{
		.date = parsed.date(),
		.day = CreateChild<LinkButton>(
			content,
			DayString(parsed.date()),
			st::scheduleDateLink),
		.time = CreateChild<InputField>(
			content,
			*args.style.timeFieldStyle,
			InputField::Mode::SingleLine,
			rpl::single(QString()),
			TimeString(parsed.time())),
		.at = CreateChild<FlatLabel>(
			content,
			tr::lng_schedule_at(),
			*args.style.atStyle),
	});

	state->date.value(
	) | rpl::on_next([=](QDate date) {
		state->day->setText(DayString(date));
	}, state->day->lifetime());
	state->time->focusedChanges(
	) | rpl::filter([](bool focused) {
		return !focused;
	}) | rpl::on_next([=] {
		if (const auto parsed = ParseTime(state->time->getLastText())) {
			state->time->setText(TimeString(*parsed));
		}
	}, state->time->lifetime());

	const auto minDate = [=] {
		return base::unixtime::parse(min()).date();
	};
	const auto maxDate = [=] {
		return base::unixtime::parse(max()).date();
	};

	state->at->widthValue() | rpl::on_next([=](int width) {
		const auto full = st::scheduleDateWidth
			+ st::scheduleAtSkip
			+ width
			+ st::scheduleAtSkip
			+ st::scheduleTimeWidth;
		content->setNaturalWidth(full);
		state->width = full;
	}, state->at->lifetime());

	const auto atFont = state->at->st().style.font;
	content->widthValue(
	) | rpl::on_next([=](int width) {
		const auto paddings = width
			- state->at->width()
			- 2 * st::scheduleAtSkip
			- st::scheduleDateWidth
			- st::scheduleTimeWidth;
		const auto left = paddings / 2;
		const auto baseline = st::scheduleDateTop
			+ state->time->st().textMargins.top()
			+ state->time->st().style.font->ascent;
		state->day->moveToLeft(
			left + (st::scheduleDateWidth - state->day->width()) / 2,
			baseline - st::scheduleDateLink.font->ascent,
			width);
		state->at->moveToLeft(
			left + st::scheduleDateWidth + st::scheduleAtSkip,
			baseline - atFont->ascent,
			width);
		state->time->resizeToWidth(st::scheduleTimeWidth);
		state->time->moveToLeft(
			width - left - st::scheduleTimeWidth,
			st::scheduleDateTop,
			width);
	}, content->lifetime());

	const auto calendar
		= content->lifetime().make_state<base::weak_qptr<CalendarBox>>();
	const auto calendarStyle = args.style.calendarStyle;
	const auto dynamicImageForDate = std::move(args.dynamicImageForDate);
	state->day->setClickedCallback([=] {
		if (*calendar) {
			return;
		}
		*calendar = box->getDelegate()->show(
			Box<CalendarBox>(Ui::CalendarBoxArgs{
				.month = state->date.current(),
				.highlighted = state->date.current(),
				.callback = crl::guard(box, [=](
						QDate chosen,
						Fn<void()> close) {
					state->date = chosen;
					close();
				}),
				.minDate = minDate(),
				.maxDate = maxDate(),
				.stColors = *calendarStyle,
				.dynamicImageForDate = dynamicImageForDate,
			}));
		(*calendar)->boxClosing(
		) | rpl::on_next(crl::guard(state->time, [=] {
			state->time->setFocusFast();
		}), (*calendar)->lifetime());
	});

	const auto collect = [=] {
		const auto time = ParseTime(state->time->getLastText());
		if (!time) {
			return 0;
		}
		const auto result = base::unixtime::serialize(
			QDateTime(state->date.current(), *time));
		if (result < min() || result > max()) {
			return 0;
		}
		return result;
	};
	const auto save = [=, done = args.done] {
		if (const auto result = collect()) {
			done(result);
		} else {
			state->time->showError();
		}
	};
	state->time->submits(
	) | rpl::on_next(save, state->time->lifetime());

	auto result = ChooseDateTimeBoxDescriptor();
	box->setFocusCallback([=] { state->time->setFocusFast(); });
	result.width = state->width.value();
	result.submit = box->addButton(std::move(args.submit), save);
	result.collect = [=] {
		if (const auto result = collect()) {
			return result;
		}
		state->time->showError();
		return 0;
	};
	result.values = rpl::single(rpl::empty) | rpl::then(rpl::merge(
		state->date.changes() | rpl::to_empty,
		state->time->changes()
	)) | rpl::map([=] { return collect(); });
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });

	return result;
}

object_ptr<Ui::RpWidget> ChooseRepeatPeriod(
		not_null<Ui::RpWidget*> parent,
		ChooseRepeatPeriodArgs &&args) {
	auto result = object_ptr<Ui::RpWidget>(parent.get());
	const auto raw = result.data();

	struct Entry {
		TimeId value = 0;
		QString text;
	};
	auto map = std::vector<Entry>{
		{ 0, tr::lng_schedule_repeat_never(tr::now) },
		{ 24 * 60 * 60, tr::lng_schedule_repeat_daily(tr::now) },
		{ 7 * 24 * 60 * 60, tr::lng_schedule_repeat_weekly(tr::now) },
		{ 14 * 24 * 60 * 60, tr::lng_schedule_repeat_biweekly(tr::now) },
		{ 30 * 24 * 60 * 60, tr::lng_schedule_repeat_monthly(tr::now) },
		{
			91 * 24 * 60 * 60,
			tr::lng_schedule_repeat_every_month(tr::now, lt_count, 3)
		},
		{
			182 * 24 * 60 * 60,
			tr::lng_schedule_repeat_every_month(tr::now, lt_count, 6)
		},
		{ 365 * 24 * 60 * 60, tr::lng_schedule_repeat_yearly(tr::now) },
	};
	if (args.test) {
		map.insert(begin(map) + 1, Entry{ 300, u"Every 5 minutes"_q });
		map.insert(begin(map) + 1, Entry{ 60, u"Every minute"_q });
	}

	const auto button = Ui::CreateChild<RepeatButton>(raw);
	rpl::combine(
		raw->widthValue(),
		button->sizeValue()
	) | rpl::on_next([=](int outer, QSize size) {
		raw->resize(outer, size.height());
		button->moveToLeft((outer - size.width()) / 2, 0, outer);
	}, raw->lifetime());

	struct State {
		rpl::variable<TimeId> value;
		rpl::variable<bool> locked;
		std::unique_ptr<Ui::PopupMenu> menu;
	};
	const auto state = raw->lifetime().make_state<State>(State{
		.value = args.value,
		.locked = std::move(args.locked),
	});

	rpl::combine(
		state->value.value(),
		state->locked.value()
	) | rpl::on_next([=](TimeId value, bool locked) {
		auto result = tr::lng_schedule_repeat_label(
			tr::now,
			tr::marked);

		const auto text = [&] {
			const auto i = ranges::lower_bound(
				map,
				value,
				ranges::less{},
				&Entry::value);
			return (i != end(map)) ? i->text : map.back().text;
		}();

		button->setMarkedText(result.append(' ').append(
			tr::bold(text).append(
				Ui::Text::IconEmoji(locked
					? &st::scheduleRepeatDropdownLock
					: &st::scheduleRepeatDropdownArrow))));
		return result;
	}, button->lifetime());

	button->setClickedCallback([=] {
		if (args.filter && args.filter()) {
			return;
		}
		const auto changed = args.changed;

		state->menu = std::make_unique<Ui::PopupMenu>(button);
		const auto menu = state->menu.get();

		menu->setDestroyedCallback(crl::guard(button, [=] {
			if (state->menu.get() == menu) {
				state->menu.release();
			}
		}));
		for (const auto &entry : map) {
			const auto value = entry.value;
			menu->addAction(entry.text, [=] {
				state->value = value;
				changed(value);
			});
		}
		menu->setForcedOrigin(Ui::PanelAnimation::Origin::BottomLeft);
		const auto anchor = button->mapToGlobal(
			QPoint(button->width() / 2, 0));
		if (menu->prepareGeometryFor(anchor)) {
			menu->move(
				anchor.x() - menu->width() / 2,
				menu->y()
					- Ui::BoxShadow::ExtendFor(menu->st().shadow).bottom());
			menu->popupPrepared();
		}
	});

	return result;
}

} // namespace Ui
