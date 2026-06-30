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
    std::istringstream iss(line);
    std::string tag;
    std::string tField;

    std::cout << line << std::endl;

    if (!(iss >> tag)) return false;
    if (tag != "DP") return false;

    if (!(iss >> out.packetNumber)) return false;
    if (!(iss >> tField)) return false;

    auto eq = tField.find('=');
    if (eq == std::string::npos) return false;

    try {
        out.missionTime = std::stod(tField.substr(eq + 1));

        std::string rollStr, pitchStr, yawStr, stateField;
        if (!(iss >> rollStr >> pitchStr >> yawStr >> stateField)) return false;

        out.roll = std::stod(rollStr);
        out.pitch = std::stod(pitchStr);
        out.yaw = std::stod(yawStr);

        auto stateEq = stateField.find('=');
        if (stateEq != std::string::npos) {
            int stateVal = std::stoi(stateField.substr(stateEq + 1));
            out.state = static_cast<States>(stateVal);
        }
    } catch (...) {
        return false;
    }

    return true;
}

class GCSWindow : public QWidget {
    Q_OBJECT

public:
    GCSWindow(QWidget *parent = nullptr) : QWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setupUI();

        try {
            serial = new SerialPort("/dev/ttyACM0", B115200);
            qDebug() << "Serial connected";
        } catch (...) {
            qDebug() << "Serial failed";
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

        auto *right = new QVBoxLayout();
        right->addWidget(createMetricWidget("MISSION TIME", "0.000", "#ffff00", &missionTimeVal));
        right->addWidget(createMetricWidget("PACKET #", "0", "#ff00ff", &packetVal));
        right->addWidget(createMetricWidget("STATE", "---", "#aaaaaa", &stateVal));

        grid->addLayout(left, 0, 0);
        grid->addWidget(pfd, 0, 1);
        grid->addLayout(right, 0, 2);
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
        if (pressedKeys.contains(Qt::Key_W)) newPitch += 20.0f;
        if (pressedKeys.contains(Qt::Key_S)) newPitch -= 20.0f;

        float newRoll = 0.0f;
        if (pressedKeys.contains(Qt::Key_D)) newRoll += 20.0f;
        if (pressedKeys.contains(Qt::Key_A)) newRoll -= 20.0f;

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

            TelemetryData temp;
            if (parseTelemetryLine(*lineOpt, temp)) {
                latest = temp;
                gotData = true;
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
