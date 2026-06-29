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
#include <QDoubleSpinBox>
#include <QPushButton>

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
    float thrust = 0.f, roll = 0.f, pitch = 0.f, yaw = 0.f;
    float xpos = 0.f, ypos = 0.f, zpos = 0.f;
};

static QString formatCommandLine(const Command& cmd) {
    QString line = QString("%1 %2 %3 %4 %5 %6 %7 %8")
        .arg(static_cast<int>(cmd.state))
        .arg(cmd.thrust, 0, 'f', 4)
        .arg(cmd.roll, 0, 'f', 4)
        .arg(cmd.pitch, 0, 'f', 4)
        .arg(cmd.yaw, 0, 'f', 4)
        .arg(cmd.xpos, 0, 'f', 4)
        .arg(cmd.ypos, 0, 'f', 4)
        .arg(cmd.zpos, 0, 'f', 4);
    return line;
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

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setStyleSheet("QWidget { background-color: #1e1e1e; }");
    window.resize(1280, 900);

    auto *mainLayout = new QVBoxLayout(&window);

    auto *header = new QLabel("UltraHawk Ground Control Software (GCS)");
    header->setStyleSheet("color: white; font-size: 24px; font-weight: bold; padding: 20px;");
    header->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(header);

    QLabel *rollVal = nullptr;
    QLabel *pitchVal = nullptr;
    QLabel *yawVal = nullptr;
    QLabel *missionTimeVal = nullptr;
    QLabel *packetVal = nullptr;
    QLabel *stateVal = nullptr;

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

    auto *pfd = new PfdWidget();

    auto *right = new QVBoxLayout();
    right->addWidget(createMetricWidget("MISSION TIME", "0.000", "#ffff00", &missionTimeVal));
    right->addWidget(createMetricWidget("PACKET #", "0", "#ff00ff", &packetVal));
    right->addWidget(createMetricWidget("STATE", "---", "#aaaaaa", &stateVal));

    grid->addLayout(left, 0, 0);
    grid->addWidget(pfd, 0, 1);
    grid->addLayout(right, 0, 2);

    mainLayout->addLayout(grid);

    auto *trend = new TrendGraphWidget();
    mainLayout->addWidget(trend);

	SerialPort *serial = nullptr;
    try {
        serial = new SerialPort("/dev/ttyACM0", B115200);
        qDebug() << "Serial connected";
    } catch (...) {
        qDebug() << "Serial failed";
    }

    auto *cmdGroup = new QGroupBox("ROVER COMMAND");
    cmdGroup->setStyleSheet("QGroupBox { color: white; font-weight: bold; border: 1px solid #444; border-radius: 8px; margin-top: 8px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; }");
    auto *cmdLayout = new QGridLayout(cmdGroup);

    auto *stateBox = new QComboBox();
    stateBox->addItem("IDLE", IDLE);
    stateBox->addItem("TELEOP", TELEOP);
    stateBox->addItem("AUTON", AUTON);

    auto makeSpin = [](double min, double max) {
        auto *sb = new QDoubleSpinBox();
        sb->setRange(min, max);
        sb->setDecimals(3);
        sb->setSingleStep(0.1);
        return sb;
    };

    auto *thrustSpin = makeSpin(-100, 100);
    auto *rollSpin = makeSpin(-180, 180);
    auto *pitchSpin = makeSpin(-180, 180);
    auto *yawSpin = makeSpin(-180, 180);
    auto *xSpin = makeSpin(-10000, 10000);
    auto *ySpin = makeSpin(-10000, 10000);
    auto *zSpin = makeSpin(-10000, 10000);

    int row = 0;
    cmdLayout->addWidget(new QLabel("State"), row, 0); cmdLayout->addWidget(stateBox, row++, 1);
    cmdLayout->addWidget(new QLabel("Thrust"), row, 0); cmdLayout->addWidget(thrustSpin, row++, 1);
    cmdLayout->addWidget(new QLabel("Roll"), row, 0); cmdLayout->addWidget(rollSpin, row++, 1);
    cmdLayout->addWidget(new QLabel("Pitch"), row, 0); cmdLayout->addWidget(pitchSpin, row++, 1);
    cmdLayout->addWidget(new QLabel("Yaw"), row, 0); cmdLayout->addWidget(yawSpin, row++, 1);
    cmdLayout->addWidget(new QLabel("X pos"), row, 0); cmdLayout->addWidget(xSpin, row++, 1);
    cmdLayout->addWidget(new QLabel("Y pos"), row, 0); cmdLayout->addWidget(ySpin, row++, 1);
    cmdLayout->addWidget(new QLabel("Z pos"), row, 0); cmdLayout->addWidget(zSpin, row++, 1);

    auto *sendBtn = new QPushButton("SEND COMMAND");
    sendBtn->setStyleSheet("background-color: #00aa55; color: white; font-weight: bold; padding: 8px; border-radius: 6px;");
    cmdLayout->addWidget(sendBtn, row++, 0, 1, 2);

    auto *lastSentLabel = new QLabel("Last sent: ---");
    lastSentLabel->setStyleSheet("color: #888; font-size: 10px;");
    cmdLayout->addWidget(lastSentLabel, row++, 0, 1, 2);

    mainLayout->addWidget(cmdGroup);

    QObject::connect(sendBtn, &QPushButton::clicked, [&]() {
        if (!serial) {
            qDebug() << "Cannot send: serial not connected";
            return;
        }

        Command cmd;
        cmd.state  = static_cast<States>(stateBox->currentData().toInt());
        cmd.thrust = static_cast<float>(thrustSpin->value());
        cmd.roll   = static_cast<float>(rollSpin->value());
        cmd.pitch  = static_cast<float>(pitchSpin->value());
        cmd.yaw    = static_cast<float>(yawSpin->value());
        cmd.xpos   = static_cast<float>(xSpin->value());
        cmd.ypos   = static_cast<float>(ySpin->value());
        cmd.zpos   = static_cast<float>(zSpin->value());

        QString line = formatCommandLine(cmd) + "\n";
        serial->writeString(line.toStdString());

        lastSentLabel->setText("Last sent: " + line.trimmed());
        qDebug() << "Sent command:" << line.trimmed();
    });

    

    QTimer timer;

    QObject::connect(&timer, &QTimer::timeout, [&]() {
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
    });

    timer.start(20); // 20ms debounce 

    window.show();
    return app.exec();
}
