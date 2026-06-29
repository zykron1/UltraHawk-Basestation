#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>
#include <QVector>

class TrendGraphWidget : public QWidget {
public:
    explicit TrendGraphWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(160);
    }

    void addSample(double timeSeconds, double rollDeg, double pitchDeg, double yawDeg) {
        samples.append({timeSeconds, rollDeg, pitchDeg, yawDeg});
        trimOldSamples(timeSeconds);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.fillRect(rect(), QColor("#2b2b2b"));

        const QRectF plotArea = rect().adjusted(50, 20, -16, -28);
        drawAxes(painter, plotArea);

        if (samples.size() < 2) {
            return;
        }

        const double latestTime = samples.last().time;
        const double earliestTime = latestTime - windowSeconds;

        drawSeries(painter, plotArea, earliestTime, latestTime, QColor("#00ff00"),
                   [](const Sample& s) { return s.roll; });
        drawSeries(painter, plotArea, earliestTime, latestTime, QColor("#00d4ff"),
                   [](const Sample& s) { return s.pitch; });
        drawSeries(painter, plotArea, earliestTime, latestTime, QColor("#ffaa00"),
                   [](const Sample& s) { return s.yaw; });

        drawLegend(painter, plotArea);
    }

private:
    struct Sample {
        double time;
        double roll;
        double pitch;
        double yaw;
    };

    QVector<Sample> samples;
    const double windowSeconds = 5.0;
    const double valueRange = 180.0;

    void trimOldSamples(double latestTime) {
        const double cutoff = latestTime - windowSeconds;
        while (!samples.isEmpty() && samples.first().time < cutoff) {
            samples.removeFirst();
        }
    }

    void drawAxes(QPainter& painter, const QRectF& plotArea) {
        QPen axisPen(QColor("#666666"), 1);
        painter.setPen(axisPen);
        painter.drawRect(plotArea);

        painter.setFont(QFont("Sans", 8));
        painter.setPen(QColor("#aaaaaa"));

        for (int value : {-180, -90, 0, 90, 180}) {
            const double y = mapValueToY(value, plotArea);
            painter.drawLine(QPointF(plotArea.left() - 4, y), QPointF(plotArea.left(), y));
            painter.drawText(QRectF(plotArea.left() - 46, y - 8, 40, 16),
                              Qt::AlignRight | Qt::AlignVCenter, QString::number(value) + "°");
        }

        painter.drawText(QRectF(plotArea.left(), plotArea.bottom() + 4, plotArea.width(), 16),
                          Qt::AlignCenter, QString("last %1 s").arg(windowSeconds, 0, 'f', 0));
    }

    double mapValueToY(double value, const QRectF& plotArea) const {
        const double t = (value + valueRange) / (2 * valueRange);
        return plotArea.bottom() - t * plotArea.height();
    }

    double mapTimeToX(double time, double earliestTime, double latestTime, const QRectF& plotArea) const {
        const double span = qMax(latestTime - earliestTime, 0.001);
        const double t = (time - earliestTime) / span;
        return plotArea.left() + t * plotArea.width();
    }

    template <typename ValueGetter>
    void drawSeries(QPainter& painter, const QRectF& plotArea, double earliestTime, double latestTime,
                     const QColor& color, ValueGetter getValue) {
        QPainterPath path;
        bool started = false;

        for (const Sample& s : samples) {
            if (s.time < earliestTime) {
                continue;
            }

            const double x = mapTimeToX(s.time, earliestTime, latestTime, plotArea);
            const double y = mapValueToY(getValue(s), plotArea);

            if (!started) {
                path.moveTo(x, y);
                started = true;
            } else {
                path.lineTo(x, y);
            }
        }

        QPen seriesPen(color, 2);
        painter.setPen(seriesPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    void drawLegend(QPainter& painter, const QRectF& plotArea) {
        Q_UNUSED(plotArea);
        const QVector<QPair<QString, QColor>> entries = {
            {"ROLL", QColor("#00ff00")},
            {"PITCH", QColor("#00d4ff")},
            {"YAW", QColor("#ffaa00")}
        };

        painter.setFont(QFont("Sans", 9, QFont::Bold));

        double x = plotArea.left();
        const double y = 4;

        for (const auto& entry : entries) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(entry.second);
            painter.drawRect(QRectF(x, y + 4, 10, 10));

            painter.setPen(Qt::white);
            const QString text = entry.first;
            const double textWidth = painter.fontMetrics().horizontalAdvance(text);
            painter.drawText(QPointF(x + 16, y + 13), text);

            x += 16 + textWidth + 20;
        }
    }
};
