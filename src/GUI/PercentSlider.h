#pragma once

#include <QWidget>
#include <QPropertyAnimation>

class PercentSlider : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal handlePos READ handlePos WRITE setHandlePos)

public:
    explicit PercentSlider(QWidget *parent = nullptr);

    int value() const;
    void setValue(int value);

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    QSize sizeHint() const override;

private:
    int m_value = 0;          // 0–100
    qreal m_handlePos = 0.0;  // 0.0–1.0
    QPropertyAnimation *m_anim;

    qreal handlePos() const;
    void setHandlePos(qreal pos);

    void updateFromPosition(int x, bool animated);
};
