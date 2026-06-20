#pragma once

#include <QTime>
#include <QVector>
#include <QWidget>

class SipSequenceWidget : public QWidget {
    Q_OBJECT

public:
    explicit SipSequenceWidget(QWidget *parent = nullptr);

    void addMessage(const QString &from, const QString &to,
                    const QString &label, const QString &kind);
    void clearMessages();
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Message {
        QString time;
        QString from;
        QString to;
        QString label;
        QString kind;
    };

    QVector<Message> messages;

    int endpointX(const QString &endpoint) const;
    QColor messageColor(const Message &message) const;
};
