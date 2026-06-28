#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>

QWidget* createMetricWidget(const QString& labelText, const QString& valueText, const QString& color) {
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
    return frame;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setStyleSheet("QWidget { background-color: #1e1e1e; }");
    window.resize(1280, 800);

    QVBoxLayout *mainLayout = new QVBoxLayout(&window);

    QLabel *header = new QLabel("UltraHawk Ground Control Software (GCS)");
    header->setStyleSheet("color: white; font-size: 24px; font-weight: bold; padding: 20px;");
    header->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(header);

    QGridLayout *uGrid = new QGridLayout();
    
    // Left Column
    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->addWidget(createMetricWidget("ALTITUDE", "450m", "#00ff00"));
    leftCol->addWidget(createMetricWidget("SPEED", "12 m/s", "#00d4ff"));
    leftCol->addWidget(createMetricWidget("VERTICAL", "+0.5 m/s", "#ffffff"));
    leftCol->addStretch();
    
    // Right Column
    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->addWidget(createMetricWidget("BATTERY", "88%", "#ffff00"));
    rightCol->addWidget(createMetricWidget("SIGNAL", "-42 dBm", "#ff00ff"));
    rightCol->addWidget(createMetricWidget("TEMP", "45°C", "#ffaa00"));
    rightCol->addStretch();

    // Bottom Row
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->addWidget(createMetricWidget("LAT", "37.43", "#ffffff"));
    bottomRow->addWidget(createMetricWidget("LON", "-121.89", "#ffffff"));
    bottomRow->addWidget(createMetricWidget("HEADING", "180°", "#ffffff"));
    bottomRow->addWidget(createMetricWidget("MODE", "GUIDED", "#00ff00"));

    uGrid->addLayout(leftCol, 0, 0);
    uGrid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding), 0, 1);
    uGrid->addLayout(rightCol, 0, 2);
    uGrid->addLayout(bottomRow, 1, 0, 1, 3);

    mainLayout->addLayout(uGrid);

    window.show();
    return app.exec();
}
