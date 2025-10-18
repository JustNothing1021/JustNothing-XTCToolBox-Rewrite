#pragma once

#ifndef MYMAINWINDOW_H
#define MYMAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>
#include <QMessageBox>


/**
 * 我的主窗口类，基于QMainWindow。
 */
class MyMainWindow : public QMainWindow {
    Q_OBJECT



public:
    /**
     * 主窗口的构造函数。
     * @param parent 父窗口指针
     */
    explicit MyMainWindow(QWidget* parent = nullptr);

protected:
    /**
     * 经过覆写的关闭事件处理函数。
     * @param event 关闭事件
     */
    void closeEvent(QCloseEvent* event) override;
};

#endif // MYMAINWINDOW_H