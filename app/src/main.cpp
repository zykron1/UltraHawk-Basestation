#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QCheckBox>
#include <QSet>
#include <QPainter>

#include <sstream>
#include <optional>
#include <string>
#include <iostream>

#include "SerialPort.hpp"
#include "PFD.hpp"
#include "TrendGraph.hpp"

enum States {
    IDLE,
    TELEOP,
    AUTON,
};

struct TelemetryData {
    States state = IDLE;
    size_t packetNumber = 0;
    double missionTime = 0.0;
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    double m1 = 0.0;
    double m2 = 0.0;
    double m3 = 0.0;
    double m4 = 0.0;
};

struct Command {
    States state = IDLE;
    float thrust = 0.f;
    float roll = 0.f;
    float pitch = 0.f;
    float yaw = 0.f;
    float xpos = 0.f;
    float ypos = 0.f;
    float zpos = 0.f;
};

static QString formatCommandLine(const Command& cmd) {
    return QString("%1 %2 %3 %4 %5 %6 %7 %8")
        .arg(static_cast<int>(cmd.state))
        .arg(cmd.thrust, 0, 'f', 4)
        .arg(cmd.roll, 0, 'f', 4)
        .arg(cmd.pitch, 0, 'f', 4)
        .arg(cmd.yaw, 0, 'f', 4)
        .arg(cmd.xpos, 0, 'f', 4)
        .arg(cmd.ypos, 0, 'f', 4)
        .arg(cmd.zpos, 0, 'f', 4);
}

static QString stateToString(States s) {
    switch (s) {
        case IDLE: return "IDLE";
        case TELEOP: return "TELEOP";
        case AUTON: return "AUTON";
        default: return "UNKNOWN";
    }
}

static QString stateToColor(States s) {
    switch (s) {
        case IDLE: return "#aaaaaa";
        case TELEOP: return "#00d4ff";
        case AUTON: return "#00ff00";
        default: return "#ff0000";
    }
}

static bool parseTelemetryLine(const std::string& line, TelemetryData& out) {
    if (line.find("DP ") != 0) return false;

    try {
        auto parseAttitudeValue = [](const std::string& token, const char* label, double& value) {
            std::string numeric = token;
            const std::string prefix = std::string(label) + "=";
            if (numeric.rfind(prefix, 0) == 0) numeric.erase(0, prefix.size());

            size_t parsed = 0;
            value = std::stod(numeric, &parsed);
            return parsed == numeric.size();
        };

        size_t tPos = line.find("t=");
        size_t statePos = line.find("state=");
        size_t motorPos = line.find("motor=");

        if (tPos == std::string::npos || statePos == std::string::npos || motorPos == std::string::npos) {
            return false;
        }

        std::string pktStr = line.substr(3, tPos - 4);
        out.packetNumber = std::stoull(pktStr);

        std::string motionStr = line.substr(tPos + 2, statePos - (tPos + 2));
        std::istringstream motionIss(motionStr);
        std::string rollToken;
        std::string pitchToken;
        std::string yawToken;
        if (!(motionIss >> out.missionTime >> rollToken >> pitchToken >> yawToken) ||
            !parseAttitudeValue(rollToken, "roll", out.roll) ||
            !parseAttitudeValue(pitchToken, "pitch", out.pitch) ||
            !parseAttitudeValue(yawToken, "yaw", out.yaw)) {
            return false;
        }

        std::string stateStr = line.substr(statePos + 6, motorPos - (statePos + 6));
        out.state = static_cast<States>(std::stoi(stateStr));

        std::string motorStr = line.substr(motorPos + 6);
        std::istringstream motorIss(motorStr);
        if (!(motorIss >> out.m1 >> out.m2 >> out.m3 >> out.m4)) {
            return false;
        }
    } catch (...) {
        return false;
    }

    return true;
}

class DroneMotorsWidget : public QWidget {
    Q_OBJECT
public:
    DroneMotorsWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(240, 260);
        setFocusPolicy(Qt::NoFocus); // Ensure widget never steals main keystroke focus
    }

    void setMotorOutputs(double m1, double m2, double m3, double m4) {
        m_m1 = m1;
        m_m2 = m2;
        m_m3 = m3;
        m_m4 = m4;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setPen(QPen(QColor("#444444"), 1));
        painter.setBrush(QColor("#2b2b2b"));
        painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 10, 10);

        int cx = width() / 2;
        int cy = height() / 2 - 15;
        int armLength = 55;

        painter.setPen(QPen(QColor("#555555"), 6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(cx - armLength, cy - armLength, cx + armLength, cy + armLength);
        painter.drawLine(cx + armLength, cy - armLength, cx - armLength, cy + armLength);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#1e1e1e"));
        painter.drawEllipse(QPoint(cx, cy), 18, 18);
        
        painter.setBrush(QColor("#ff3b30"));
        QPolygon centerTri;
        centerTri << QPoint(cx, cy - 12) << QPoint(cx - 6, cy - 2) << QPoint(cx + 6, cy - 2);
        painter.drawPolygon(centerTri);

        QPoint m1Pos(cx + armLength, cy - armLength);
        QPoint m2Pos(cx + armLength, cy + armLength);
        QPoint m3Pos(cx - armLength, cy + armLength);
        QPoint m4Pos(cx - armLength, cy - armLength);

        auto drawMotor = [&](const QPoint& pos, const QString& name, double val, const QColor& color) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#111111"));
            painter.drawEllipse(pos, 16, 16);
            
            painter.setPen(QPen(color, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(pos, 14, 14);

            painter.setPen(QColor("#ffffff"));
            QFont font = painter.font();
            font.setPointSize(8);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(QRect(pos.x() - 15, pos.y() - 15, 30, 30), Qt::AlignCenter, name);
        };

        drawMotor(m1Pos, "M1", m_m1, QColor("#00d4ff"));
        drawMotor(m2Pos, "M2", m_m2, QColor("#00ff00"));
        drawMotor(m3Pos, "M3", m_m3, QColor("#ffaa00"));
        drawMotor(m4Pos, "M4", m_m4, QColor("#ff3366"));

        QFont textFont = painter.font();
        textFont.setPointSize(9);
        textFont.setBold(false);
        painter.setFont(textFont);
        painter.setPen(QColor("#aaaaaa"));

        int textY = height() - 40;
        painter.drawText(QRect(10, textY, 105, 15), Qt::AlignLeft, QString("M1: %1").arg(m_m1, 0, 'f', 2));
        painter.drawText(QRect(125, textY, 105, 15), Qt::AlignRight, QString("M2: %1").arg(m_m2, 0, 'f', 2));
        painter.drawText(QRect(10, textY + 18, 105, 15), Qt::AlignLeft, QString("M4: %1").arg(m_m4, 0, 'f', 2));
        painter.drawText(QRect(125, textY + 18, 105, 15), Qt::AlignRight, QString("M3: %1").arg(m_m3, 0, 'f', 2));
    }

private:
    double m_m1 = 0.0, m_m2 = 0.0, m_m3 = 0.0, m_m4 = 0.0;
};

class GCSWindow : public QWidget {
    Q_OBJECT

public:
    GCSWindow(QWidget *parent = nullptr) : QWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setupUI();

        try {
            serial = new SerialPort("/dev/ttyACM0", B115200);
            qDebug() << "Serial connected";
        } catch (const std::exception& error) {
            qWarning() << "Serial failed:" << error.what();
        } catch (...) {
            qWarning() << "Serial failed: unknown error";
        }

        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &GCSWindow::readTelemetry);
        timer->start(20);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (stateBox->currentData().toInt() != TELEOP) {
            QWidget::keyPressEvent(event);
            return;
        }

        bool thrustChanged = false;
        if (event->key() == Qt::Key_Q) {
            cmd.thrust = qBound(-100.0f, cmd.thrust + 5.0f, 100.0f);
            thrustChanged = true;
        } else if (event->key() == Qt::Key_E) {
            cmd.thrust = qBound(-100.0f, cmd.thrust - 5.0f, 100.0f);
            thrustChanged = true;
        }

        if (event->isAutoRepeat()) {
            if (thrustChanged) {
                updateSetpointsDisplay();
                sendCommand();
            }
            return;
        }

        pressedKeys.insert(event->key());
        updateMovementKeys();

        if (thrustChanged && !event->isAutoRepeat()) {
            updateSetpointsDisplay();
            sendCommand();
        }
    }

    void keyReleaseEvent(QKeyEvent *event) override {
        if (event->isAutoRepeat()) {
            return;
        }

        pressedKeys.remove(event->key());
        updateMovementKeys();
    }

    // Force grab control focus back if window background area is clicked
    void mousePressEvent(QMouseEvent *event) override {
        Q_UNUSED(event);
        this->setFocus();
    }

private:
    Command cmd;
    QSet<int> pressedKeys;
    SerialPort *serial = nullptr;

    QComboBox *stateBox = nullptr;
    QLabel *throttleSpLabel = nullptr;
    QLabel *pitchSpLabel = nullptr;
    QLabel *yawSpLabel = nullptr;
    QLabel *rollSpLabel = nullptr;
    QLabel *lastSentLabel = nullptr;

    QLabel *rollVal = nullptr;
    QLabel *pitchVal = nullptr;
    QLabel *yawVal = nullptr;
    QLabel *missionTimeVal = nullptr;
    QLabel *packetVal = nullptr;
    QLabel *stateVal = nullptr;
    
    PfdWidget *pfd = nullptr;
    DroneMotorsWidget *droneMotors = nullptr;
    TrendGraphWidget *trend = nullptr;

    void setupUI() {
        auto *mainLayout = new QVBoxLayout(this);

        auto *header = new QLabel("UltraHawk Ground Control Software (GCS)");
        header->setStyleSheet("color: white; font-size: 24px; font-weight: bold; padding: 20px;");
        header->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(header);

        auto createMetricWidget = [&](const QString& label, const QString& value, const QString& color, QLabel** out) {
            QFrame *frame = new QFrame();
            frame->setStyleSheet("background-color: #2b2b2b; border-radius: 10px;");
            frame->setFixedSize(160, 80);
            auto *layout = new QVBoxLayout(frame);
            auto *lbl = new QLabel(label);
            auto *val = new QLabel(value);
            val->setStyleSheet("font-size: 18px; font-weight: bold; color: " + color + ";");
            lbl->setStyleSheet("color: #aaaaaa; font-size: 10px;");
            layout->addWidget(lbl);
            layout->addWidget(val);
            layout->setAlignment(Qt::AlignCenter);
            if (out) *out = val;
            return frame;
        };

        auto *grid = new QGridLayout();

        auto *left = new QVBoxLayout();
        left->addWidget(createMetricWidget("ROLL", "0.0°", "#00ff00", &rollVal));
        left->addWidget(createMetricWidget("PITCH", "0.0°", "#00d4ff", &pitchVal));
        left->addWidget(createMetricWidget("YAW", "0.0°", "#ffffff", &yawVal));

        pfd = new PfdWidget();
        droneMotors = new DroneMotorsWidget();

        auto *right = new QVBoxLayout();
        right->addWidget(createMetricWidget("MISSION TIME", "0.000", "#ffff00", &missionTimeVal));
        right->addWidget(createMetricWidget("PACKET #", "0", "#ff00ff", &packetVal));
        right->addWidget(createMetricWidget("STATE", "---", "#aaaaaa", &stateVal));

        grid->addLayout(left, 0, 0);
        grid->addWidget(pfd, 0, 1);
        grid->addWidget(droneMotors, 0, 2); 
        grid->addLayout(right, 0, 3);
        mainLayout->addLayout(grid);

        trend = new TrendGraphWidget();
        mainLayout->addWidget(trend);

        auto *spGroup = new QGroupBox("COMMAND SETPOINTS");
        spGroup->setStyleSheet("QGroupBox { color: white; font-weight: bold; border: 1px solid #444; border-radius: 8px; margin-top: 8px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; }");
        auto *spLayout = new QHBoxLayout(spGroup);

        stateBox = new QComboBox();
        stateBox->addItem("IDLE", IDLE);
        stateBox->addItem("TELEOP", TELEOP);
        stateBox->addItem("AUTON", AUTON);
        stateBox->setFocusPolicy(Qt::NoFocus);
        spLayout->addWidget(new QLabel("STATE:"));
        spLayout->addWidget(stateBox);

        auto createSpWidget = [](const QString& title, QLabel*& valLabel) {
            QFrame* f = new QFrame();
            f->setStyleSheet("background-color: #2b2b2b; border-radius: 6px; padding: 5px;");
            QVBoxLayout* l = new QVBoxLayout(f);
            QLabel* t = new QLabel(title);
            t->setStyleSheet("color: #aaaaaa; font-size: 10px;");
            valLabel = new QLabel("0.0");
            valLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
            l->addWidget(t);
            l->addWidget(valLabel);
            l->setAlignment(Qt::AlignCenter);
            return f;
        };

        spLayout->addWidget(createSpWidget("THROTTLE", throttleSpLabel));
        spLayout->addWidget(createSpWidget("PITCH", pitchSpLabel));
        spLayout->addWidget(createSpWidget("YAW", yawSpLabel));
        spLayout->addWidget(createSpWidget("ROLL", rollSpLabel));

        lastSentLabel = new QLabel("Last sent: ---");
        lastSentLabel->setStyleSheet("color: #888; font-size: 10px;");
        spLayout->addWidget(lastSentLabel);
        spLayout->addStretch();

        mainLayout->addWidget(spGroup);

        connect(stateBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            cmd.state = static_cast<States>(stateBox->currentData().toInt());
            if (cmd.state != TELEOP) {
                cmd.pitch = 0.0f;
                cmd.yaw = 0.0f;
                cmd.roll = 0.0f;
                cmd.thrust = 0.0f;
                pressedKeys.clear();
            }
            updateSetpointsDisplay();
            sendCommand();
            this->setFocus();
        });
    }

    void updateMovementKeys() {
        if (stateBox->currentData().toInt() != TELEOP) return;

        float newPitch = 0.0f;
        if (pressedKeys.contains(Qt::Key_W)) newPitch += 5.0f;
        if (pressedKeys.contains(Qt::Key_S)) newPitch -= 5.0f;

        float newRoll = 0.0f;
        if (pressedKeys.contains(Qt::Key_D)) newRoll += 5.0f;
        if (pressedKeys.contains(Qt::Key_A)) newRoll -= 5.0f;

        if (cmd.pitch != newPitch || cmd.roll != newRoll) {
            cmd.pitch = newPitch;
            cmd.roll = newRoll;
            updateSetpointsDisplay();
            sendCommand();
        }
    }

    void updateSetpointsDisplay() {
        throttleSpLabel->setText(QString::number(cmd.thrust, 'f', 1));
        pitchSpLabel->setText(QString::number(cmd.pitch, 'f', 1) + "°");
        yawSpLabel->setText(QString::number(cmd.yaw, 'f', 1) + "°");
        rollSpLabel->setText(QString::number(cmd.roll, 'f', 1) + "°");
    }

    void sendCommand() {
        if (!serial) return;
        QString line = formatCommandLine(cmd) + "\n";
        qInfo().noquote() << "Serial TX:" << line.trimmed();
        serial->writeString(line.toStdString());
        lastSentLabel->setText("Last sent: " + line.trimmed());
    }

    void readTelemetry() {
        if (!serial) return;

        TelemetryData latest;
        bool gotData = false;

        for (int i = 0; i < 200; i++) {
            auto lineOpt = serial->readLine();
            if (!lineOpt) break;

            qInfo().noquote() << "Serial RX:" << QString::fromStdString(*lineOpt);

            TelemetryData temp;
            if (parseTelemetryLine(*lineOpt, temp)) {
                qInfo() << "Telemetry parsed: packet" << temp.packetNumber
                        << "mission time" << temp.missionTime;
                latest = temp;
                gotData = true;
            } else {
                qWarning().noquote() << "Telemetry rejected:" << QString::fromStdString(*lineOpt);
            }
        }

        if (!gotData) return;

        packetVal->setText(QString::number(latest.packetNumber));
        missionTimeVal->setText(QString::number(latest.missionTime, 'f', 3));
        rollVal->setText(QString::number(latest.roll, 'f', 2) + "°");
        pitchVal->setText(QString::number(latest.pitch, 'f', 2) + "°");
        yawVal->setText(QString::number(latest.yaw, 'f', 2) + "°");

        stateVal->setText(stateToString(latest.state));
        stateVal->setStyleSheet(
            "font-size: 18px; font-weight: bold; color: " + stateToColor(latest.state)
        );

        pfd->setAttitude(latest.roll, latest.pitch);
        droneMotors->setMotorOutputs(latest.m1, latest.m2, latest.m3, latest.m4);
        trend->addSample(latest.missionTime, latest.roll, latest.pitch, latest.yaw);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    app.setStyleSheet(
        "QWidget { background-color: #1e1e1e; color: white; }"
        "QCheckBox { color: white; background-color: transparent; spacing: 5px; }"
        "QCheckBox::indicator { width: 15px; height: 15px; background-color: #444; border: 1px solid #777; }"
        "QCheckBox::indicator:checked { background-color: #00ff00; }"
        "QComboBox { color: white; background-color: #2b2b2b; border: 1px solid #444; padding: 4px; }"
        "QComboBox QAbstractItemView { color: white; background-color: #2b2b2b; selection-background-color: #444; }"
    );

    GCSWindow window;
    window.resize(1280, 900);
    window.show();

    return app.exec();
}

#include "main.moc"
