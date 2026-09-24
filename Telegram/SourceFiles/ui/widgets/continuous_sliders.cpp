/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/continuous_sliders.h"

#include "ui/style/style_radius.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "base/timer.h"
#include "base/weak_qptr.h"
#include "base/platform/base_platform_info.h"
#include "styles/style_widgets.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>

namespace Ui {
namespace {

constexpr auto kByWheelFinishedTimeout = 1000;
constexpr auto kNativeSliderSteps = 1000000;

} // namespace

ContinuousSlider::ContinuousSlider(QWidget *parent) : RpWidget(parent) {
	_native = new QSlider(Qt::Horizontal, this);
	if (const auto nativeStyle = QStyleFactory::create(u"Windows"_q)) {
		nativeStyle->setParent(_native);
		_native->setStyle(nativeStyle);
	}
	_native->setRange(0, kNativeSliderSteps);
	_native->setSingleStep(kNativeSliderSteps / 100);
	_native->setPageStep(kNativeSliderSteps / 10);
	auto palette = _native->palette();
	palette.setColor(QPalette::Button, QColor(212, 208, 200));
	palette.setColor(QPalette::Window, QColor(212, 208, 200));
	palette.setColor(QPalette::Light, Qt::white);
	palette.setColor(QPalette::Dark, QColor(128, 128, 128));
	palette.setColor(QPalette::Shadow, QColor(64, 64, 64));
	_native->setPalette(palette);
	setFocusProxy(_native);
	setMinimumHeight(_native->minimumSizeHint().height());
	QObject::connect(_native, &QSlider::sliderPressed, this, [=] {
		_mouseDown = true;
		_downValue = _value;
	});
	QObject::connect(_native, &QSlider::valueChanged, this, [=](int value) {
		auto adjusted = value / float64(kNativeSliderSteps);
		if (_adjustCallback) {
			adjusted = std::clamp(_adjustCallback(adjusted), 0., 1.);
		}
		{
			const auto blocker = QSignalBlocker(_native);
			_native->setValue(qRound(adjusted * kNativeSliderSteps));
		}
		_value = _downValue = adjusted;
		const auto weak = base::make_weak(this);
		if (_changeProgressCallback) {
			_changeProgressCallback(adjusted);
		}
		if (weak && !_mouseDown && _changeFinishedCallback) {
			_changeFinishedCallback(adjusted);
		}
	});
	QObject::connect(_native, &QSlider::sliderReleased, this, [=] {
		_mouseDown = false;
		_value = _downValue;
		if (_changeFinishedCallback) {
			_changeFinishedCallback(_value);
		}
	});
	_native->installEventFilter(this);
	_native->show();
}

bool ContinuousSlider::eventFilter(QObject *object, QEvent *event) {
	if (object == _native && event->type() == QEvent::Wheel) {
		wheelEvent(static_cast<QWheelEvent*>(event));
		return true;
	} else if (object == _native && event->type() == QEvent::KeyPress) {
		keyPressEvent(static_cast<QKeyEvent*>(event));
		return true;
	}
	return RpWidget::eventFilter(object, event);
}

void ContinuousSlider::setDirection(Direction direction) {
	_direction = direction;
	_native->setOrientation(isHorizontal() ? Qt::Horizontal : Qt::Vertical);
	setMinimumSize(isHorizontal()
		? QSize(0, _native->minimumSizeHint().height())
		: QSize(_native->minimumSizeHint().width(), 0));
}

void ContinuousSlider::resizeEvent(QResizeEvent *e) {
	_native->setGeometry(rect());
}

void ContinuousSlider::setDisabled(bool disabled) {
	if (_disabled != disabled) {
		_disabled = disabled;
		_native->setEnabled(!disabled);
		setCursor(_disabled ? style::cur_default : style::cur_pointer);
		update();
	}
}

void ContinuousSlider::setMoveByWheel(bool move) {
	if (move != moveByWheel()) {
		if (move) {
			_byWheelFinished = std::make_unique<base::Timer>([=] {
				if (_changeFinishedCallback) {
					_changeFinishedCallback(getCurrentValue());
				}
			});
		} else {
			_byWheelFinished = nullptr;
		}
	}
}

QRect ContinuousSlider::getSeekRect() const {
	const auto decrease = getSeekDecreaseSize();
	return isHorizontal()
		? QRect(decrease.width() / 2, 0, width() - decrease.width(), height())
		: QRect(0, decrease.height() / 2, width(), height() - decrease.width());
}

float64 ContinuousSlider::value() const {
	return getCurrentValue();
}

void ContinuousSlider::setValue(float64 value) {
	setValue(value, -1);
}

void ContinuousSlider::setValue(float64 value, float64 receivedTill) {
	if (_value != value || _receivedTill != receivedTill) {
		_value = value;
		_receivedTill = receivedTill;
		const auto blocker = QSignalBlocker(_native);
		_native->setValue(qRound(
			std::clamp(getCurrentValue(), 0., 1.) * kNativeSliderSteps));
		update();
	}
}

void ContinuousSlider::setFadeOpacity(float64 opacity) {
	_fadeOpacity = opacity;
	auto effect = qobject_cast<QGraphicsOpacityEffect*>(_native->graphicsEffect());
	if (opacity < 1.) {
		if (!effect) {
			effect = new QGraphicsOpacityEffect(_native);
			_native->setGraphicsEffect(effect);
		}
		effect->setOpacity(opacity);
	} else if (effect) {
		_native->setGraphicsEffect(nullptr);
	}
	update();
}

void ContinuousSlider::mouseMoveEvent(QMouseEvent *e) {
	if (_mouseDown) {
		updateDownValueFromPos(e->pos());
	}
}

float64 ContinuousSlider::computeValue(const QPoint &pos) const {
	const auto seekRect = myrtlrect(getSeekRect());
	const auto result = isHorizontal() ?
		(pos.x() - seekRect.x()) / float64(seekRect.width()) :
		(1. - (pos.y() - seekRect.y()) / float64(seekRect.height()));
	const auto snapped = std::clamp(result, 0., 1.);
	return _adjustCallback ? _adjustCallback(snapped) : snapped;
}

void ContinuousSlider::mousePressEvent(QMouseEvent *e) {
	_mouseDown = true;
	_downValue = computeValue(e->pos());
	update();
	if (_changeProgressCallback) {
		_changeProgressCallback(_downValue);
	}
}

void ContinuousSlider::mouseReleaseEvent(QMouseEvent *e) {
	if (_mouseDown) {
		_mouseDown = false;
		const auto weak = base::make_weak(this);
		if (_changeFinishedCallback) {
			_changeFinishedCallback(_downValue);
		}
		if (!weak) {
			return;
		}
		_value = _downValue;
		update();
	}
}

void ContinuousSlider::wheelEvent(QWheelEvent *e) {
	if (_mouseDown || !moveByWheel()) {
		return;
	}
	constexpr auto step = static_cast<int>(QWheelEvent::DefaultDeltasPerStep);
	constexpr auto coef = 1. / (step * 10.);

	auto deltaX = e->angleDelta().x(), deltaY = e->angleDelta().y();
	if (Platform::IsMac()) {
		deltaY *= -1;
	} else {
		deltaX *= -1;
	}
	auto delta = (qAbs(deltaX) > qAbs(deltaY)) ? deltaX : deltaY;
	auto finalValue = std::clamp(_value + delta * coef, 0., 1.);
	setValue(finalValue);
	const auto weak = base::make_weak(this);
	if (_changeProgressCallback) {
		_changeProgressCallback(finalValue);
	}
	if (!weak) {
		return;
	}
	_byWheelFinished->callOnce(kByWheelFinishedTimeout);
}

void ContinuousSlider::keyPressEvent(QKeyEvent *e) {
	const auto changeBy = [&](float64 step) {
		Expects(step != 0.);

		auto steps = 0;
		while (true) {
			++steps;
			auto result = _value + (steps * step);
			const auto stopping = (result <= 0.) || (result >= 1.);
			if (_adjustCallback) {
				result = _adjustCallback(result);
			}
			result = std::clamp(result, 0., 1.);
			if (result != _value || stopping) {
				return result;
			}
		}
	};

	const auto newValue = [&] {
		constexpr auto kSmallStep = 0.01;
		constexpr auto kLargeStep = 0.10;
		switch (e->key()) {
		case Qt::Key_Right:
		case Qt::Key_Up: return changeBy(kSmallStep);
		case Qt::Key_Left:
		case Qt::Key_Down: return changeBy(-kSmallStep);
		case Qt::Key_PageUp: return changeBy(kLargeStep);
		case Qt::Key_PageDown: return changeBy(-kLargeStep);
		case Qt::Key_Home: return changeBy(-1.);
		case Qt::Key_End: return changeBy(1.);
		default: e->ignore();
		}
		return _value;
	}();

	if (_value == newValue) {
		return;
	}
	setValue(newValue);
	const auto weak = base::make_weak(this);
	if (_changeProgressCallback) {
		_changeProgressCallback(_value);
	}
	if (weak && _changeFinishedCallback) {
		_changeFinishedCallback(_value);
	}
	if (!weak) {
		return;
	}
	accessibilityValueChanged();
}

void ContinuousSlider::updateDownValueFromPos(const QPoint &pos) {
	_downValue = computeValue(pos);
	update();
	if (_changeProgressCallback) {
		_changeProgressCallback(_downValue);
	}
}

void ContinuousSlider::enterEventHook(QEnterEvent *e) {
	setOver(true);
}

void ContinuousSlider::leaveEventHook(QEvent *e) {
	setOver(false);
}

void ContinuousSlider::setOver(bool over) {
	if (_over == over) return;

	_over = over;
	auto from = _over ? 0. : 1., to = _over ? 1. : 0.;
	_overAnimation.start([=] { update(); }, from, to, getOverDuration());
}

FilledSlider::FilledSlider(QWidget *parent, const style::FilledSlider &st) : ContinuousSlider(parent)
, _st(st) {
	nativeSlider()->hide();
	setMinimumHeight(0);
}

QSize FilledSlider::getSeekDecreaseSize() const {
	return QSize(0, 0);
}

float64 FilledSlider::getOverDuration() const {
	return _st.duration;
}

void FilledSlider::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	PainterHighQualityEnabler hq(p);

	p.setPen(Qt::NoPen);

	const auto masterOpacity = fadeOpacity();
	const auto disabled = isDisabled();
	const auto over = getCurrentOverFactor();
	const auto lineWidth = _st.lineWidth + ((_st.fullWidth - _st.lineWidth) * over);
	const auto lineWidthRounded = std::floor(lineWidth);
	const auto lineWidthPartial = lineWidth - lineWidthRounded;
	const auto seekRect = getSeekRect();
	const auto value = getCurrentValue();
	const auto from = seekRect.x();
	const auto mid = qRound(from + value * seekRect.width());
	const auto end = from + seekRect.width();
	if (mid > from) {
		p.setOpacity(masterOpacity);
		p.fillRect(from, height() - lineWidthRounded, (mid - from), lineWidthRounded, disabled ? _st.disabledFg : _st.activeFg);
		if (lineWidthPartial > 0.01) {
			p.setOpacity(masterOpacity * lineWidthPartial);
			p.fillRect(from, height() - lineWidthRounded - 1, (mid - from), 1, disabled ? _st.disabledFg : _st.activeFg);
		}
	}
	if (end > mid && over > 0) {
		p.setOpacity(masterOpacity * over);
		p.fillRect(mid, height() - lineWidthRounded, (end - mid), lineWidthRounded, _st.inactiveFg);
		if (lineWidthPartial > 0.01) {
			p.setOpacity(masterOpacity * lineWidthPartial);
			p.fillRect(mid, height() - lineWidthRounded - 1, (end - mid), 1, _st.inactiveFg);
		}
	}
}

MediaSlider::MediaSlider(QWidget *parent, const style::MediaSlider &st) : ContinuousSlider(parent)
, _st(st) {
}

QSize MediaSlider::getSeekDecreaseSize() const {
	return _alwaysDisplayMarker ? _st.seekSize : QSize();
}

float64 MediaSlider::getOverDuration() const {
	return _st.duration;
}

void MediaSlider::disablePaint(bool disabled) {
	_paintDisabled = disabled;
	nativeSlider()->setVisible(!disabled);
}

void MediaSlider::addDivider(float64 atValue, const QSize &size) {
	_dividers.push_back(Divider{ atValue, size });
	_dividerExclusionSize = QSize();
}

void MediaSlider::clearDividers() {
	_dividers.clear();
	_dividerExclusion = QRegion();
	_dividerExclusionSize = QSize();
}

void MediaSlider::rebuildDividerExclusion() {
	const auto currentSize = size();
	if (_dividerExclusionSize == currentSize) {
		return;
	}
	_dividerExclusionSize = currentSize;
	_dividerExclusion = QRegion();
	if (_dividers.empty()) {
		return;
	}
	const auto horizontal = isHorizontal();
	const auto from = 0;
	const auto length = horizontal ? width() : height();
	for (const auto &divider : _dividers) {
		const auto dividerValue = horizontal
			? divider.atValue
			: (1. - divider.atValue);
		const auto dividerMid = int(base::SafeRound(
			from + dividerValue * length));
		const auto &s = divider.size;
		_dividerExclusion += horizontal
			? QRect(
				dividerMid - s.width() / 2,
				(height() - s.height()) / 2,
				s.width(),
				s.height())
			: QRect(
				(width() - s.height()) / 2,
				dividerMid - s.width() / 2,
				s.height(),
				s.width());
	}
}

void MediaSlider::setDividerStyle(DividerStyle style) {
	_dividerStyle = style;
	_dividerExclusionSize = QSize();
	update();
}

void MediaSlider::setColorOverrides(ColorOverrides overrides) {
	_overrides = std::move(overrides);
	update();
}

void MediaSlider::paintEvent(QPaintEvent *e) {
}

} // namespace Ui
