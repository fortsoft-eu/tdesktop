/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/history_view_self_forwards_tagger.h"

#include "base/call_delayed.h"
#include "base/event_filter.h"
#include "base/timer_rpl.h"
#include "boxes/choose_filter_box.h"
#include "chat_helpers/share_message_phrase_factory.h"
#include "chat_helpers/tabbed_panel.h"
#include "chat_helpers/tabbed_selector.h"
#include "core/ui_integration.h"
#include "data/data_chat_filters.h"
#include "data/data_document.h"
#include "data/data_message_reaction_id.h"
#include "data/data_message_reactions.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/rect.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast_widget.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/tooltip.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_premium.h"

namespace HistoryView {
namespace {

constexpr auto kInitTimer = crl::time(3000);
constexpr auto kTimerOnLeave = crl::time(2000);
const auto kAddTagLink = u"internal:add_tag"_q;

} // namespace

SelfForwardsTagger::SelfForwardsTagger(
	not_null<Window::SessionController*> controller,
	not_null<Ui::RpWidget*> parent,
	not_null<QWidget*> scroll,
	Fn<History*()> history)
: _controller(controller)
, _parent(parent)
, _scroll(scroll)
, _history(std::move(history)) {
	setup();
}

SelfForwardsTagger::~SelfForwardsTagger() = default;

void SelfForwardsTagger::setup() {
	_controller->session().data().recentSelfForwards(
	) | rpl::on_next([=](const Data::RecentSelfForwards &data) {
		const auto history = _history ? _history() : nullptr;
		if (!history || history->peer->id != data.fromPeerId) {
			return;
		}
		showSelectorForMessages(data.ids);
	}, _lifetime);
	_controller->session().data().recentJoinChat(
	) | rpl::on_next([=](const Data::RecentJoinChat &data) {
		if (!_controller->session().data().chatsFilters().has()) {
			return;
		}
		const auto history = _history ? _history() : nullptr;
		if (!history || history->peer->id != data.fromPeerId) {
			return;
		}
		const auto peerId = data.joinedPeerId;
		if (const auto peer = _controller->session().data().peer(peerId)) {
			showChannelFilterToast(peer);
		}
	}, _lifetime);
}

void SelfForwardsTagger::showSelectorForMessages(
		const MessageIdsList &ids) {
	if (ids.empty()) {
		return;
	}
	const auto lastId = ids.back();
	const auto item = _controller->session().data().message(lastId);
	if (!item) {
		return;
	}
	const auto reactions = Data::LookupPossibleReactions(item, true);
	if (reactions.recent.empty()) {
		return;
	}

	auto text = rpl::variable<TextWithEntities>(
		ChatHelpers::ForwardedMessagePhrase({
			.toCount = 1,
			.singleMessage = (ids.size() == 1),
			.to1 = _controller->session().user(),
			.toSelfWithPremiumIsEmpty = false,
		})).current();
	text.append('\n').append(tr::link(
		tr::bold(tr::lng_add_tag_button(tr::now)),
		kAddTagLink));
	showToast(text, [=] { showTagPanel(ids); });
	if (const auto toast = _toast.get()) {
		const auto widget = toast->widget();
		const auto state = widget->lifetime().make_state<ToastTimerState>();
		setupToastTimer(widget, state, [=] { hideToast(); });
	}
}

void SelfForwardsTagger::showTagPanel(const MessageIdsList &ids) {
	using Selector = ChatHelpers::TabbedSelector;
	auto selector = object_ptr<Selector>(
		nullptr,
		_controller->uiShow(),
		Window::GifPauseReason::Layer,
		Selector::Mode::FullReactions);
	_tagPanel = base::make_unique_q<ChatHelpers::TabbedPanel>(
		_parent,
		ChatHelpers::TabbedPanelDescriptor{
			.regularWindow = _controller,
			.ownedSelector = std::move(selector),
			.separateWindow = true,
			.windowTitle = tr::lng_add_tag_button(tr::now),
		});
	_tagPanel->setDesiredHeightValues(
		1.,
		st::emojiPanMinHeight / 2,
		st::emojiPanMinHeight);
	_tagPanel->hide();
	_tagPanel->selector()->setCurrentPeer(_controller->session().user());

	const auto apply = [=](Data::ReactionId reaction) {
		for (const auto &id : ids) {
			if (const auto item = _controller->session().data().message(id)) {
				item->toggleReaction(
					reaction,
					HistoryReactionSource::Selector);
			}
		}
		_tagPanel->hideAnimated();
		hideToast();
		base::call_delayed(st::defaultToggle.duration, _parent, [=] {
			showTaggedToast(reaction);
		});
	};
	_tagPanel->selector()->emojiChosen(
	) | rpl::on_next([=](ChatHelpers::EmojiChosen data) {
		apply(Data::ReactionId{ data.emoji->text() });
	}, _tagPanel->lifetime());
	_tagPanel->selector()->customEmojiChosen(
	) | rpl::on_next([=](ChatHelpers::FileChosen data) {
		apply(Data::ReactionId{ data.document->id });
	}, _tagPanel->lifetime());
	_tagPanel->showAnimated();
}

void SelfForwardsTagger::showToast(
		const TextWithEntities &text,
		Fn<void()> callback) {
	hideToast();
	_toast = Ui::Toast::Show(_scroll, Ui::Toast::Config{
		.text = text,
		.textContext = Core::TextContext({
			.session = &_controller->session(),
		}),
		.filter = [
				fallback = ChatHelpers::ForwardedToSavedMessagesFilter(
					&_controller->session()),
				callback = std::move(callback)](
					const ClickHandlerPtr &handler,
					Qt::MouseButton button) {
			if (handler && handler->url() == kAddTagLink) {
				callback();
				return false;
			}
			return fallback(handler, button);
		},
		.iconLottie = u"toast/saved_messages"_q,
		.iconPadding = st::selfForwardsTaggerIconPadding,
		.st = &st::selfForwardsTaggerToast,
		.attach = RectPart::Top,
		.acceptinput = true,
		.infinite = true,
	});
}


void SelfForwardsTagger::showTaggedToast(
		const Data::ReactionId &reaction) {
	auto text = tr::lng_message_tagged_with(
		tr::now,
		lt_emoji,
		(reaction.custom()
			? Data::SingleCustomEmoji(reaction.custom())
			: TextWithEntities{ reaction.emoji() }),
		tr::marked);
	hideToast();

	const auto &st = st::selfForwardsTaggerToast;
	const auto viewText = tr::lng_tagged_view_saved(tr::now);
	const auto viewFont = st::classicActionFont;
	const auto rightSkip = viewFont->width(viewText)
		+ st::toastUndoSpace;

	_toast = Ui::Toast::Show(_scroll, Ui::Toast::Config{
		.text = text,
		.textContext = Core::TextContext({
			.session = &_controller->session(),
		}),
		.iconLottie = u"toast/tagged"_q,
		.iconPadding = st::selfForwardsTaggerIconPadding,
		.padding = rpl::single(QMargins(0, 0, rightSkip, 0)),
		.st = &st,
		.attach = RectPart::Top,
		.acceptinput = true,
		.duration = crl::time(3000),
	});
	if (const auto strong = _toast.get()) {
		const auto widget = strong->widget();

		const auto button = Ui::CreateChild<Ui::AbstractButton>(widget.get());
		button->setPointerCursor(true);
		button->setClickedCallback([=] {
			_controller->showPeerHistory(_controller->session().user());
			hideToast();
		});

		button->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(button);
			const auto font = viewFont->underline(button->isOver());
			const auto top = (button->height() - font->height) / 2;
			p.setPen(st::historyPremiumViewSet.textFg);
			p.setFont(font);
			p.drawText(0, top + font->ascent, viewText);
		}, button->lifetime());

		button->resize(
			viewFont->width(viewText),
			st::historyPremiumViewSet.height);

		rpl::combine(
			widget->sizeValue(),
			button->sizeValue()
		) | rpl::on_next([=](const QSize &outer, const QSize &inner) {
			button->moveToRight(
				st.padding.right(),
				(outer.height() - inner.height()) / 2,
				outer.width());
		}, widget->lifetime());

		button->show();
	}
}

void SelfForwardsTagger::showChannelFilterToast(not_null<PeerData*> peer) {
	hideToast();
	const auto toastText = peer->isChannel() && !peer->isMegagroup()
		? tr::lng_add_channel_to_filter_selector(tr::now)
		: tr::lng_add_group_to_filter_selector(tr::now);
	_toast = Ui::Toast::Show(_scroll, Ui::Toast::Config{
		.text = { .text = toastText },
		.iconLottie = u"toast/chats_filter_in"_q,
		.iconPadding = st::selfForwardsTaggerIconPadding,
		.st = &st::joinChatAddToFilterToast,
		.attach = RectPart::Top,
		.acceptinput = true,
		.infinite = true,
	});
	if (const auto strong = _toast.get()) {
		const auto widget = strong->widget();
		const auto rightButton = createRightButton(widget);
		const auto history = peer->owner().history(peer);

		const auto state = widget->lifetime().make_state<ToastTimerState>();

		rightButton->setClickedCallback([=] {
			state->expanded = true;
			state->timerLifetime.destroy();
			const auto menu = Ui::CreateChild<Ui::PopupMenu>(
				rightButton,
				st::foldersMenu);
			menu->setForcedOrigin(Ui::PanelAnimation::Origin::TopRight);
			FillChooseFilterMenu(_controller, menu, history);
			if (!menu->empty()) {
				menu->popup(
					rightButton->mapToGlobal(
						QPoint(
							rightButton->width(),
							rightButton->height() + rightButton->y())));
				QObject::connect(menu, &QObject::destroyed, [=] {
					hideToast();
				});
			} else {
				hideToast();
			}
		});

		setupToastTimer(widget, state, [=] { hideToast(); });
	}
}

not_null<Ui::AbstractButton*> SelfForwardsTagger::createRightButton(
		not_null<Ui::RpWidget*> widget) {
	const auto button = Ui::CreateChild<Ui::IconButton>(
		widget.get(),
		st::joinChatAddToFilterToastButton);
	widget->sizeValue() | rpl::on_next([=](const QSize &size) {
		button->moveToRight(
			st::lineWidth * 4,
			(size.height() - button->height()) / 2);
	}, button->lifetime());

	button->show();
	return button;
}

void SelfForwardsTagger::setupToastTimer(
		not_null<Ui::RpWidget*> widget,
		not_null<ToastTimerState*> state,
		Fn<void()> hideCallback) {
	const auto restartTimer = [=](crl::time ms) {
		state->timerLifetime.destroy();
		base::timer_once(ms) | rpl::on_next([=] {
			hideCallback();
		}, state->timerLifetime);
	};

	base::install_event_filter(widget, [=](not_null<QEvent*> event) {
		if (event->type() == QEvent::MouseButtonPress) {
			state->timerLifetime.destroy();
			return base::EventFilterResult::Continue;
		} else if (!state->expanded && event->type() == QEvent::Enter) {
			state->timerLifetime.destroy();
			return base::EventFilterResult::Continue;
		} else if (!state->expanded && event->type() == QEvent::Leave) {
			restartTimer(kTimerOnLeave);
			return base::EventFilterResult::Continue;
		}
		return base::EventFilterResult::Continue;
	}, widget->lifetime());

	restartTimer(kInitTimer);
}

void SelfForwardsTagger::hideToast() {
	if (const auto strong = _toast.get()) {
		strong->hideAnimated();
	}
}

} // namespace HistoryView
