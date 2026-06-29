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


