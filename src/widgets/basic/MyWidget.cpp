
#include "utils/logger.h"
#include "widgets/basic/MyWidget.h"
#include "widgets/basic/MyMainWindow.h"


using namespace std;

MyWidget::MyWidget(QWidget* parent) : QWidget(parent) {
    LINFO("MyWidget::MyWidget", fmt::format("新子窗口 MyWidget({:p}) 被创建", (void*) this));
    this->is_running = true;
    this->resize(400, 300);
    this->setWindowFlags(
        Qt::WindowType::WindowCloseButtonHint |
        Qt::WindowType::MSWindowsFixedSizeDialogHint |
        Qt::WindowType::WindowTitleHint |
        Qt::WindowType::WindowSystemMenuHint |
        Qt::WindowType::Window
    );
    this->setWindowModality(Qt::WindowModality::ApplicationModal);
    this->startAnimation1 = nullptr;
    this->closeAnimation1 = nullptr;
    this->closeAnimation2 = nullptr;
}


void MyWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    LINFO("MyWidget::showEvent", fmt::format("子窗口 MyWidget({:p}) 显示", (void*) this));
    this->closed = false;
    this->showStartAnimation();
}


void MyWidget::showStartAnimation(bool blocking) {
    LDEBUG("MyWidget::showStartAnimation", fmt::format("子窗口 MyWidget({:p}) 开始显示动画", (void*) this));
    QPoint animStartPoint;
    QPoint animEndPoint;
    QObject* parent = this->parent();
    QWidget* widgetParent = nullptr;
    QEventLoop* loop;
    QTimer* timer;
    bool has_parent = false;
    if (parent != nullptr) {
        widgetParent = qobject_cast<QWidget*>(parent); // 以防传进来的是诡异的东西
        if (widgetParent != nullptr) {
            has_parent = true;
        }
    }
    if (has_parent) {
        animEndPoint = (
            widgetParent->geometry().topLeft()
            + QPoint(
                widgetParent->geometry().width() / 2 - this->geometry().width() / 2,
                widgetParent->geometry().height() / 2 - this->geometry().height() / 2
            )
            );
    } else {
        animEndPoint = (
            QGuiApplication::primaryScreen()->geometry().topLeft()
            + QPoint(
                QGuiApplication::primaryScreen()->geometry().width() / 2 - this->geometry().width() / 2,
                QGuiApplication::primaryScreen()->geometry().height() / 2 - this->geometry().height() / 2
            )
            );
    }
    animStartPoint = QPoint(
        animEndPoint.x(),
        QGuiApplication::primaryScreen()->geometry().height() + this->geometry().height()
    );
    this->move(animStartPoint);
    this->show();
    this->startAnimation1 = new QPropertyAnimation(this, "pos");
    this->startAnimation1->setDuration((double) 500 / ANIMATION_SPEED);
    this->startAnimation1->setStartValue(animStartPoint);
    this->startAnimation1->setEndValue(animEndPoint);
    this->startAnimation1->setEasingCurve(QEasingCurve::OutCubic);
    if (blocking) {
        LDEBUG("MyWidget::showStartAnimation", fmt::format("子窗口等待动画结束", (void*) this));
        this->startAnimation1->start();
        loop = new QEventLoop(this);
        timer = new QTimer(this);
        function<void()> eventLoop = [&]() {
            if (this->startAnimation1->state() == QAbstractAnimation::Stopped) loop->quit();
            };
        QObject::connect(timer, &QTimer::timeout, eventLoop);
        timer->start(100);
        loop->exec();
        timer->stop();
        delete timer;
        delete loop;
        LDEBUG("MyWidget::showStartAnimation", fmt::format("子窗口 MyWidget({:p}) 动画结束", (void*) this));
    } else {
        this->startAnimation1->start(QPropertyAnimation::DeleteWhenStopped);
    }
}


void MyWidget::closeEvent(QCloseEvent* event) {
    LINFO("MyWidget::closeEvent", fmt::format("子窗口 MyWidget({:p}) 关闭", (void*) this));
    this->showCloseAnimation();
    QWidget::closeEvent(event);
}

void MyWidget::showCloseAnimation() {
    if (this->closed) return;
    LDEBUG("MyWidget::showCloseAnimation", fmt::format("子窗口 MyWidget({:p}) 开始展示关闭动画", (void*) this));
    this->closed = true;
    QPoint animStartPoint1 = this->pos();
    QPoint animEndPoint1 = QPoint(animStartPoint1.x(), animStartPoint1.y() - 75);
    this->closeAnimation1 = new QPropertyAnimation(this, "pos");
    this->closeAnimation1->setDuration((double) 150 / ANIMATION_SPEED);
    this->closeAnimation1->setStartValue(animStartPoint1);
    this->closeAnimation1->setEndValue(animEndPoint1);
    this->closeAnimation1->setEasingCurve(QEasingCurve::OutQuad);
    this->closeAnimation1->start();
    QTimer* timer = new QTimer(this);
    QEventLoop* loop = new QEventLoop(this);
    function<void()> eventLoop;
    eventLoop = [&]() {
        if (this->closeAnimation1->state() == QAbstractAnimation::Stopped) loop->quit();
        };
    QObject::connect(timer, &QTimer::timeout, eventLoop);
    timer->start(33);
    loop->exec();
    timer->stop();
    LDEBUG("MyWidget::showCloseAnimation", fmt::format("子窗口关闭动画阶段1完成", (void*) this));
    QPoint animStartPoint2 = this->pos();
    QPoint animEndPoint2 = QPoint(animStartPoint2.x(), QGuiApplication::primaryScreen()->geometry().height() + this->height());
    this->closeAnimation2 = new QPropertyAnimation(this, "pos");
    this->closeAnimation2->setDuration((double) 230 / ANIMATION_SPEED);
    this->closeAnimation2->setStartValue(animStartPoint2);
    this->closeAnimation2->setEndValue(animEndPoint2);
    this->closeAnimation2->setEasingCurve(QEasingCurve::InQuad);
    this->closeAnimation2->start();
    timer = new QTimer(this);
    loop = new QEventLoop(this);
    eventLoop = [&]() {
        if (this->closeAnimation2->state() == QAbstractAnimation::Stopped) loop->quit();
        };
    QObject::connect(timer, &QTimer::timeout, eventLoop);
    timer->start(33);
    loop->exec();
    timer->stop();
    LDEBUG("MyWidget::showCloseAnimation", fmt::format("子窗口关闭动画阶段2完成", (void*) this));
    this->deleteLater();
    delete closeAnimation1;
    delete closeAnimation2;
    delete timer;
    delete loop;
}