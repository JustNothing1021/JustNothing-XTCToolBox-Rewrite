#pragma once


#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <functional>
#include <QTimer>
#include <QWidget>
#include <QMainWindow>
#include <QPropertyAnimation>

#define ANIMATION_SPEED 1.0

class MyWidget : public QWidget {
    Q_OBJECT

public:
    explicit MyWidget(QWidget *parent = nullptr);
    bool is_running;
    bool closed;
    QPropertyAnimation *startAnimation1;
    QPropertyAnimation *closeAnimation1;
    QPropertyAnimation *closeAnimation2;


protected:
    /**
     * 经过覆写的启动事件处理函数。
     * @param event 启动事件的指针
     */
    void showEvent(QShowEvent *event) override;
    /**
     * 经过覆写的关闭事件处理函数。
     * @param event 关闭事件的指针
     */
    void closeEvent(QCloseEvent *event) override;
    /**
     * 执行启动动画。
     * @param blocking 是否阻塞线程
     */
    void showStartAnimation(bool blocking = false);
    /**
     * 执行关闭动画。
     */
    void showCloseAnimation();
};

#endif // MYWIDGET_H