/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/controls/jump_down_button.h"

#include "ui/effects/ripple_animation.h"
#include "ui/style/style_classic.h"
#include "ui/unread_badge_paint.h"
#include "styles/style_chat_helpers.h"

namespace Ui {
namespace {

UnreadBadgeStyle JumpBadgeStyle() {
	auto result = UnreadBadgeStyle();
	result.align = style::al_left;
	result.font = st::historyToDownBadgeFont;
	result.size = st::historyToDownBadgeSize;
	result.sizeId = UnreadBadgeSize::HistoryToDown;
	return result;
}

QRegion JumpButtonMask(const style::TwoIconButton &st, int width, int badgeWidth) {
	auto result = QRegion(style::rtlrect(
		width - st.width,
		st.height - st::historyToDownButtonSize,
		st.width,
		st::historyToDownButtonSize,
		width));
	if (badgeWidth > 0) {
		result += QRect((width - badgeWidth) / 2, 0, badgeWidth, st::historyToDownBadgeSize);
	}
	return result;
}

} // namespace

JumpDownButton::JumpDownButton(
	QWidget *parent,
	const style::TwoIconButton &st,
	Context context)
: RippleButton(parent, st.ripple)
, _st(st)
, _context(context) {
	resize(_st.width, _st.height);
	setMask(JumpButtonMask(_st, width(), 0));
	setCursor(style::cur_default);

	hide();
}

QImage JumpDownButton::prepareRippleMask() const {
	return Ui::RippleAnimation::RectMask(
		QSize(_st.rippleAreaSize, _st.rippleAreaSize));
}

QPoint JumpDownButton::prepareRippleStartPosition() const {
	return mapFromGlobal(QCursor::pos()) - _st.rippleAreaPosition;
}

void JumpDownButton::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);

	const auto over = isOver();
	const auto down = isDown();
	const auto buttonTop = height() - st::historyToDownButtonSize;
	const auto buttonLeft = style::RightToLeft() ? 0 : width() - _st.width;
	const auto buttonRect = QRect(
		buttonLeft,
		buttonTop,
		_st.width,
		st::historyToDownButtonSize);
	PaintClassicButton(p, buttonRect, this, down);
	p.save();
	p.translate(buttonRect.topLeft()
		+ (_context == Context::MessageViewport
			? ClassicMessageButtonContentOffset(down, true)
			: ClassicButtonContentOffset(this, down)));
	((over || down)
		? _st.iconAboveOver
		: _st.iconAbove).paintInCenter(
			p,
			QRect(QPoint(), buttonRect.size()),
			st::classicMenuText->c);
	p.restore();
	if (_unreadCount > 0) {
		const auto text = QString::number(_unreadCount);
		const auto badgeStyle = JumpBadgeStyle();
		const auto badgeWidth = CountUnreadBadgeSize(text, badgeStyle).width();
		const auto left = (width() - badgeWidth) / 2;
		PaintUnreadBadge(p, text, left, 0, badgeStyle);
	}
}

void JumpDownButton::setUnreadCount(int unreadCount) {
	if (_unreadCount != unreadCount) {
		_unreadCount = unreadCount;
		const auto badgeWidth = _unreadCount > 0 
			? CountUnreadBadgeSize(QString::number(_unreadCount), JumpBadgeStyle()).width()
			: 0;
		const auto newWidth = std::max(_st.width, badgeWidth);
		setMask(JumpButtonMask(_st, newWidth, badgeWidth));
		resize(newWidth, _st.height);
		update();
	}
}

} // namespace Ui
