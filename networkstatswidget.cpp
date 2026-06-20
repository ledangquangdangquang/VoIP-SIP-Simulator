#include "networkstatswidget.h"

#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int HistoryLimit = 60;
}

NetworkStatsWidget::NetworkStatsWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void NetworkStatsWidget::resetStats() {
    current = NetworkStatsSample();
    uploadHistory.clear();
    downloadHistory.clear();
    active = true;
    update();
}

void NetworkStatsWidget::updateStats(const NetworkStatsSample &sample) {
    current = sample;
    active = true;
    uploadHistory.append(sample.uploadKbps);
    downloadHistory.append(sample.downloadKbps);
    if (uploadHistory.size() > HistoryLimit) uploadHistory.removeFirst();
    if (downloadHistory.size() > HistoryLimit) downloadHistory.removeFirst();
    update();
}

void NetworkStatsWidget::setInactive() {
    active = false;
    update();
}

QSize NetworkStatsWidget::sizeHint() const {
    return QSize(620, 320);
}

void NetworkStatsWidget::drawMetricCard(QPainter &painter, const QRectF &rect,
                                        const QString &title, const QString &value,
                                        const QColor &accent) const {
    painter.setPen(QPen(QColor("#e2e8f0"), 1));
    painter.setBrush(QColor("#f8fafc"));
    painter.drawRoundedRect(rect, 8, 8);
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawRoundedRect(QRectF(rect.left(), rect.top(), 4, rect.height()), 2, 2);

    QFont titleFont = painter.font();
    titleFont.setPointSize(8);
    titleFont.setBold(false);
    painter.setFont(titleFont);
    painter.setPen(QColor("#64748b"));
    painter.drawText(rect.adjusted(12, 7, -7, -30), Qt::AlignLeft | Qt::AlignVCenter, title);

    QFont valueFont = painter.font();
    valueFont.setPointSize(12);
    valueFont.setBold(true);
    painter.setFont(valueFont);
    painter.setPen(QColor("#0f172a"));
    painter.drawText(rect.adjusted(12, 27, -7, -5), Qt::AlignLeft | Qt::AlignVCenter, value);
}

void NetworkStatsWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);

    const QColor qualityColor = current.packetLossPercent < 1.0 && current.jitterMs < 30.0
        && current.rttMs < 150.0 ? QColor("#16a34a") : QColor("#d97706");
    painter.setPen(Qt::NoPen);
    painter.setBrush(active ? qualityColor : QColor("#94a3b8"));
    painter.drawEllipse(QRectF(10, 8, 9, 9));

    QFont statusFont = painter.font();
    statusFont.setPointSize(9);
    statusFont.setBold(true);
    painter.setFont(statusFont);
    painter.setPen(QColor("#334155"));
    painter.drawText(QRectF(25, 2, width() / 2.0 - 30, 22), Qt::AlignVCenter,
                     active ? "ĐANG ĐO RTP" : "CHỜ CUỘC GỌI");
    painter.setPen(QColor("#64748b"));
    painter.drawText(QRectF(width() / 2.0, 2, width() / 2.0 - 10, 22),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("Codec: %1   •   Thời lượng: %2").arg(current.codec, current.duration));

    const qreal gap = 8;
    const qreal cardTop = 28;
    const bool compact = width() < 780;
    const int columnCount = compact ? 3 : 5;
    const int rowCount = compact ? 2 : 1;
    const qreal cardWidth = (width() - 20 - gap * (columnCount - 1)) / columnCount;
    const QStringList titles = {"UPLOAD", "DOWNLOAD", "PACKET LOSS", "JITTER", "RTT"};
    const QStringList values = {
        QString::number(current.uploadKbps, 'f', 1) + " kbps",
        QString::number(current.downloadKbps, 'f', 1) + " kbps",
        QString::number(current.packetLossPercent, 'f', 2) + " %",
        QString::number(current.jitterMs, 'f', 1) + " ms",
        QString::number(current.rttMs, 'f', 1) + " ms"
    };
    const QVector<QColor> colors = {
        QColor("#2563eb"), QColor("#7c3aed"), qualityColor,
        current.jitterMs < 30.0 ? QColor("#16a34a") : QColor("#d97706"),
        current.rttMs < 150.0 ? QColor("#16a34a") : QColor("#d97706")
    };

    for (int index = 0; index < 5; ++index) {
        const int row = index / columnCount;
        const int column = index % columnCount;
        drawMetricCard(painter, QRectF(10 + column * (cardWidth + gap),
                                      cardTop + row * (62 + gap), cardWidth, 62),
                       titles[index], values[index], colors[index]);
    }

    const qreal chartTop = cardTop + rowCount * 62 + (rowCount - 1) * gap + 22;
    const QRectF chartRect(42, chartTop, width() - 54, height() - chartTop - 18);
    painter.setPen(QPen(QColor("#e2e8f0"), 1));
    for (int line = 0; line <= 3; ++line) {
        const qreal y = chartRect.top() + chartRect.height() * line / 3.0;
        painter.drawLine(QPointF(chartRect.left(), y), QPointF(chartRect.right(), y));
    }

    double maximum = 1.0;
    for (double value : uploadHistory) maximum = qMax(maximum, value);
    for (double value : downloadHistory) maximum = qMax(maximum, value);

    auto drawHistory = [&](const QVector<double> &history, const QColor &color) {
        if (history.isEmpty()) return;
        QPainterPath path;
        for (int index = 0; index < history.size(); ++index) {
            const qreal x = chartRect.left() + chartRect.width()
                * index / qMax(1, HistoryLimit - 1);
            const qreal y = chartRect.bottom() - chartRect.height() * history[index] / maximum;
            if (index == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        painter.setPen(QPen(color, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    };
    drawHistory(uploadHistory, QColor("#2563eb"));
    drawHistory(downloadHistory, QColor("#7c3aed"));

    QFont chartFont = painter.font();
    chartFont.setPointSize(8);
    chartFont.setBold(false);
    painter.setFont(chartFont);
    painter.setPen(QColor("#64748b"));
    painter.drawText(QRectF(10, chartTop - 7, 30, 18), Qt::AlignRight,
                     QString::number(maximum, 'f', 0));
    painter.drawText(QRectF(10, chartRect.bottom() - 8, 30, 18), Qt::AlignRight, "0");
    painter.drawText(QRectF(chartRect.left(), chartRect.bottom() + 3, chartRect.width(), 16),
                     Qt::AlignLeft, QString("TX %1 packets").arg(current.txPackets));
    painter.drawText(QRectF(chartRect.left(), chartRect.bottom() + 3, chartRect.width(), 16),
                     Qt::AlignRight, QString("RX %1 packets").arg(current.rxPackets));
}
