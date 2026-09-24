/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/effects/outline_segments.h"

#include "ui/style/style_radius.h"
#include <cmath>

namespace Ui {
namespace {

void PaintRectArc(QPainter &p, QRectF rect, int from, int length) {
	const auto pointAt = [&](float64 angle) {
		const auto radians = angle * (2. * std::acos(-1.) / arc::kFullLength);
		const auto x = std::cos(radians);
		const auto y = -std::sin(radians);
		const auto scale = std::max(std::abs(x), std::abs(y));
		return rect.center() + QPointF(x * rect.width() / (2. * scale), y * rect.height() / (2. * scale));
	};
	const auto quarter = arc::kQuarterLength;
	const auto firstCorner = (std::floor((from - quarter / 2.) / quarter) + 1.) * quarter + quarter / 2.;
	auto path = QPainterPath();
	path.moveTo(pointAt(from));
	for (auto angle = firstCorner; angle < from + length; angle += quarter) {
		path.lineTo(pointAt(angle));
	}
	path.lineTo(pointAt(from + length));
	p.drawPath(path);
}

} // namespace

void PaintOutlineSegments(
		QPainter &p,
		QRectF ellipse,
		const std::vector<OutlineSegment> &segments,
		float64 fromFullProgress) {
	Expects(!segments.empty());

	p.setBrush(Qt::NoBrush);
	const auto count = std::min(int(segments.size()), kOutlineSegmentsMax);
	if (count == 1) {
		p.setPen(QPen(segments.front().brush, segments.front().width, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
		p.drawRect(ellipse);
		return;
	}
	const auto small = 160;
	const auto full = arc::kFullLength;
	const auto separator = (full > 1.1 * small * count)
		? small
		: (full / (count * 1.1));
	const auto left = full - (separator * count);
	const auto length = left / float64(count);
	const auto spin = separator * (1. - fromFullProgress);

	auto start = 0. + (arc::kQuarterLength + (separator / 2)) + (3. * spin);
	auto pen = QPen(
		segments.back().brush,
		segments.back().width,
		Qt::SolidLine,
		Qt::FlatCap,
		Qt::MiterJoin);
	p.setPen(pen);
	for (auto i = 0; i != count;) {
		const auto &segment = segments[count - (++i)];
		if (!segment.width) {
			start += length + separator;
			continue;
		} else if (pen.brush() != segment.brush
			|| pen.widthF() != segment.width) {
			pen = QPen(
				segment.brush,
				segment.width,
				Qt::SolidLine,
				Qt::FlatCap,
				Qt::MiterJoin);
			p.setPen(pen);
		}
		const auto from = int(base::SafeRound(start));
		const auto till = start + length;
		auto added = spin;
		for (; i != count;) {
			start += length + separator;
			const auto &next = segments[count - (++i)];
			if (next.width) {
				--i;
				break;
			}
			added += (separator + length) * (1. - fromFullProgress);
		}
		PaintRectArc(p, ellipse, from, int(base::SafeRound(till + added)) - from);
	}
}

void PaintOutlineSegments(
		QPainter &p,
		QRectF rect,
		float64 radius,
		const std::vector<OutlineSegment> &segments) {
	Expects(!segments.empty());

	p.setBrush(Qt::NoBrush);
	const auto count = std::min(int(segments.size()), kOutlineSegmentsMax);
	if (count == 1 || true) {
		p.setPen(QPen(segments.back().brush, segments.back().width));
		p.drawRoundedRect(rect, style::CornerRadius(radius), style::CornerRadius(radius));
		return;
	}
}

QLinearGradient UnreadStoryOutlineGradient(
		QRectF rect,
		const QColor &c1,
		const QColor &c2) {
	auto result = QLinearGradient(rect.topRight(), rect.bottomLeft());
	result.setStops({ { 0., c1 }, { 1., c2 } });
	return result;
}

QLinearGradient UnreadStoryOutlineGradient(QRectF rect) {
	return UnreadStoryOutlineGradient(
		std::move(rect),
		st::groupCallLive1->c,
		st::groupCallMuted1->c);
}

} // namespace Ui
