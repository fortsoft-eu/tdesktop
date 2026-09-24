/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "chat_helpers/tabbed_panel.h"

#include "ui/style/style_classic.h"
#include "ui/widgets/shadow.h"
#include "ui/image/image_prepare.h"
#include "ui/ui_utility.h"
#include "chat_helpers/tabbed_selector.h"
#include "window/window_session_controller.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/stickers/data_stickers.h"
#include "core/application.h"
#include "base/options.h"
#include "lang/lang_keys.h"
#include "styles/style_chat_helpers.h"

#include <QtGui/QCloseEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QScreen>
#include <QtGui/QWindow>

namespace ChatHelpers {
namespace {

base::options::toggle TabbedPanelShowOnClick({
	.id = kOptionTabbedPanelShowOnClick,
	.name = "Show tabbed panel by click",
	.description = "Show Emoji / Stickers / GIFs panel only after a click.",
	.defaultValue = true,
});

} // namespace

const char kOptionTabbedPanelShowOnClick[] = "tabbed-panel-show-on-click";

bool ShowPanelOnClick() {
	return TabbedPanelShowOnClick.value();
}

TabbedPanel::TabbedPanel(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<TabbedSelector*> selector)
: TabbedPanel(parent, {
	.regularWindow = controller,
	.nonOwnedSelector = selector,
}) {
}

TabbedPanel::TabbedPanel(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	object_ptr<TabbedSelector> selector)
: TabbedPanel(parent, {
	.regularWindow = controller,
	.ownedSelector = std::move(selector),
}) {
}

TabbedPanel::TabbedPanel(
	QWidget *parent,
	TabbedPanelDescriptor &&descriptor)
: RpWidget(parent)
, _regularWindow(descriptor.regularWindow)
, _separateWindow(descriptor.separateWindow)
, _ownedSelector(std::move(descriptor.ownedSelector))
, _selector(descriptor.nonOwnedSelector
	? descriptor.nonOwnedSelector
	: _ownedSelector.data())
, _heightRatio(st::emojiPanHeightRatio)
, _minContentHeight(st::emojiPanMinHeight)
, _maxContentHeight(st::emojiPanMaxHeight)
, _shadow(_selector->st().showAnimation.shadow) {
	Expects(_selector != nullptr);
	_selector->setParent(this);
	if (_separateWindow) {
		applySeparateWindowStyle();
		setWindowTitle(descriptor.windowTitle.isEmpty()
			? (tr::lng_switch_emoji(tr::now)
				+ u" / "_q + tr::lng_switch_stickers(tr::now)
				+ u" / "_q + tr::lng_switch_gifs(tr::now))
			: descriptor.windowTitle);
	}
	_selector->setRoundRadius(st::emojiPanRadius);
	_selector->setAfterShownCallback([=](SelectorTab tab) {
		if (_regularWindow) {
			_regularWindow->enableGifPauseReason(_selector->level());
		}
		_pauseAnimations.fire(true);
	});
	_selector->setBeforeHidingCallback([=](SelectorTab tab) {
		if (_regularWindow) {
			_regularWindow->disableGifPauseReason(_selector->level());
		}
		_pauseAnimations.fire(false);
	});
	_selector->showRequests(
	) | rpl::on_next([=] {
		showFromSelector();
	}, lifetime());

	resize(
		QRect(0, 0, st::emojiPanWidth, st::emojiPanMaxHeight).marginsAdded(
			innerPadding()).size());

	if (_separateWindow) {
		setFixedWidth(width());
		setMinimumHeight(_minContentHeight
			+ _selector->marginTop() + _selector->marginBottom()
			+ innerPadding().top() + innerPadding().bottom());
	}

	_contentMaxHeight = st::emojiPanMaxHeight;
	_contentHeight = _contentMaxHeight;

	_selector->resize(st::emojiPanWidth, _contentHeight);
	_selector->move(innerRect().topLeft());

	_selector->cancelled(
	) | rpl::on_next([=] {
		hideAnimated();
	}, lifetime());

	if (_regularWindow) {
		_regularWindow->session().data().stickers().gifWithCaptionSent(
		) | rpl::on_next([=] {
			hideAnimated();
		}, lifetime());
	}

	_selector->slideFinished(
	) | rpl::on_next([=] {
		InvokeQueued(this, [=] {
			if (_hideAfterSlide) {
				startOpacityAnimation(true);
			}
		});
	}, lifetime());

	setAttribute(Qt::WA_OpaquePaintEvent, false);

	hideChildren();
	hide();
}

not_null<TabbedSelector*> TabbedPanel::selector() const {
	return _selector;
}

rpl::producer<bool> TabbedPanel::pauseAnimations() const {
	return _pauseAnimations.events();
}

bool TabbedPanel::isSelectorStolen() const {
	return (_selector->parent() != this);
}

void TabbedPanel::selectorWasStolen() {
	if (!isSelectorStolen()) {
		return;
	}
	_hideAfterSlide = false;
	_hiding = false;
	_shouldFinishHide = false;
	_a_show.stop();
	_a_opacity.stop();
	_showAnimation.reset();
	_cache = QPixmap();
	hide();
}

void TabbedPanel::moveBottomRight(int bottom, int right) {
	if (isSelectorStolen()) {
		return;
	}
	const auto isNew = (_bottom != bottom || _right != right);
	_bottom = bottom;
	_right = right;
	// If the panel is already shown, update the position.
	if (!isHidden() && isNew) {
		moveHorizontally();
	} else {
		updateContentHeight();
	}
}

void TabbedPanel::moveTopRight(int top, int right) {
	if (isSelectorStolen()) {
		return;
	}
	const auto isNew = (_top != top || _right != right);
	_top = top;
	_right = right;
	// If the panel is already shown, update the position.
	if (!isHidden() && isNew) {
		moveHorizontally();
	} else {
		updateContentHeight();
	}
}

void TabbedPanel::setDesiredHeightValues(
		float64 ratio,
		int minHeight,
		int maxHeight) {
	_heightRatio = ratio;
	_minContentHeight = minHeight;
	_maxContentHeight = maxHeight;
	updateContentHeight();
}

void TabbedPanel::setShowAnimationOrigin(Ui::PanelAnimation::Origin origin) {
	_showAnimationOrigin = origin;
}

void TabbedPanel::setDropDown(bool dropDown) {
	selector()->setDropDown(dropDown);
	_dropDown = dropDown;
}

void TabbedPanel::updateContentHeight() {
	if (isSelectorStolen()) {
		return;
	}
	auto addedHeight = innerPadding().top() + innerPadding().bottom();
	auto marginsHeight = _selector->marginTop() + _selector->marginBottom();
	auto availableHeight = _dropDown
		? (parentWidget()->height() - _top - marginsHeight)
		: (_bottom - marginsHeight);
	auto wantedContentHeight = qRound(_heightRatio * availableHeight)
		- addedHeight;
	auto contentHeight = marginsHeight + std::clamp(
		wantedContentHeight,
		_minContentHeight,
		_maxContentHeight);
	auto resultTop = _dropDown
		? _top
		: (_bottom - addedHeight - contentHeight);
	if (contentHeight != _contentHeight
		&& (!_separateWindow || isHidden())) {
		_contentHeight = contentHeight;
		resize(QRect(0, 0, innerRect().width(), _contentHeight).marginsAdded(innerPadding()).size());
		_selector->resize(innerRect().width(), _contentHeight);
	}
	if (_separateWindow) {
		const auto frameSize = frameGeometry().size();
		const auto outerWidth = std::max(frameSize.width(), width());
		const auto outerHeight = std::max(frameSize.height(), height());
		const auto left = std::max(_right - outerWidth, 0);
		const auto local = style::rtlrect(
			left,
			_dropDown ? resultTop : (_bottom - outerHeight),
			outerWidth,
			outerHeight,
			parentWidget()->width());
		auto global = parentWidget()->mapToGlobal(local.topLeft());
		const auto screen = QGuiApplication::screenAt(global);
		const auto available = (screen ? screen : parentWidget()->screen())
			->availableGeometry();
		const auto maxX = available.x()
			+ std::max(available.width() - outerWidth, 0);
		const auto maxY = available.y()
			+ std::max(available.height() - outerHeight, 0);
		global.setX(std::clamp(global.x(), available.x(), maxX));
		global.setY(std::clamp(global.y(), available.y(), maxY));
		move(global);
	} else {
		moveHorizontally();
		move(x(), resultTop);
	}

	update();
}

void TabbedPanel::resizeEvent(QResizeEvent *e) {
	RpWidget::resizeEvent(e);
	if (isSelectorStolen()) {
		return;
	}
	_contentHeight = innerRect().height();
	_selector->setGeometry(innerRect());
	if (_showAnimation || _a_opacity.animating()) {
		_a_show.stop();
		_showAnimation.reset();
		_a_opacity.stop();
		_cache = QPixmap();
		if (_hiding) {
			_hiding = false;
			hideFinished();
		} else if (isVisible()) {
			showChildren();
			_selector->afterShown();
		}
	}
}

void TabbedPanel::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);

	// This call can finish _a_show animation and destroy _showAnimation.
	auto opacityAnimating = _a_opacity.animating();

	auto showAnimating = _a_show.animating();
	if (_showAnimation && !showAnimating) {
		_showAnimation.reset();
		if (!opacityAnimating) {
			showChildren();
			_selector->afterShown();
		}
	}

	if (showAnimating) {
		Assert(_showAnimation != nullptr);
		if (auto opacity = _a_opacity.value(_hiding ? 0. : 1.)) {
			_showAnimation->paintFrame(p, 0, 0, width(), _a_show.value(1.), opacity);
		}
	} else if (opacityAnimating) {
		p.setOpacity(_a_opacity.value(_hiding ? 0. : 1.));
		p.drawPixmap(0, 0, _cache);
	} else if (_hiding || isHidden()) {
		hideFinished();
	} else {
		if (!_cache.isNull()) _cache = QPixmap();
		_shadow.paint(p, innerRect(), st::emojiPanRadius);
	}
}

void TabbedPanel::moveHorizontally() {
	if (_separateWindow) {
		updateContentHeight();
		return;
	}
	const auto padding = innerPadding();
	const auto useWidth = innerRect().width()
		+ padding.left()
		+ padding.right();
	const auto right = std::max(
		parentWidget()->width() - std::max(_right, useWidth),
		0);
	moveToRight(right, y());
}

void TabbedPanel::applySeparateWindowStyle() {
	Ui::SetClassicSettingsStyle(this);
	setWindowFlags(Qt::WindowFlags(Qt::Tool)
		| Qt::WindowTitleHint
		| Qt::WindowSystemMenuHint
		| Qt::WindowCloseButtonHint);
	setAttribute(Qt::WA_QuitOnClose, false);
	setFocusPolicy(Qt::StrongFocus);
}

void TabbedPanel::activateSeparateWindow() {
	if (!_separateWindow || testAttribute(Qt::WA_ShowWithoutActivating)) {
		return;
	}
	if (!isWindow() || windowType() != Qt::Tool) {
		applySeparateWindowStyle();
		show();
	}
	updateContentHeight();
	raise();
	activateWindow();
	setFocus(Qt::ActiveWindowFocusReason);
	if (const auto handle = windowHandle()) {
		handle->requestActivate();
	}
	_selector->focusSearch();
	InvokeQueued(this, [=] {
		if (!isHidden()) {
			activateWindow();
			if (const auto handle = windowHandle()) {
				handle->requestActivate();
			}
			_selector->focusSearch();
		}
	});
}

void TabbedPanel::closeEvent(QCloseEvent *e) {
	e->ignore();
	hideFast();
}

void TabbedPanel::keyPressEvent(QKeyEvent *e) {
	if (_separateWindow && e->key() == Qt::Key_Escape) {
		e->accept();
		hideFast();
		return;
	}
	RpWidget::keyPressEvent(e);
}

void TabbedPanel::otherEnter() {
	showAnimated();
}

void TabbedPanel::hideFast() {
	if (isSelectorStolen()) {
		selectorWasStolen();
		return;
	}
	if (isHidden()) return;

	if (_selector && !_selector->isHidden()) {
		_selector->beforeHiding();
	}
	_hiding = false;
	_a_opacity.stop();
	hideFinished();
}

void TabbedPanel::opacityAnimationCallback() {
	update();
	if (!_a_opacity.animating()) {
		if (_hiding) {
			_hiding = false;
			hideFinished();
		} else if (!_a_show.animating()) {
			showChildren();
			_selector->afterShown();
		}
	}
}

void TabbedPanel::prepareCacheFor(bool hiding) {
	if (_a_opacity.animating()) {
		_hiding = hiding;
		return;
	}

	auto showAnimation = base::take(_a_show);
	auto showAnimationData = base::take(_showAnimation);
	_hiding = false;
	showChildren();

	_cache = Ui::GrabWidget(this);

	_a_show = base::take(showAnimation);
	_showAnimation = base::take(showAnimationData);
	_hiding = hiding;
	if (_a_show.animating()) {
		hideChildren();
	}
}

void TabbedPanel::startOpacityAnimation(bool hiding) {
	if (_selector && !_selector->isHidden()) {
		_selector->beforeHiding();
	}
	prepareCacheFor(hiding);
	hideChildren();
	_a_opacity.start(
		[=] { opacityAnimationCallback(); },
		_hiding ? 1. : 0.,
		_hiding ? 0. : 1.,
		st::emojiPanDuration);
}

void TabbedPanel::startShowAnimation() {
	if (!_a_show.animating()) {
		auto image = grabForAnimation();

		const auto origin = _showAnimationOrigin.value_or(_dropDown
			? Ui::PanelAnimation::Origin::TopRight
			: Ui::PanelAnimation::Origin::BottomRight);
		_showAnimation = std::make_unique<Ui::PanelAnimation>(
			_selector->st().showAnimation,
			origin);
		auto inner = innerRect();
		_showAnimation->setFinalImage(
			std::move(image),
			QRect(
				inner.topLeft() * style::DevicePixelRatio(),
				inner.size() * style::DevicePixelRatio()),
			st::emojiPanRadius);
		_showAnimation->setCornerMasks(Images::CornersMask(st::emojiPanRadius));
		_showAnimation->start();
	}
	hideChildren();
	_a_show.start([this] { update(); }, 0., 1., st::emojiPanShowDuration);
}

QImage TabbedPanel::grabForAnimation() {
	auto cache = base::take(_cache);
	auto opacityAnimation = base::take(_a_opacity);
	auto showAnimationData = base::take(_showAnimation);
	auto showAnimation = base::take(_a_show);

	showChildren();
	Ui::SendPendingMoveResizeEvents(this);

	auto result = QImage(
		size() * style::DevicePixelRatio(),
		QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(style::DevicePixelRatio());
	result.fill(Qt::transparent);
	if (_selector) {
		QPainter p(&result);
		Ui::RenderWidget(p, _selector, _selector->pos());
	}

	_a_show = base::take(showAnimation);
	_showAnimation = base::take(showAnimationData);
	_a_opacity = base::take(opacityAnimation);
	_cache = base::take(cache);

	return result;
}

void TabbedPanel::hideAnimated() {
	if (isSelectorStolen()) {
		selectorWasStolen();
		return;
	}
	if (isHidden() || _hiding) {
		return;
	}

	if (_selector->isSliding()) {
		_hideAfterSlide = true;
	} else {
		startOpacityAnimation(true);
	}

	// There is no reason to worry about the message scheduling box
	// while it moves the user to the separate scheduled section.
	_shouldFinishHide = _selector->hasMenu();
}

void TabbedPanel::toggleAnimated() {
	if (isHidden() || _hiding || _hideAfterSlide) {
		showAnimated();
	} else {
		hideAnimated();
	}
}

void TabbedPanel::hideFinished() {
	hide();
	_a_show.stop();
	_showAnimation.reset();
	_cache = QPixmap();
	_hiding = false;
	_shouldFinishHide = false;
	_selector->hideFinished();
}

void TabbedPanel::showAnimated() {
	if (isSelectorStolen()) {
		return;
	}
	_hideAfterSlide = false;
	showStarted();
}

void TabbedPanel::showStarted() {
	if (_shouldFinishHide) {
		return;
	}
	if (isHidden()) {
		_selector->showStarted();
		moveHorizontally();
		raise();
		show();
		if (_separateWindow) {
			activateSeparateWindow();
		}
		startShowAnimation();
	} else if (_hiding) {
		startOpacityAnimation(false);
	}
}

bool TabbedPanel::eventFilter(QObject *obj, QEvent *e) {
	if (isSelectorStolen() || TabbedPanelShowOnClick.value()) {
		return false;
	} else if (e->type() == QEvent::Enter) {
		otherEnter();
	}
	return false;
}

void TabbedPanel::showFromSelector() {
	if (isSelectorStolen()) {
		return;
	}
	if (isHidden()) {
		moveHorizontally();
		startShowAnimation();
		show();
		if (_separateWindow) {
			activateSeparateWindow();
		}
	}
	showChildren();
	showAnimated();
}

style::margins TabbedPanel::innerPadding() const {
	return {};
}

QRect TabbedPanel::innerRect() const {
	return rect().marginsRemoved(innerPadding());
}

bool TabbedPanel::overlaps(const QRect &globalRect) const {
	if (isHidden() || !_cache.isNull()) return false;

	auto testRect = QRect(mapFromGlobal(globalRect.topLeft()), globalRect.size());
	auto inner = innerRect();
	const auto radius = st::emojiPanRadius;
	return inner.marginsRemoved(QMargins(radius, 0, radius, 0)).contains(testRect)
		|| inner.marginsRemoved(QMargins(0, radius, 0, radius)).contains(testRect);
}

TabbedPanel::~TabbedPanel() {
	hideFast();
	if (!_ownedSelector && _regularWindow) {
		_regularWindow->takeTabbedSelectorOwnershipFrom(this);
	}
}

} // namespace ChatHelpers
