#include "PercentSlider.h"
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>

PercentSlider::PercentSlider(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    m_anim = new QPropertyAnimation(this, "handlePos", this);
    m_anim->setDuration(120);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

QSize PercentSlider::sizeHint() const
{
    return {200, 24};
}

int PercentSlider::value() const
{
    return m_value;
}

void PercentSlider::setValue(int value)
{
    value = qBound(0, value, 100);

    if (m_value == value)
        return;

    m_value = value;
    emit valueChanged(m_value);

    m_anim->stop();
    m_anim->setStartValue(m_handlePos);
    m_anim->setEndValue(m_value / 100.0);
    m_anim->start();
}

qreal PercentSlider::handlePos() const
{
    return m_handlePos;
}

void PercentSlider::setHandlePos(qreal pos)
{
    m_handlePos = qBound(0.0, pos, 1.0);
    update();
}

void PercentSlider::mousePressEvent(QMouseEvent *e)
{
    updateFromPosition(e->pos().x(), true);
}

void PercentSlider::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton)
        updateFromPosition(e->pos().x(), false);
}

void PercentSlider::updateFromPosition(int x, bool animated)
{
    int margin = 10;
    int usableWidth = width() - margin * 2;

    qreal pos = qBound(0.0,
                       (x - margin) / static_cast<qreal>(usableWidth),
                       1.0);

    int newValue = qRound(pos * 100);

    if (newValue == m_value)
        return;

    m_value = newValue;
    emit valueChanged(m_value);

    if (animated)
    {
        m_anim->stop();
        m_anim->setStartValue(m_handlePos);
        m_anim->setEndValue(pos);
        m_anim->start();
    }
    else
    {
        m_handlePos = pos;
        update();
    }
}

void PercentSlider::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int margin = 10;
    int trackHeight = 6;
    int y = height() / 2 - trackHeight / 2;

    // Track background
    QRectF trackBg(margin, y, width() - margin * 2, trackHeight);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#444"));
    p.drawRoundedRect(trackBg, 3, 3);

    // Filled track
    QRectF trackFill = trackBg;
    trackFill.setWidth(trackBg.width() * m_handlePos);
    p.setBrush(QColor("#00c853"));
    p.drawRoundedRect(trackFill, 3, 3);

    // Handle
    qreal handleRadius = 10;
    qreal handleX = trackBg.left() + trackBg.width() * m_handlePos;

    QRectF handleRect(
        handleX - handleRadius,
        height() / 2 - handleRadius,
        handleRadius * 2,
        handleRadius * 2
    );

    p.setBrush(Qt::white);
    p.drawEllipse(handleRect);
}
