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
#include <sstream>
#include "SerialPort.hpp"

class PfdWidget : public QWidget {
public:
    explicit PfdWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(280, 280);
    }

    void setAttitude(double rollDeg, double pitchDeg) {
        roll = rollDeg;
        pitch = pitchDeg;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const int side = qMin(width(), height());
        const QRectF bounds((width() - side) / 2.0, (height() - side) / 2.0, side, side);
        const QPointF center = bounds.center();
        const double radius = side / 2.0;

        painter.setClipPath(circlePath(center, radius));
        drawHorizon(painter, center, radius);
        painter.setClipping(false);

        drawBezel(painter, center, radius);
        drawRollPointer(painter, center, radius);
        drawFixedAircraft(painter, center, radius);
        drawAngleReadouts(painter, center, radius);
    }

private:
    double roll = 0.0;
    double pitch = 0.0;

    static QPainterPath circlePath(const QPointF& center, double radius) {
        QPainterPath path;
        path.addEllipse(center, radius, radius);
        return path;
    }

    void drawHorizon(QPainter& painter, const QPointF& center, double radius) {
        const double pitchPixelsPerDegree = radius / 25.0;

        painter.save();
        painter.translate(center);
        painter.rotate(-roll);
        painter.translate(0, pitch * pitchPixelsPerDegree);

        const double span = radius * 4;
        QLinearGradient skyGradient(0, -span, 0, 0);
        skyGradient.setColorAt(0, QColor("#1c4f8a"));
        skyGradient.setColorAt(1, QColor("#4f9bdc"));

        QLinearGradient groundGradient(0, 0, 0, span);
        groundGradient.setColorAt(0, QColor("#6b4a27"));
        groundGradient.setColorAt(1, QColor("#3a2a16"));

        painter.setPen(Qt::NoPen);
        painter.setBrush(skyGradient);
        painter.drawRect(QRectF(-span, -span, span * 2, span));

        painter.setBrush(groundGradient);
        painter.drawRect(QRectF(-span, 0, span * 2, span));

        QPen horizonPen(Qt::white, 2);
        painter.setPen(horizonPen);
        painter.drawLine(QPointF(-span, 0), QPointF(span, 0));

        drawPitchLadder(painter, pitchPixelsPerDegree);

        painter.restore();
    }

    void drawPitchLadder(QPainter& painter, double pixelsPerDegree) {
        QPen ladderPen(Qt::white, 1.5);
        painter.setPen(ladderPen);
        painter.setFont(QFont("Sans", 9));

        for (int deg = -90; deg <= 90; deg += 10) {
            if (deg == 0) {
                continue;
            }

            const double y = -deg * pixelsPerDegree;
            const double halfWidth = (deg % 30 == 0) ? 45 : 25;

            painter.drawLine(QPointF(-halfWidth, y), QPointF(halfWidth, y));

            if (deg % 30 == 0) {
                painter.drawText(QPointF(halfWidth + 4, y + 4), QString::number(deg));
                painter.drawText(QPointF(-halfWidth - 22, y + 4), QString::number(deg));
            }
        }
    }

    void drawBezel(QPainter& painter, const QPointF& center, double radius) {
        QPen bezelPen(QColor("#0d0d0d"), 6);
        painter.setPen(bezelPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius - 3, radius - 3);
    }

    void drawRollPointer(QPainter& painter, const QPointF& center, double radius) {
        painter.save();
        painter.translate(center);

        QPen tickPen(Qt::white, 2);
        painter.setPen(tickPen);

        for (int angle : {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60}) {
            painter.save();
            painter.rotate(angle);
            const double outer = radius - 6;
            const double inner = (angle == 0) ? outer - 12 : outer - 7;
            painter.drawLine(QPointF(0, -outer), QPointF(0, -inner));
            painter.restore();
        }

        painter.rotate(-roll);
        QPolygonF pointer;
        pointer << QPointF(0, -radius + 14)
                << QPointF(-7, -radius + 26)
                << QPointF(7, -radius + 26);
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(pointer);

        painter.restore();
    }

    void drawFixedAircraft(QPainter& painter, const QPointF& center, double radius) {
        Q_UNUSED(radius);
        painter.save();
        painter.translate(center);

        QPen wingPen(QColor("#ffaa00"), 4);
        painter.setPen(wingPen);
        painter.drawLine(QPointF(-40, 0), QPointF(-12, 0));
        painter.drawLine(QPointF(12, 0), QPointF(40, 0));
        painter.drawLine(QPointF(-12, 0), QPointF(-12, 8));
        painter.drawLine(QPointF(12, 0), QPointF(12, 8));

        painter.setBrush(QColor("#ffaa00"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(0, 0), 3, 3);

        painter.restore();
    }

    void drawAngleReadouts(QPainter& painter, const QPointF& center, double radius) {
        painter.save();
        painter.setFont(QFont("Sans", 10, QFont::Bold));

        const QString rollText = QString("R %1°").arg(roll, 0, 'f', 1);
        const QString pitchText = QString("P %1°").arg(pitch, 0, 'f', 1);

        const QRectF rollRect(center.x() - radius + 8, center.y() + radius - 26, 70, 18);
        const QRectF pitchRect(center.x() + radius - 78, center.y() + radius - 26, 70, 18);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 140));
        painter.drawRoundedRect(rollRect, 4, 4);
        painter.drawRoundedRect(pitchRect, 4, 4);

        painter.setPen(Qt::white);
        painter.drawText(rollRect, Qt::AlignCenter, rollText);
        painter.drawText(pitchRect, Qt::AlignCenter, pitchText);

        painter.restore();
    }
};

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

QWidget* createMetricWidget(const QString& labelText, const QString& valueText,
                             const QString& color, QLabel** valueLabelOut = nullptr) {
    QFrame *frame = new QFrame();
    frame->setStyleSheet("background-color: #2b2b2b; border-radius: 10px;");
    frame->setFixedSize(160, 80);

    QVBoxLayout *layout = new QVBoxLayout(frame);
    QLabel *lbl = new QLabel(labelText);
    QLabel *val = new QLabel(valueText);

    val->setStyleSheet("font-size: 18px; font-weight: bold; color: " + color + ";");
    lbl->setStyleSheet("color: #aaaaaa; font-size: 10px;");

    layout->addWidget(lbl);
    layout->addWidget(val);
    layout->setAlignment(Qt::AlignCenter);

    if (valueLabelOut) {
        *valueLabelOut = val;
    }

    return frame;
}

struct TelemetryData {
    size_t packetNumber = 0;
    double missionTime = 0.0;
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
};

bool parseTelemetryLine(const std::string& line, TelemetryData& out) {
    std::istringstream iss(line);
    std::string tag;
    std::string tField;

    if (!(iss >> tag)) {
        return false;
    }

    if (tag != "DP") {
        return false;
    }

    if (!(iss >> out.packetNumber)) {
        return false;
    }

    if (!(iss >> tField)) {
        return false;
    }

    auto eq = tField.find('=');
    if (eq == std::string::npos) {
        return false;
    }

    try {
        out.missionTime = std::stod(tField.substr(eq + 1));
        std::string rollStr, pitchStr, yawStr;
        if (!(iss >> rollStr >> pitchStr >> yawStr)) {
            return false;
        }
        out.roll = std::stod(rollStr);
        out.pitch = std::stod(pitchStr);
        out.yaw = std::stod(yawStr);
    } catch (...) {
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setStyleSheet("QWidget { background-color: #1e1e1e; }");
    window.resize(1280, 900);

    QVBoxLayout *mainLayout = new QVBoxLayout(&window);

    QLabel *header = new QLabel("UltraHawk Ground Control Software (GCS)");
    header->setStyleSheet("color: white; font-size: 24px; font-weight: bold; padding: 20px;");
    header->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(header);

    QGridLayout *uGrid = new QGridLayout();

    QLabel *rollVal = nullptr;
    QLabel *pitchVal = nullptr;
    QLabel *yawVal = nullptr;
    QLabel *missionTimeVal = nullptr;
    QLabel *packetVal = nullptr;
    QLabel *statusVal = nullptr;

    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->addWidget(createMetricWidget("ROLL", "0.0°", "#00ff00", &rollVal));
    leftCol->addWidget(createMetricWidget("PITCH", "0.0°", "#00d4ff", &pitchVal));
    leftCol->addWidget(createMetricWidget("YAW", "0.0°", "#ffffff", &yawVal));
    leftCol->addStretch();

    PfdWidget *pfd = new PfdWidget();

    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->addWidget(createMetricWidget("MISSION TIME", "0.000 s", "#ffff00", &missionTimeVal));
    rightCol->addWidget(createMetricWidget("PACKET #", "0", "#ff00ff", &packetVal));
    rightCol->addWidget(createMetricWidget("LINK STATUS", "WAITING", "#ffaa00", &statusVal));
    rightCol->addStretch();

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->addWidget(createMetricWidget("LAT", "37.43", "#ffffff"));
    bottomRow->addWidget(createMetricWidget("LON", "-121.89", "#ffffff"));
    bottomRow->addWidget(createMetricWidget("HEADING", "180°", "#ffffff"));
    bottomRow->addWidget(createMetricWidget("MODE", "GUIDED", "#00ff00"));

    uGrid->addLayout(leftCol, 0, 0);
    uGrid->addWidget(pfd, 0, 1, Qt::AlignCenter);
    uGrid->addLayout(rightCol, 0, 2);
    uGrid->addLayout(bottomRow, 1, 0, 1, 3);
    uGrid->setColumnStretch(0, 1);
    uGrid->setColumnStretch(1, 2);
    uGrid->setColumnStretch(2, 1);

    mainLayout->addLayout(uGrid);

    TrendGraphWidget *trendGraph = new TrendGraphWidget();
    mainLayout->addWidget(trendGraph);

    window.show();

    SerialPort* serial = nullptr;
    try {
        serial = new SerialPort("/dev/ttyACM0", B115200);
        if (statusVal) {
            statusVal->setText("CONNECTED");
        }
    } catch (const std::exception& e) {
        qWarning() << "Failed to open serial port:" << e.what();
        if (statusVal) {
            statusVal->setText("NO PORT");
        }
    }

    QTimer pollTimer;
    QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
        if (!serial) {
            return;
        }

        while (auto lineOpt = serial->readLine()) {
            const std::string& line = *lineOpt;

            TelemetryData data;
            if (parseTelemetryLine(line, data)) {
                if (packetVal) {
                    packetVal->setText(QString::number(data.packetNumber));
                }
                if (missionTimeVal) {
                    missionTimeVal->setText(QString::number(data.missionTime, 'f', 3) + " s");
                }
                if (rollVal) {
                    rollVal->setText(QString::number(data.roll, 'f', 2) + "°");
                }
                if (pitchVal) {
                    pitchVal->setText(QString::number(data.pitch, 'f', 2) + "°");
                }
                if (yawVal) {
                    yawVal->setText(QString::number(data.yaw, 'f', 2) + "°");
                }
                if (statusVal) {
                    statusVal->setText("CONNECTED TO GS");
                }
                if (pfd) {
                    pfd->setAttitude(data.roll, data.pitch);
                }
                if (trendGraph) {
                    trendGraph->addSample(data.missionTime, data.roll, data.pitch, data.yaw);
                }
            } else {
                qWarning() << "Failed to parse line:" << QString::fromStdString(line);
            }
        }
    });
    pollTimer.start(50);

    int ret = app.exec();
    delete serial;
    return ret;
}
