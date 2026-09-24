/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_cover_classic.h"

#include "api/api_user_privacy.h"
#include "base/timer_rpl.h"
#include "data/data_peer_values.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_emoji_statuses.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_changes.h"
#include "data/data_saved_music.h"
#include "data/data_session.h"
#include "data/data_forum_topic.h"
#include "data/stickers/data_custom_emoji.h"
#include "info/profile/info_profile_badge.h"
#include "info/profile/info_profile_emoji_status_panel.h"
#include "info/profile/info_profile_music_button.h"
#include "info/profile/info_profile_values.h"
#include "info/saved/info_saved_music_widget.h"
#include "info/info_controller.h"
#include "info/info_wrap_widget.h"
#include "ui/text/format_song_document_name.h"
#include "ui/style/style_radius.h"
#include "info/info_memento.h"
#include "boxes/peers/edit_forum_topic_box.h"
#include "boxes/report_messages_box.h"
#include "history/view/media/history_view_sticker_player.h"
#include "lang/lang_keys.h"
#include "ui/boxes/show_or_premium_box.h"
#include "ui/controls/stars_rating.h"
#include "ui/controls/userpic_button.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/text/text_utilities.h"
#include "ui/basic_click_handlers.h"
#include "ui/ui_utility.h"
#include "ui/painter.h"
#include "base/event_filter.h"
#include "base/unixtime.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "settings/sections/settings_premium.h"
#include "chat_helpers/stickers_lottie.h"
#include "apiwrap.h"
#include "api/api_peer_photo.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_info_profile_actions.h"
#include "styles/style_info_profile_top_bar.h"
#include "styles/style_dialogs.h"
#include "styles/style_menu_icons.h"

namespace Info::Profile {
namespace {

constexpr auto kWaitBeforeGiftBadge = crl::time(1000);
constexpr auto kGiftBadgeGlares = 3;
constexpr auto kGlareDurationStep = crl::time(320);
constexpr auto kGlareTimeout = crl::time(1000);

[[nodiscard]] auto MembersStatusText(int count) {
	return tr::lng_chat_status_members(tr::now, lt_count_decimal, count);
};

[[nodiscard]] auto OnlineStatusText(int count) {
	return tr::lng_chat_status_online(tr::now, lt_count_decimal, count);
};

[[nodiscard]] auto ChatStatusText(int fullCount, int onlineCount, bool isGroup) {
	if (onlineCount > 1 && onlineCount <= fullCount) {
		return tr::lng_chat_status_members_online(tr::now, lt_members_count, MembersStatusText(fullCount), lt_online_count, OnlineStatusText(onlineCount));
	} else if (fullCount > 0) {
		return isGroup
			? tr::lng_chat_status_members(tr::now, lt_count_decimal, fullCount)
			: tr::lng_chat_status_subscribers(tr::now, lt_count_decimal, fullCount);
	}
	return isGroup ? tr::lng_group_status(tr::now) : tr::lng_channel_status(tr::now);
};

[[nodiscard]] const style::InfoProfileCover &CoverStyle(not_null<PeerData*> peer, Data::ForumTopic *topic, ClassicCover::Role role) {
	return role == ClassicCover::Role::EditContact
		? st::infoEditContactCover
		: topic
		? st::infoTopicCover
		: peer->isMegagroup()
		? st::infoProfileMegagroupCover
		: st::infoProfileCover;
}

[[nodiscard]] MusicButtonData DocumentMusicButtonData(
		not_null<DocumentData*> document) {
	return { .name = Ui::Text::FormatSongNameFor(document) };
}

} // namespace

bool UseClassicProfile(not_null<const Controller*> controller) {
	const auto key = controller->key();
	const auto peer = key.peer();
	return controller->wrap() == Wrap::Side
		&& !key.topic()
		&& !key.sublist()
		&& !key.savedMessages()
		&& peer
		&& (peer->isUser() || peer->isChat() || peer->isChannel());
}

class ClassicCover::BadgeTooltip final : public Ui::RpWidget {
public:
	BadgeTooltip(
		not_null<QWidget*> parent,
		std::shared_ptr<Data::EmojiStatusCollectible> collectible,
		not_null<QWidget*> pointTo);

	void fade(bool shown);
	void finishAnimating();

	[[nodiscard]] crl::time glarePeriod() const;

private:
	void paintEvent(QPaintEvent *e) override;

	void setupGeometry(not_null<QWidget*> pointTo);
	void prepareImage();
	void showGlare();

	const style::ImportantTooltip &_st;
	std::shared_ptr<Data::EmojiStatusCollectible> _collectible;
	QString _text;
	const style::font &_font;
	QSize _inner;
	QSize _outer;
	int _stroke = 0;
	int _skip = 0;
	QSize _full;
	int _glareSize = 0;
	int _glareRange = 0;
	crl::time _glareDuration = 0;
	base::Timer _glareTimer;

	Ui::Animations::Simple _showAnimation;
	Ui::Animations::Simple _glareAnimation;

	QImage _image;
	int _glareRight = 0;
	int _imageGlareRight = 0;
	int _arrowMiddle = 0;
	int _imageArrowMiddle = 0;

	bool _shown = false;

};

ClassicCover::BadgeTooltip::BadgeTooltip(
	not_null<QWidget*> parent,
	std::shared_ptr<Data::EmojiStatusCollectible> collectible,
	not_null<QWidget*> pointTo)
: Ui::RpWidget(parent)
, _st(st::infoGiftTooltip)
, _collectible(std::move(collectible))
, _text(_collectible->title)
, _font(st::infoGiftTooltipFont)
, _inner(_font->width(_text), _font->height)
, _outer(_inner.grownBy(_st.padding))
, _stroke(st::lineWidth)
, _skip(2 * _stroke)
, _full(_outer + QSize(2 * _skip, _st.arrow + 2 * _skip))
, _glareSize(_outer.height() * 3)
, _glareRange(_outer.width() + _glareSize)
, _glareDuration(_glareRange * kGlareDurationStep / _glareSize)
, _glareTimer([=] { showGlare(); }) {
	resize(_full + QSize(0, _st.shift));
	setupGeometry(pointTo);
}

void ClassicCover::BadgeTooltip::fade(bool shown) {
	if (_shown == shown) {
		return;
	}
	show();
	_shown = shown;
	_showAnimation.start([=] {
		update();
		if (!_showAnimation.animating()) {
			if (!_shown) {
				hide();
			} else {
				showGlare();
			}
		}
	}, _shown ? 0. : 1., _shown ? 1. : 0., _st.duration, anim::easeInCirc);
}

void ClassicCover::BadgeTooltip::showGlare() {
	_glareAnimation.start([=] {
		update();
		if (!_glareAnimation.animating()) {
			_glareTimer.callOnce(kGlareTimeout);
		}
	}, 0., 1., _glareDuration);
}

void ClassicCover::BadgeTooltip::finishAnimating() {
	_showAnimation.stop();
	if (!_shown) {
		hide();
	}
}

crl::time ClassicCover::BadgeTooltip::glarePeriod() const {
	return _glareDuration + kGlareTimeout;
}

void ClassicCover::BadgeTooltip::paintEvent(QPaintEvent *e) {
	const auto glare = _glareAnimation.value(0.);
	_glareRight = anim::interpolate(0, _glareRange, glare);
	prepareImage();

	auto p = QPainter(this);
	const auto shown = _showAnimation.value(_shown ? 1. : 0.);
	p.setOpacity(shown);
	const auto imageHeight = _image.height() / _image.devicePixelRatio();
	const auto top = anim::interpolate(0, height() - imageHeight, shown);
	p.drawImage(0, top, _image);
}

void ClassicCover::BadgeTooltip::setupGeometry(not_null<QWidget*> pointTo) {
	auto widget = pointTo.get();
	const auto parent = parentWidget();

	const auto refresh = [=] {
		const auto rect = Ui::MapFrom(parent, pointTo, pointTo->rect());
		const auto point = QPoint(rect.center().x(), rect.y());
		const auto left = point.x() - (width() / 2);
		const auto skip = _st.padding.left();
		setGeometry(
			std::min(std::max(left, skip), parent->width() - width() - skip),
			std::max(point.y() - height() - _st.margin.bottom(), skip),
			width(),
			height());
		const auto arrowMiddle = point.x() - x();
		if (_arrowMiddle != arrowMiddle) {
			_arrowMiddle = arrowMiddle;
			update();
		}
	};
	refresh();
	while (widget && widget != parent) {
		base::install_event_filter(this, widget, [=](not_null<QEvent*> e) {
			if (e->type() == QEvent::Resize || e->type() == QEvent::Move || e->type() == QEvent::ZOrderChange) {
				refresh();
				raise();
			}
			return base::EventFilterResult::Continue;
		});
		widget = widget->parentWidget();
	}
}

void ClassicCover::BadgeTooltip::prepareImage() {
	const auto ratio = style::DevicePixelRatio();
	const auto arrow = _st.arrow;
	const auto size = _full * ratio;
	if (_image.size() != size) {
		_image = QImage(size, QImage::Format_ARGB32_Premultiplied);
		_image.setDevicePixelRatio(ratio);
	} else if (_imageGlareRight == _glareRight
		&& _imageArrowMiddle == _arrowMiddle) {
		return;
	}
	_imageGlareRight = _glareRight;
	_imageArrowMiddle = _arrowMiddle;
	_image.fill(Qt::transparent);

	const auto gfrom = _imageGlareRight - _glareSize;
	const auto gtill = _imageGlareRight;

	auto path = QPainterPath();
	const auto width = _outer.width();
	const auto height = _outer.height();
	const auto radius = style::CornerRadius((height + 1) / 2);
	const auto diameter = style::CornerRadius(height);
	path.moveTo(radius, 0);
	path.lineTo(width - radius, 0);
	if (radius) {
		path.arcTo(QRect(QPoint(width - diameter, 0), QSize(diameter, diameter)), 90, -180);
	} else {
		path.lineTo(width, height);
	}
	const auto xarrow = _arrowMiddle - _skip;
	if (xarrow - arrow <= radius || xarrow + arrow >= width - radius) {
		path.lineTo(radius, height);
	} else {
		path.lineTo(xarrow + arrow, height);
		path.lineTo(xarrow, height + arrow);
		path.lineTo(xarrow - arrow, height);
		path.lineTo(radius, height);
	}
	if (radius) {
		path.arcTo(QRect(QPoint(0, 0), QSize(diameter, diameter)), -90, -180);
	}
	path.closeSubpath();

	auto p = QPainter(&_image);
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(Qt::NoPen);
	if (gtill > 0) {
		auto gradient = QLinearGradient(gfrom, 0, gtill, 0);
		gradient.setStops({
			{ 0., _collectible->edgeColor },
			{ 0.5, _collectible->centerColor },
			{ 1., _collectible->edgeColor },
		});
		p.setBrush(gradient);
	} else {
		p.setBrush(_collectible->edgeColor);
	}
	p.translate(_skip, _skip);
	p.drawPath(path);
	p.setCompositionMode(QPainter::CompositionMode_Source);
	p.setBrush(Qt::NoBrush);
	auto copy = _collectible->textColor;
	copy.setAlpha(0);
	if (gtill > 0) {
		auto gradient = QLinearGradient(gfrom, 0, gtill, 0);
		gradient.setStops({
			{ 0., copy },
			{ 0.5, _collectible->textColor },
			{ 1., copy },
		});
		p.setPen(QPen(gradient, _stroke));
	} else {
		p.setPen(QPen(copy, _stroke));
	}
	p.drawPath(path);
	p.setCompositionMode(QPainter::CompositionMode_SourceOver);
	p.setFont(_font);
	p.setPen(QColor(255, 255, 255));
	p.drawText(_st.padding.left(), _st.padding.top() + _font->ascent, _text);
}

ClassicCover::ClassicCover(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	Fn<not_null<QWidget*>()> parentForTooltip)
: ClassicCover(
	parent,
	controller,
	peer,
	nullptr,
	Role::Info,
	NameValue(peer),
	parentForTooltip) {
}

ClassicCover::ClassicCover(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<Data::ForumTopic*> topic)
: ClassicCover(
	parent,
	controller,
	topic->peer(),
	topic,
	Role::Info,
	TitleValue(topic),
	nullptr) {
}

ClassicCover::ClassicCover(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	Role role,
	rpl::producer<QString> title)
: ClassicCover(
	parent,
	controller,
	peer,
	nullptr,
	role,
	std::move(title),
	nullptr) {
}

ClassicCover::ClassicCover(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	Data::ForumTopic *topic,
	Role role,
	rpl::producer<QString> title,
	Fn<not_null<QWidget*>()> parentForTooltip)
: FixedHeightWidget(parent, CoverStyle(peer, topic, role).height)
, _st(CoverStyle(peer, topic, role))
, _role(role)
, _controller(controller)
, _peer(peer)
, _emojiStatusPanel(peer->isSelf()
	? std::make_unique<EmojiStatusPanel>()
	: nullptr)
, _botVerify(role == Role::EditContact
	? nullptr
	: std::make_unique<Badge>(
		this,
		st::infoBotVerifyBadge,
		&peer->session(),
		BotVerifyBadgeForPeer(peer),
		nullptr,
		[=] {
			return controller->isGifPausedAtLeastFor(
				Window::GifPauseReason::Layer);
		}))
, _badgeContent(BadgeContentForPeer(peer))
, _badge(role == Role::EditContact
	? nullptr
	: std::make_unique<Badge>(
		this,
		st::infoPeerBadge,
		&peer->session(),
		_badgeContent.value(),
		_emojiStatusPanel.get(),
		[=] {
			return controller->isGifPausedAtLeastFor(
				Window::GifPauseReason::Layer);
		}))
, _verified(role == Role::EditContact
	? nullptr
	: std::make_unique<Badge>(
		this,
		st::infoPeerBadge,
		&peer->session(),
		VerifiedContentForPeer(peer),
		_emojiStatusPanel.get(),
		[=] {
			return controller->isGifPausedAtLeastFor(
				Window::GifPauseReason::Layer);
		}))
, _parentForTooltip(std::move(parentForTooltip))
, _badgeTooltipHide([=] { hideBadgeTooltip(); })
, _userpic(topic
	? nullptr
	: object_ptr<Ui::UserpicButton>(
		this,
		controller,
		_peer->userpicPaintingPeer(),
		Ui::UserpicButton::Role::OpenPhoto,
		Ui::UserpicButton::Source::PeerPhoto,
		_st.photo,
		_peer->userpicShape()))
, _changePersonal((role == Role::Info
	|| role == Role::EditContact
	|| topic
	|| !_peer->isUser()
	|| _peer->isSelf()
	|| _peer->asUser()->isBot())
	? nullptr
	: CreateUploadSubButton(this, _peer->asUser(), controller).get())
, _iconButton(topic
	? object_ptr<TopicIconButton>(this, controller, topic)
	: nullptr)
, _name(this, _st.name)
, _starsRating(_peer->isUser() && _role != Role::EditContact
	? std::make_unique<Ui::StarsRating>(
		this,
		_controller->uiShow(),
		_peer->isSelf() ? QString() : _peer->shortName(),
		Data::StarsRatingValue(_peer),
		(_peer->isSelf()
			? [=] { return _peer->owner().pendingStarsRating(); }
			: Fn<Data::StarsRatingPending()>()))
	: nullptr)
, _status(this, _st.status)
, _showLastSeen(this, tr::lng_status_lastseen_when(), _st.showLastSeen)
, _refreshStatusTimer([this] { refreshStatusText(); }) {
	_peer->updateFull();
	if (const auto broadcast = _peer->monoforumBroadcast()) {
		broadcast->updateFull();
	}

	_name->setSelectable(true);
	_name->setContextCopyText(tr::lng_profile_copy_fullname(tr::now));

	if (!_peer->isMegagroup()) {
		_status->setAttribute(Qt::WA_TransparentForMouseEvents);
		if (const auto rating = _starsRating.get()) {
			_statusShift = rating->widthValue();
			_statusShift.changes() | rpl::on_next([=] {
				refreshStatusGeometry(width());
			}, _status->lifetime());
			rating->raise();
		}
	}

	setupShowLastSeen();

	if (_badge) {
		_badge->setPremiumClickCallback([=] {
			if (const auto panel = _emojiStatusPanel.get()) {
				panel->show(_controller, _badge->widget(), _badge->sizeTag());
			} else {
				::Settings::ShowEmojiStatusPremium(_controller, _peer);
			}
		});
	}
	auto badgeUpdates = rpl::producer<rpl::empty_value>();
	if (_badge) {
		badgeUpdates = rpl::merge(
			std::move(badgeUpdates),
			_badge->updated());
	}
	if (_verified) {
		badgeUpdates = rpl::merge(
			std::move(badgeUpdates),
			_verified->updated());
	}
	if (_botVerify) {
		badgeUpdates = rpl::merge(
			std::move(badgeUpdates),
			_botVerify->updated());
	}
	std::move(badgeUpdates) | rpl::on_next([=] {
		refreshNameGeometry(width());
	}, _name->lifetime());

	initViewers(std::move(title));
	setupChildGeometry();
	setupUniqueBadgeTooltip();
	if (_role != Role::EditContact) {
		setupSavedMusic();
	}

	if (_userpic) {
	} else if (topic->canEdit()) {
		_iconButton->setClickedCallback([=] {
			_controller->show(Box(
				EditForumTopicBox,
				_controller,
				topic->history(),
				topic->rootId()));
		});
	} else {
		_iconButton->setAttribute(Qt::WA_TransparentForMouseEvents);
	}
}

void ClassicCover::setupShowLastSeen() {
	const auto user = _peer->asUser();
	if (_st.showLastSeenVisible
		&& user
		&& !user->isSelf()
		&& !user->isBot()
		&& !user->isServiceUser()
		&& user->session().premiumPossible()) {
		if (user->session().premium()) {
			if (user->lastseen().isHiddenByMe()) {
				user->updateFullForced();
			}
			_showLastSeen->hide();
			return;
		}

		rpl::combine(
			user->session().changes().peerFlagsValue(
				user,
				Data::PeerUpdate::Flag::OnlineStatus),
			Data::AmPremiumValue(&user->session())
		) | rpl::on_next([=](auto, bool premium) {
			const auto wasShown = !_showLastSeen->isHidden();
			const auto hiddenByMe = user->lastseen().isHiddenByMe();
			const auto shown = hiddenByMe
				&& !user->lastseen().isOnline(base::unixtime::now())
				&& !premium
				&& user->session().premiumPossible();
			_showLastSeen->setVisible(shown);
			if (wasShown && premium && hiddenByMe) {
				user->updateFullForced();
			}
		}, _showLastSeen->lifetime());

		_controller->session().api().userPrivacy().value(
			Api::UserPrivacy::Key::LastSeen
		) | rpl::filter([=](Api::UserPrivacy::Rule rule) {
			return (rule.option == Api::UserPrivacy::Option::Everyone);
		}) | rpl::on_next([=] {
			if (user->lastseen().isHiddenByMe()) {
				user->updateFullForced();
			}
		}, _showLastSeen->lifetime());
	} else {
		_showLastSeen->hide();
	}

	using TextTransform = Ui::RoundButtonTextTransform;
	_showLastSeen->setTextTransform(TextTransform::NoTransform);
	_showLastSeen->setFullRadius(true);

	_showLastSeen->setClickedCallback([=] {
		const auto type = Ui::ShowOrPremium::LastSeen;
		auto box = Box(Ui::ShowOrPremiumBox, type, user->shortName(), crl::guard(this, [=] {
			_controller->session().api().userPrivacy().save(
				::Api::UserPrivacy::Key::LastSeen,
				{});
		}), crl::guard(this, [=] {
			::Settings::ShowPremium(_controller, u"lastseen_hidden"_q);
		}));
		_controller->show(std::move(box));
	});
}

void ClassicCover::setupChildGeometry() {
	widthValue(
	) | rpl::on_next([this](int newWidth) {
		if (_userpic) {
			_userpic->moveToLeft(_st.photoLeft, _st.photoTop, newWidth);
		} else {
			_iconButton->moveToLeft(_st.photoLeft, _st.photoTop, newWidth);
		}
		if (_changePersonal) {
			_changePersonal->moveToLeft(
				(_st.photoLeft
					+ _st.photo.photoSize
					- _changePersonal->width()
					+ st::infoClassicPersonalLeft),
				(_userpic->y()
					+ _userpic->height()
					- _changePersonal->height()));
		}
		refreshNameGeometry(newWidth);
		refreshStatusGeometry(newWidth);
	}, lifetime());
}

void ClassicCover::setupSavedMusic() {
	if (!Data::SavedMusic::Supported(_peer->id)) {
		return;
	}
	Data::SavedMusicList(
		_peer,
		nullptr,
		1
	) | rpl::map([=](const Data::SavedMusicSlice &data) {
		return data.size() ? data[0].get() : nullptr;
	}) | rpl::on_next([=](HistoryItem *item) {
		const auto media = item ? item->media() : nullptr;
		const auto document = media ? media->document() : nullptr;
		if (!document) {
			_musicButton = nullptr;
			resize(width(), _st.height);
		} else if (!_musicButton) {
			using namespace Info::Saved;
			_musicButton = std::make_unique<MusicButton>(
				this,
				DocumentMusicButtonData(document),
				[=] { _controller->showSection(MakeMusic(_peer)); });
			_musicButton->show();

			widthValue(
			) | rpl::on_next([=](int newWidth) {
				_musicButton->resizeToWidth(newWidth);
				const auto skip = st::infoClassicMusicBottom;
				_musicButton->moveToLeft(0, _st.height - skip, newWidth);
				resize(width(), _st.height + _musicButton->height());
			}, _musicButton->lifetime());
		} else {
			_musicButton->updateData(DocumentMusicButtonData(document));
		}
	}, lifetime());
}

ClassicCover *ClassicCover::setOnlineCount(rpl::producer<int> &&count) {
	_onlineCount = std::move(count);
	return this;
}

std::optional<QImage> ClassicCover::updatedPersonalPhoto() const {
	return _personalChosen;
}

void ClassicCover::initViewers(rpl::producer<QString> title) {
	using Flag = Data::PeerUpdate::Flag;
	std::move(
		title
	) | rpl::on_next([=](const QString &title) {
		_name->setText(title);
		refreshNameGeometry(width());
	}, lifetime());

	rpl::combine(
		_peer->session().changes().peerFlagsValue(
			_peer,
			Flag::OnlineStatus | Flag::Members),
		_onlineCount.value()
	) | rpl::on_next([=] {
		refreshStatusText();
	}, lifetime());

	_peer->session().changes().peerFlagsValue(
		_peer,
		(_peer->isUser() ? Flag::IsContact : Flag::Rights)
	) | rpl::on_next([=] {
		refreshUploadPhotoOverlay();
	}, lifetime());

	setupChangePersonal();
}

void ClassicCover::refreshUploadPhotoOverlay() {
	if (!_userpic) {
		return;
	} else if (_role == Role::EditContact) {
		_userpic->setAttribute(Qt::WA_TransparentForMouseEvents);
		return;
	}

	const auto canChange = [&] {
		if (const auto chat = _peer->asChat()) {
			return chat->canEditInformation();
		} else if (const auto channel = _peer->asChannel()) {
			return channel->canEditInformation()
				&& !channel->isMonoforum();
		} else if (const auto user = _peer->asUser()) {
			return user->isSelf()
				|| (user->isContact()
					&& !user->isInaccessible()
					&& !user->isServiceUser());
		}
		Unexpected("Peer type in Info::Profile::ClassicCover.");
	}();

	_userpic->switchChangePhotoOverlay(canChange, [=](
			Ui::UserpicButton::ChosenImage chosen) {
		using ChosenType = Ui::UserpicButton::ChosenType;
		auto result = Api::PeerPhoto::UserPhoto{
			base::take<QImage>(chosen.image), // Strange MSVC bug with take.
			chosen.markup.documentId,
			chosen.markup.colors,
			std::move(chosen.video),
		};
		switch (chosen.type) {
		case ChosenType::Set:
			_userpic->showCustom(base::duplicate(result.image));
			_peer->session().api().peerPhoto().upload(
				_peer,
				std::move(result));
			break;
		case ChosenType::Suggest:
			_peer->session().api().peerPhoto().suggest(
				_peer,
				std::move(result));
			break;
		}
	});

	const auto canReport = [=, peer = _peer] {
		if (!peer->hasUserpic()) {
			return false;
		}
		const auto user = peer->asUser();
		if (!user) {
			if (canChange) {
				return false;
			}
		} else if (user->hasPersonalPhoto()
				|| user->isSelf()
				|| user->isInaccessible()
				|| user->isRepliesChat()
				|| user->isVerifyCodes()
				|| (user->botInfo && user->botInfo->canEditInformation)
				|| user->isServiceUser()) {
			return false;
		}
		return true;
	};

	const auto contextMenu = _userpic->lifetime().make_state<base::unique_qptr<Ui::PopupMenu>>();
	const auto showMenu = [=, peer = _peer, controller = _controller](
			not_null<Ui::RpWidget*> parent) {
		if (!canReport()) {
			return false;
		}
		*contextMenu = base::make_unique_q<Ui::PopupMenu>(parent, st::popupMenuWithIcons);
		contextMenu->get()->addAction(tr::lng_profile_report(tr::now), [=] {
			controller->show(
				ReportProfilePhotoBox(peer, peer->owner().photo(peer->userpicPhotoId())),
				Ui::LayerOption::CloseOther);
		}, &st::menuIconReport);
		contextMenu->get()->popup(QCursor::pos());
		return true;
	};
	base::install_event_filter(_userpic, [showMenu, raw = _userpic.data()](not_null<QEvent*> e) {
		return (e->type() == QEvent::ContextMenu && showMenu(raw))
			? base::EventFilterResult::Cancel
			: base::EventFilterResult::Continue;
	});

	if (const auto user = _peer->asUser()) {
		_userpic->resetPersonalRequests() | rpl::on_next([=] {
			user->session().api().peerPhoto().clearPersonal(user);
			_userpic->showSource(Ui::UserpicButton::Source::PeerPhoto);
		}, lifetime());
	}
}

void ClassicCover::setupChangePersonal() {
	if (!_changePersonal) {
		return;
	}

	_changePersonal->chosenImages() | rpl::on_next([=](Ui::UserpicButton::ChosenImage &&chosen) {
		if (chosen.type == Ui::UserpicButton::ChosenType::Suggest) {
			_peer->session().api().peerPhoto().suggest(
				_peer,
				{
					std::move(chosen.image),
					chosen.markup.documentId,
					chosen.markup.colors,
					std::move(chosen.video),
				});
		} else {
			_personalChosen = std::move(chosen.image);
			_userpic->showCustom(base::duplicate(*_personalChosen));
			_changePersonal->overrideHasPersonalPhoto(true);
			_changePersonal->showSource(
				Ui::UserpicButton::Source::NonPersonalIfHasPersonal);
		}
	}, _changePersonal->lifetime());

	_changePersonal->resetPersonalRequests() | rpl::on_next([=] {
		_personalChosen = QImage();
		_userpic->showSource(
			Ui::UserpicButton::Source::NonPersonalPhoto);
		_changePersonal->overrideHasPersonalPhoto(false);
		_changePersonal->showCustom(QImage());
	}, _changePersonal->lifetime());
}

void ClassicCover::refreshStatusText() {
	auto hasMembersLink = [&] {
		if (auto megagroup = _peer->asMegagroup()) {
			return megagroup->canViewMembers();
		}
		return false;
	}();
	auto statusText = [&]() -> TextWithEntities {
		using namespace Ui::Text;
		auto currentTime = base::unixtime::now();
		if (auto user = _peer->asUser()) {
			const auto result = Data::OnlineTextFull(user, currentTime);
			const auto showOnline = Data::OnlineTextActive(user, currentTime);
			const auto updateIn = Data::OnlineChangeTimeout(user, currentTime);
			if (showOnline) {
				_refreshStatusTimer.callOnce(updateIn);
			}
			return showOnline ? Ui::Text::Colorized(result) : TextWithEntities{ .text = result };
		} else if (auto chat = _peer->asChat()) {
			if (!chat->amIn()) {
				return tr::lng_chat_status_unaccessible({}, tr::marked);
			}
			const auto onlineCount = _onlineCount.current();
			const auto fullCount = std::max(chat->count, int(chat->participants.size()));
			return { .text = ChatStatusText(fullCount, onlineCount, true) };
		} else if (auto broadcast = _peer->monoforumBroadcast()) {
			auto result = ChatStatusText(qMax(broadcast->membersCount(), 1), 0, false);
			return TextWithEntities{ .text = result };
		} else if (auto channel = _peer->asChannel()) {
			const auto onlineCount = _onlineCount.current();
			const auto fullCount = qMax(channel->membersCount(), 1);
			auto result = ChatStatusText(fullCount, onlineCount, channel->isMegagroup());
			return hasMembersLink ? Ui::Text::Link(result) : TextWithEntities{ .text = result };
		}
		return tr::lng_chat_status_unaccessible(tr::now, tr::marked);
	}();
	_status->setMarkedText(statusText);
	if (hasMembersLink) {
		_status->setLink(1, std::make_shared<LambdaClickHandler>([=] {
			_showSection.fire(Info::Section::Type::Members);
		}));
	}
	refreshStatusGeometry(width());
}

ClassicCover::~ClassicCover() {
	base::take(_badgeTooltip);
	base::take(_badgeOldTooltips);
}

void ClassicCover::refreshNameGeometry(int newWidth) {
	auto nameWidth = newWidth - _st.nameLeft - _st.rightSkip;
	const auto verifiedWidget = _verified ? _verified->widget() : nullptr;
	const auto badgeWidget = _badge ? _badge->widget() : nullptr;
	if (verifiedWidget) {
		nameWidth -= verifiedWidget->width();
	}
	if (badgeWidget) {
		nameWidth -= badgeWidget->width();
	}
	if (verifiedWidget || badgeWidget) {
		nameWidth -= st::infoVerifiedCheckPosition.x();
	}
	auto nameLeft = _st.nameLeft;
	const auto badgeTop = _st.nameTop;
	const auto badgeBottom = _st.nameTop + _name->height();
	const auto margins = LargeCustomEmojiMargins();

	if (_botVerify) {
		_botVerify->move(nameLeft - margins.left(), badgeTop, badgeBottom);
		if (const auto widget = _botVerify->widget()) {
			const auto skip = widget->width() + st::infoVerifiedCheckPosition.x();
			nameLeft += skip;
			nameWidth -= skip;
		}
	}
	_name->resizeToNaturalWidth(nameWidth);
	_name->moveToLeft(nameLeft, _st.nameTop, newWidth);
	const auto badgeLeft = nameLeft + _name->width();
	if (_badge) {
		_badge->move(badgeLeft, badgeTop, badgeBottom);
	}
	if (_verified) {
		_verified->move(badgeLeft + (badgeWidget ? badgeWidget->width() : 0), badgeTop, badgeBottom);
	}
}

void ClassicCover::refreshStatusGeometry(int newWidth) {
	if (const auto rating = _starsRating.get()) {
		rating->moveTo(_st.starsRatingLeft, _st.starsRatingTop);
	}
	const auto statusLeft = _st.statusLeft + _statusShift.current();
	auto statusWidth = newWidth - statusLeft - _st.rightSkip;
	_status->resizeToNaturalWidth(statusWidth);
	_status->moveToLeft(statusLeft, _st.statusTop, newWidth);
	const auto left = statusLeft + _status->textMaxWidth();
	const auto &buttonStyle = _showLastSeen->st();
	const auto buttonTop = _st.statusTop + _status->st().margin.top()
		+ _status->st().style.font->ascent
		- buttonStyle.padding.top() - buttonStyle.textTop
		- buttonStyle.style.font->ascent;
	_showLastSeen->moveToLeft(left + _st.showLastSeenPosition.x(), buttonTop, newWidth);
}

void ClassicCover::hideBadgeTooltip() {
	_badgeTooltipHide.cancel();
	if (auto old = base::take(_badgeTooltip)) {
		const auto raw = old.get();
		_badgeOldTooltips.push_back(std::move(old));

		raw->fade(false);
		raw->shownValue() | rpl::filter(
			!rpl::mappers::_1
		) | rpl::on_next([=] {
			const auto i = ranges::find(_badgeOldTooltips, raw, &std::unique_ptr<BadgeTooltip>::get);
			if (i != end(_badgeOldTooltips)) {
				_badgeOldTooltips.erase(i);
			}
		}, raw->lifetime());
	}
}

void ClassicCover::setupUniqueBadgeTooltip() {
	if (!_badge) {
		return;
	}
	base::timer_once(kWaitBeforeGiftBadge) | rpl::then(_badge->updated()) | rpl::on_next([=] {
		const auto widget = _badge->widget();
		const auto &content = _badgeContent.current();
		const auto &collectible = content.emojiStatusId.collectible;
		const auto premium = (content.badge == BadgeType::Premium);
		const auto id = (collectible && widget && premium) ? collectible->id : uint64();
		if (_badgeCollectibleId == id) {
			return;
		}
		hideBadgeTooltip();
		if (!collectible) {
			return;
		}
		const auto parent = _parentForTooltip ? _parentForTooltip() : _controller->window().widget()->bodyWidget();
		_badgeTooltip = std::make_unique<BadgeTooltip>(parent, collectible, widget);
		const auto raw = _badgeTooltip.get();
		raw->fade(true);
		_badgeTooltipHide.callOnce(kGiftBadgeGlares * raw->glarePeriod() - st::infoGiftTooltip.duration * 1.5);
	}, lifetime());

	if (const auto raw = _badgeTooltip.get()) {
		raw->finishAnimating();
	}
}

} // namespace Info::Profile
