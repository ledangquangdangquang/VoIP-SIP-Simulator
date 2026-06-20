#pragma once

#include <QVector>
#include <QWidget>

struct NetworkStatsSample {
    double uploadKbps = 0.0;
    double downloadKbps = 0.0;
    double packetLossPercent = 0.0;
    double jitterMs = 0.0;
    double rttMs = 0.0;
    quint64 txPackets = 0;
    quint64 rxPackets = 0;
    QString codec = "—";
    QString duration = "00:00";
};

class NetworkStatsWidget : public QWidget {
    Q_OBJECT

public:
    explicit NetworkStatsWidget(QWidget *parent = nullptr);

    void resetStats();
    void updateStats(const NetworkStatsSample &sample);
    void setInactive();
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    NetworkStatsSample current;
    QVector<double> uploadHistory;
    QVector<double> downloadHistory;
    bool active = false;

    void drawMetricCard(QPainter &painter, const QRectF &rect,
                        const QString &title, const QString &value,
                        const QColor &accent) const;
};
