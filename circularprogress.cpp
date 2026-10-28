#include "circularprogress.h"

#include <QPainter>
#include <QPen>
#include <QtMath>
#include <algorithm>

CircularProgress::CircularProgress(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(12, 12);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void CircularProgress::setProgress(int value)
{
    value = qBound(0, value, 100);

    setToolTip(QString("%1%").arg(value));

    if (m_progress == value)
        return;

    m_progress = value;
    update();
}

QSize CircularProgress::sizeHint() const
{
    return QSize(24, 24);
}

void CircularProgress::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Adjust these values to customize the indicator.
    const qreal lineWidth = 3.0;

    const qreal side = qMin(width(), height());
    const qreal margin = lineWidth / 2.0 + 1.0;

    const QRectF circleRect(
        (width() - side) / 2.0 + margin,
        (height() - side) / 2.0 + margin,
        side - 2.0 * margin,
        side - 2.0 * margin
        );

    // Background track.
    QPen backgroundPen(
        palette().color(QPalette::Mid),
        lineWidth
        );

    backgroundPen.setCapStyle(Qt::RoundCap);
    painter.setPen(backgroundPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(circleRect, 0, 360 * 16);

    if (m_progress <= 0)
        return;

    // Progress arc.
    QPen progressPen(
        palette().color(QPalette::Highlight),
        lineWidth
        );

    progressPen.setCapStyle(Qt::RoundCap);
    painter.setPen(progressPen);

    // Start at the top and move clockwise.
    const int spanAngle = -qRound(m_progress * 360.0 * 16.0 / 100.0);

    painter.drawArc(circleRect, 90 * 16, spanAngle);
}
