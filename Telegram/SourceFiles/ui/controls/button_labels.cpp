/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/controls/button_labels.h"

#include "ui/qt_object_factory.h"
#include "ui/style/style_classic.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"

#include <QtCore/QScopedValueRollback>

namespace Ui {

void SetButtonTwoLabels(
		not_null<Ui::RoundButton*> button,
		rpl::producer<TextWithEntities> title,
		rpl::producer<TextWithEntities> subtitle,
		const style::FlatLabel &st,
		const style::FlatLabel &subst,
		const style::color *textFg) {
	SetButtonTwoLabels(
		button,
		button->st().textTop,
		std::move(title),
		std::move(subtitle),
		st,
		subst,
		textFg);
}

void SetButtonTwoLabels(
		not_null<Ui::RpWidget*> button,
		int singleLineTextTop,
		rpl::producer<TextWithEntities> title,
		rpl::producer<TextWithEntities> subtitle,
		const style::FlatLabel &st,
		const style::FlatLabel &subst,
		const style::color *textFg) {
	const auto round = dynamic_cast<RoundButton*>(button.get());
	const auto classic = round || button->property("classicButton").toBool();
	struct State {
		style::FlatLabel title;
		style::FlatLabel subtitle;
		bool withSubtitle = false;
		bool updating = false;
	};
	const auto state = button->lifetime().make_state<State>(State{
		.title = st,
		.subtitle = subst,
	});
	if (classic) {
		for (const auto label : { &state->title, &state->subtitle }) {
			label->style.font = style::font(st::classicSettingsFont->size(), label->style.font->flags(), u"Tahoma"_q);
			label->style.lineHeight = 0;
			label->textFg = st::classicMenuText;
			label->maxHeight = label->style.font->height;
		}
	}
	const auto buttonTitle = Ui::CreateChild<Ui::FlatLabel>(
		button,
		std::move(title),
		state->title);
	buttonTitle->show();
	const auto buttonSubtitle = Ui::CreateChild<Ui::FlatLabel>(
		button,
		rpl::duplicate(
			subtitle
		) | rpl::filter([](const TextWithEntities &text) {
			return !text.empty();
		}),
		state->subtitle);
	buttonSubtitle->setOpacity(classic ? 1. : 0.6);
	if (textFg && !classic) {
		buttonTitle->setTextColorOverride((*textFg)->c);
		buttonSubtitle->setTextColorOverride((*textFg)->c);
		style::PaletteChanged() | rpl::on_next([=] {
			buttonTitle->setTextColorOverride((*textFg)->c);
			buttonSubtitle->setTextColorOverride((*textFg)->c);
		}, buttonTitle->lifetime());
	}
	const auto updateGeometry = [=] {
		if (state->updating) {
			return;
		}
		const auto guard = QScopedValueRollback(state->updating, true);
		const auto content = classic
			? ClassicButtonContentRect(button->rect(), button)
			: button->rect();
		buttonTitle->resizeToWidth(std::min(
			buttonTitle->naturalWidth(), content.width()));
		buttonSubtitle->resizeToWidth(std::min(
			buttonSubtitle->naturalWidth(), content.width()));
		const auto two = buttonTitle->height() + buttonSubtitle->height();
		if (classic) {
			const auto height = state->withSubtitle
				? std::max(st::classicButtonHeight,
					two + 2 * st::classicButtonMinimumPadding)
				: st::classicButtonHeight;
			button->setMinimumHeight(height);
			button->resize(button->width(), height);
		}
		const auto abstract = dynamic_cast<AbstractButton*>(button.get());
		const auto down = abstract && abstract->isDown();
		const auto shift = classic
			? ClassicButtonContentOffset(button, down)
			: QPoint();
		const auto titleTop = state->withSubtitle
			? (button->height() - two) / 2
			: classic
			? (button->height() - buttonTitle->height()) / 2
			: singleLineTextTop;
		buttonTitle->moveToLeft(
			(button->width() - buttonTitle->width()) / 2 + shift.x(),
			titleTop + shift.y());
		buttonSubtitle->moveToLeft(
			(button->width() - buttonSubtitle->width()) / 2 + shift.x(),
			titleTop + buttonTitle->height() + shift.y());
	};
	std::move(subtitle) | rpl::on_next([=](const TextWithEntities &text) {
		state->withSubtitle = !text.empty();
		buttonSubtitle->setVisible(state->withSubtitle);
		updateGeometry();
	}, button->lifetime());
	rpl::combine(
		button->sizeValue(),
		buttonTitle->naturalWidthValue(),
		buttonSubtitle->naturalWidthValue()
	) | rpl::on_next(updateGeometry, button->lifetime());
	button->paintRequest() | rpl::on_next(updateGeometry, button->lifetime());
	buttonTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
	buttonSubtitle->setAttribute(Qt::WA_TransparentForMouseEvents);
}

} // namespace Ui
