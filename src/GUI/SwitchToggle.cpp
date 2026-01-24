#include "SwitchToggle.h"
#include <QPainter>
#include <QMouseEvent>

SwitchToggle::SwitchToggle(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);

    m_anim = new QPropertyAnimation(this, "offset", this);
    m_anim->setDuration(180);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

QSize SwitchToggle::sizeHint() const
{
    return {50, 26};
}

bool SwitchToggle::isChecked() const
{
    return m_checked;
}

void SwitchToggle::setChecked(bool checked)
{
    if (m_checked == checked) return;
    m_checked = checked;
    
    m_anim->stop();
    m_anim->setStartValue(m_offset);
    m_anim->setEndValue(m_checked ? 1.0 : 0.0);
    m_anim->start();
}

void SwitchToggle::setEnabled(bool enabled)
{
    m_enabled = enabled;
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

qreal SwitchToggle::offset() const
{
    return m_offset;
}

void SwitchToggle::setOffset(qreal value)
{
    m_offset = value;
    update();
}

void SwitchToggle::mousePressEvent(QMouseEvent *)
{
    if (!m_enabled) return;
    
    m_checked = !m_checked;
    emit toggled(m_checked);

    m_anim->stop();
    m_anim->setStartValue(m_offset);
    m_anim->setEndValue(m_checked ? 1.0 : 0.0);
    m_anim->start();
}

void SwitchToggle::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF bgRect(0, 0, width(), height());
    qreal radius = height() / 2;

    // Background (dimmed when disabled)
    QColor bgColor = m_checked ? QColor("#0061f1") : QColor("#777");
    if (!m_enabled) bgColor = bgColor.darker(150);
    p.setBrush(bgColor);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bgRect, radius, radius);

    // Handle
    qreal handleSize = height() - 4;
    qreal xPos = 2 + m_offset * (width() - handleSize - 4);

    QRectF handleRect(xPos, 2, handleSize, handleSize);
    QColor handleColor = m_enabled ? Qt::white : QColor("#ccc");
    p.setBrush(handleColor);
    p.drawEllipse(handleRect);
}
