
#include "utils/logger.h"
#include "widgets/basic/MyMainWindow.h"

MyMainWindow::MyMainWindow(QWidget* parent) : QMainWindow(parent) {
    LINFO("MyMainWindow::MyMainWindow", fmt::format("主窗口 MainWindow({:p}) 被创建", (void*) this));
    this->resize(800, 600);
}

void MyMainWindow::closeEvent(QCloseEvent* event) {
    LINFO("MyMainWindow::closeEvent", fmt::format("主窗口 MainWindow({:p}) 关闭事件被触发", (void*) this));
    bool confirm = QMessageBox::question(
        this, "提示", "确定要退出吗？",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    if (confirm) {
        LINFO( "MyMainWindow::closeEvent", fmt::format("用户选择退出，主窗口即将关闭"));
        event->accept();
    } else {
        LINFO("MyMainWindow::closeEvent", fmt::format("用户取消退出，主窗口关闭事件被忽略"));
        event->ignore();
    }
}