#include "sipsequencewidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

namespace {
constexpr int HeaderHeight = 62;
constexpr int RowHeight = 42;
constexpr int MinimumSideMargin = 78;
}

SipSequenceWidget::SipSequenceWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(460);
    setMinimumHeight(HeaderHeight + RowHeight * 3);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
}

void SipSequenceWidget::addMessage(const QString &from, const QString &to,
                                   const QString &label, const QString &kind) {
    messages.append({QTime::currentTime().toString("hh:mm:ss"), from, to, label, kind});
    if (messages.size() > 200) messages.removeFirst();

    setMinimumHeight(HeaderHeight + qMax(3, messages.size()) * RowHeight + 18);
    updateGeometry();
    update();
}

void SipSequenceWidget::clearMessages() {
    messages.clear();
    setMinimumHeight(HeaderHeight + RowHeight * 3 + 18);
    updateGeometry();
    update();
}

QSize SipSequenceWidget::sizeHint() const {
    return QSize(760, HeaderHeight + qMax(3, messages.size()) * RowHeight + 18);
}

int SipSequenceWidget::endpointX(const QString &endpoint) const {
    const int sideMargin = qMax(MinimumSideMargin, width() / 7);
    if (endpoint == "1001") return sideMargin;
    if (endpoint == "1002") return width() - sideMargin;
    return width() / 2;
}

QColor SipSequenceWidget::messageColor(const Message &message) const {
    if (message.kind == "Response") return QColor("#16a34a");
    if (message.kind == "Challenge") return QColor("#d97706");
    if (message.kind == "Media") return QColor("#7c3aed");
    if (message.kind == "Error") return QColor("#dc2626");
    return QColor("#2563eb");
}

void SipSequenceWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#f8fafc"));

    const QStringList endpoints = {"1001", "Asterisk", "1002"};
    const QStringList subtitles = {"Qt client", "SIP server", "Remote client"};
    const QVector<int> positions = {endpointX("1001"), endpointX("Asterisk"), endpointX("1002")};

    for (int index = 0; index < endpoints.size(); ++index) {
        const int x = positions[index];
        const qreal badgeWidth = qMin(122, width() / 4);
        const QRectF badge(x - badgeWidth / 2, 8, badgeWidth, 42);
        painter.setPen(QPen(QColor("#cbd5e1"), 1));
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(badge, 8, 8);

        QFont endpointFont = painter.font();
        endpointFont.setBold(true);
        endpointFont.setPointSize(10);
        painter.setFont(endpointFont);
        painter.setPen(QColor("#0f172a"));
        painter.drawText(QRectF(x - badgeWidth / 2 + 3, 11, badgeWidth - 6, 18),
                         Qt::AlignCenter, endpoints[index]);

        QFont subtitleFont = painter.font();
        subtitleFont.setBold(false);
        subtitleFont.setPointSize(8);
        painter.setFont(subtitleFont);
        painter.setPen(QColor("#64748b"));
        painter.drawText(QRectF(x - badgeWidth / 2 + 3, 28, badgeWidth - 6, 16),
                         Qt::AlignCenter, subtitles[index]);

        QPen lifelinePen(QColor("#cbd5e1"), 1, Qt::DashLine);
        painter.setPen(lifelinePen);
        painter.drawLine(QPointF(x, 50), QPointF(x, height() - 8));
    }

    if (messages.isEmpty()) {
        painter.setPen(QColor("#94a3b8"));
        painter.drawText(QRect(0, HeaderHeight + 20, width(), 40), Qt::AlignCenter,
                         "Thực hiện đăng ký hoặc cuộc gọi để xem luồng SIP");
        return;
    }

    for (int index = 0; index < messages.size(); ++index) {
        const Message &message = messages[index];
        const qreal y = HeaderHeight + index * RowHeight + 24;
        const qreal sourceX = endpointX(message.from);
        const qreal targetX = endpointX(message.to);
        const qreal direction = targetX >= sourceX ? 1.0 : -1.0;
        const qreal startX = sourceX + direction * 8;
        const qreal endX = targetX - direction * 10;
        const QColor color = messageColor(message);

        if (index % 2 == 0) painter.fillRect(QRectF(0, y - 21, width(), RowHeight), QColor("#f1f5f9"));

        painter.setPen(QColor("#94a3b8"));
        QFont timeFont = painter.font();
        timeFont.setPointSize(8);
        timeFont.setBold(false);
        painter.setFont(timeFont);
        painter.drawText(QRectF(8, y - 8, 68, 18), Qt::AlignLeft, message.time);

        QPen arrowPen(color, 2);
        if (message.kind == "Media") arrowPen.setStyle(Qt::DashLine);
        painter.setPen(arrowPen);
        painter.drawLine(QPointF(startX, y), QPointF(endX, y));

        auto drawArrowHead = [&](qreal tipX, qreal arrowDirection) {
            QPolygonF head;
            head << QPointF(tipX, y)
                 << QPointF(tipX - arrowDirection * 9, y - 5)
                 << QPointF(tipX - arrowDirection * 9, y + 5);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawPolygon(head);
        };
        drawArrowHead(endX, direction);
        if (message.kind == "Media") drawArrowHead(startX, -direction);

        QFont labelFont = painter.font();
        labelFont.setPointSize(9);
        labelFont.setBold(true);
        painter.setFont(labelFont);
        const int labelWidth = qMin(220, qMax(80, painter.fontMetrics().horizontalAdvance(message.label) + 16));
        const qreal centerX = (startX + endX) / 2.0;
        const QRectF labelRect(centerX - labelWidth / 2.0, y - 20, labelWidth, 17);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#f8fafc"));
        painter.drawRoundedRect(labelRect, 4, 4);
        painter.setPen(color);
        painter.drawText(labelRect, Qt::AlignCenter, painter.fontMetrics().elidedText(
                             message.label, Qt::ElideRight, labelWidth - 10));
    }
}
