#pragma once

#include <QWidget>
#include <QPropertyAnimation>

class SwitchToggle : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal offset READ offset WRITE setOffset)

public:
    explicit SwitchToggle(QWidget *parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked);
    void setEnabled(bool enabled);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    QSize sizeHint() const override;

private:
    bool m_checked = false;
    bool m_enabled = true;
    qreal m_offset = 0.0;
    QPropertyAnimation *m_anim;

    qreal offset() const;
    void setOffset(qreal value);
};
