#include <map>
#include <windows.h>
#include <shellapi.h>

#include <QTimer>
#include <QMainWindow>
#include <QApplication>
#include <QVBoxLayout>

// #include <emmcdl/emmcdl.h>
#include <emmcdl/utils.h>
#include <emmcdl_new/xmlparser.h>
#include <emmcdl_new/serialport.h>
#include <emmcdl_new/emmcdl.h>

#include <utils/logger.h>
#include <utils/usb_utils.h>
#include <utils/time_utils.h>
#include <utils/string_utils.h>

#include <datatypes/thread.h>
#include <datatypes/bytearray.h>

#include <widgets/basic/MyMainWindow.h>
#include <widgets/custom/MainWindow.h>


using namespace std;

class MainWindow : public MyMainWindow, public Ui_MainWindow {

private:
    bool isRefreshingComboBox = false;
    bool isFirstComboBoxUpdate = true;
    map<int, SerialPort> portMap;
    map<int, bool> portStatus;
    ByteArray lastSent, lastReceived;
    QTimer* updatePortListTimer;
    QTimer* updateWindowTimer;

public:
    MainWindow(QWidget* parent = nullptr) : MyMainWindow(parent) {
        setupUi(this);
        this->setFixedSize(this->width(), this->height());
        this->setWindowTitle("测试");
        updatePortListTimer = new QTimer(this);
        connect(updatePortListTimer, &QTimer::timeout, this, &MainWindow::updateComboBox);
        connect(comboBox,&QComboBox::currentIndexChanged, this, &MainWindow::onPortSelIdxChanged);
        connect(pushButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
        connect(pushButton_2, &QPushButton::clicked, this, &MainWindow::onRecvButtonClicked);
        connect(checkBox, &QCheckBox::toggled, this, &MainWindow::sendBrowserHexmodeTriggered);
        connect(checkBox_2, &QCheckBox::toggled, this, &MainWindow::recvBrowserHexmodeTriggered);
        connect(checkBox_3, &QCheckBox::toggled, this, &MainWindow::limitLenModeTriggered);
        connect(pushButton_3, &QPushButton::clicked, this, &MainWindow::onOpenPortButtonClicked);
        connect(pushButton_4, &QPushButton::clicked, this, &MainWindow::onClosePortButtonClicked);
        lastSent = "Hello, world!";
        textBrowser_2->setReadOnly(true);
        updatePortListTimer->start(1000);
        updateWindowTimer = new QTimer(this);
        connect(updateWindowTimer, &QTimer::timeout, this, &MainWindow::update);
        updateWindowTimer->start(1000 / 15);
        label_7->setText("<无发送记录>");
        label_10->setText("<无接收记录>");
        label_9->setText("<无接收记录>");
        label_12->setText("<无接收记录>");
        sendBrowserHexmodeTriggered();
        recvBrowserHexmodeTriggered();
        limitLenModeTriggered();
    }

    ~MainWindow() {
        delete updatePortListTimer;
        delete updateWindowTimer;
    }

    void update() {
        label_3->setText(
            QString::fromStdString(
                getSelectedPort() == -1 ? "未选择" : fmt::format("COM{}", getSelectedPort())
            )
        );
        label_17->setText(
            QString::fromStdString(
                getSelectedPort() == -1 ? "" : (portStatus[getSelectedPort()] ? "（开启）" : "（关闭）")
            )
        );
    }

    void sendBrowserHexmodeTriggered() {
        if (checkBox->isChecked()) {
            lastSent = textBrowser->toPlainText().toStdString();
            textBrowser->setPlainText(QString::fromStdString(lastSent.to_hex_view(8, false)));
            textBrowser->setReadOnly(true);
            label_15->setText("不可编辑");
        } else {
            textBrowser->setPlainText(QString::fromStdString(lastSent.to_string()));
            textBrowser->setReadOnly(false);
            label_15->setText("编辑状态");
        }
    }

    void recvBrowserHexmodeTriggered() {
        if (checkBox_2->isChecked()) {
            textBrowser_2->setPlainText(QString::fromStdString(lastReceived.to_hex_view(8, false)));
        } else {
            textBrowser_2->setPlainText(QString::fromStdString(lastReceived.to_string()));
        }
    }

    void limitLenModeTriggered() {
        if (checkBox_3->isChecked()) {
            spinBox->setEnabled(true);
        } else {
            spinBox->setEnabled(false);
        }
    }

    void onSendButtonClicked() {
        try {
            LINFO("MainWindow::onSendButtonClicked", "发送按钮被点击");
            if (!checkBox->isChecked()) lastSent = textBrowser->toPlainText().toStdString();
            int portNum = getSelectedPort();
            if (portNum == -1) {
                LWARN("MainWindow::onSendButtonClicked", "未选择端口，无法发送数据");
                QMessageBox::warning(this, "错误", "未选择端口，无法发送数据");
                return;
            }
            if (!portStatus[portNum]) {
                LINFO("MainWindow::onSendButtonClicked", "端口%d未打开，尝试打开端口", portNum);
                portMap[portNum] = SerialPort();
                portMap[portNum].EnableBinaryLog();
                int stat = portMap[portNum].Open(portNum);
                if (stat != ERROR_SUCCESS) {
                    LWARN("MainWindow::onSendButtonClicked", "打开端口 COM%d 失败，错误代码 %d", portNum, stat);
                    QMessageBox::warning(this, "错误", 
                        fmt::format("打开端口 COM{} 失败，错误信息: {}", 
                            portNum, string_utils::wstr2str(getErrorDescription(stat))).c_str());
                    label_7->setText(QString::fromStdString("<打开串口失败>"));
                    label_10->setText(QString::fromStdString(string_utils::wstr2str(getErrorDescription(stat))));
                    return;
                }
            }
            LINFO("MainWindow::onSendButtonClicked", "端口%d已打开，开始发送数据", portNum);
            portStatus[portNum] = true;
            LDEBUG("MainWindow::onSendButtonClicked", lastSent.to_hex_view());
            DWORD size = lastSent.size();
            int ret = portMap[portNum].Write(lastSent);
            LINFO("MainWindow::onSendButtonClicked", "发送数据完成，状态: %s", 
                    string_utils::wstr2str(getErrorDescription(ret)).c_str());
            label_7->setText(QString::fromStdString(
                time_utils::get_formatted_time_with_frac(time_utils::get_time(), "%Y-%m-%d %H:%M:%S.%f")));
            label_10->setText(QString::fromStdString(string_utils::wstr2str(getErrorDescription(ret))));
        } catch (const std::exception& e) {
            LERROR("MainWindow::onRecvButtonClicked", "发送数据时发生异常: %s", e.what());
            QMessageBox::critical(this, "错误", fmt::format("发送数据时发生异常: {}", e.what()).c_str());
        } catch (...) {
            LERROR("MainWindow::onRecvButtonClicked", "发送数据时发生未知异常");
            QMessageBox::critical(this, "错误", "发送数据时发生未知异常");
        }
        
    }

    void onRecvButtonClicked() {
        try {
            LINFO("MainWindow::onRecvButtonClicked", "接收按钮被点击");
            int portNum = getSelectedPort();
            if (portNum == -1) {
                LWARN("MainWindow::onRecvButtonClicked", "未选择端口，无法接收数据");
                QMessageBox::warning(this, "错误", "未选择端口，无法接收数据");
                return;
            }
            if (!portStatus[portNum]) {
                LINFO("MainWindow::onRecvButtonClicked", "端口%d未打开，尝试打开端口", portNum);
                portMap[portNum] = SerialPort();
                portMap[portNum].EnableBinaryLog();
                int stat = portMap[portNum].Open(portNum);
                if (stat != ERROR_SUCCESS) {
                    LWARN("MainWindow::onSendButtonClicked", fmt::format("打开端口 COM{} 失败，错误代码 {}", portNum, stat));
                    QMessageBox::warning(this, "错误", 
                    fmt::format("打开端口 COM{} 失败，错误信息: {}", 
                        portNum, string_utils::wstr2str(getErrorDescription(stat))).c_str());
                    label_9->setText(QString::fromStdString("<打开串口失败>"));
                    label_12->setText(QString::fromStdString(string_utils::wstr2str(getErrorDescription(stat))));
                    return;
                }
            }
            portStatus[portNum] = true;
            LINFO("MainWindow::onRecvButtonClicked", "端口%d已打开，开始接收数据，长度上限: %d", 
                portNum, checkBox_3->isChecked() ? spinBox->value() : -1);
            int ret = portMap[portNum].Read(lastReceived, checkBox_3->isChecked() ? spinBox->value() : -1);
            LDEBUG("MainWindow::onRecvButtonClicked", lastReceived.to_hex_view());
            LINFO("MainWindow::onRecvButtonClicked", "接收数据完成，状态: %s",
                    string_utils::wstr2str(getErrorDescription(ret)).c_str());
            label_9->setText(QString::fromStdString(
                time_utils::get_formatted_time_with_frac(time_utils::get_time(), "%Y-%m-%d %H:%M:%S.%f")));
            label_12->setText(QString::fromStdString(string_utils::wstr2str(getErrorDescription(ret))));
            if (checkBox_2->isChecked()) {
                textBrowser_2->setPlainText(QString::fromStdString(lastReceived.to_hex_view(8, false)));
            } else {
                textBrowser_2->setPlainText(QString::fromStdString(lastReceived.to_string()));
            }
        } catch (const std::exception& e) {
            LERROR("MainWindow::onRecvButtonClicked", "接收数据时发生异常: %s", e.what());
            QMessageBox::critical(this, "错误", fmt::format("接收数据时发生异常: {}", e.what()).c_str());
        } catch (...) {
            LERROR("MainWindow::onRecvButtonClicked", "接收数据时发生未知异常");
            QMessageBox::critical(this, "错误", "接收数据时发生未知异常");
        }
    
    }

    void onPortSelIdxChanged(int idx) {
        if (isRefreshingComboBox) return; // 防止不必要的调用
        int port = comboBox->itemData(idx).toInt();
        if (port == -1) {
            LINFO("MainWindow::onPortSelIdxChanged", "端口更改为未选择");
        } else {
            LINFO("MainWindow::onPortSelIdxChanged", fmt::format("端口更改为COM{}", port));
        }
    }

    void onOpenPortButtonClicked() {
        int selectedPort = getSelectedPort();
        if (selectedPort == -1) {
            LWARN("MainWindow::onOpenPortButtonClicked", "未选择端口，无法打开");
            QMessageBox::warning(this, "错误", "未选择端口，无法打开");
            return;
        }
        if (!portStatus[selectedPort]) {
            LINFO("MainWindow::onOpenPortButtonClicked", "尝试打开端口COM%d", selectedPort);
            portMap[selectedPort] = SerialPort();
            portMap[selectedPort].EnableBinaryLog();

            int ret = portMap[selectedPort].Open(selectedPort);
            if (ret != ERROR_SUCCESS) {
                LWARN("MainWindow::onOpenPortButtonClicked", "打开端口COM%d失败，错误代码%d", selectedPort, ret);
                QMessageBox::warning(this, "错误",
                    fmt::format("打开端口COM{}失败，错误信息: {}", 
                        selectedPort, string_utils::wstr2str(getErrorDescription(ret))).c_str());
                return;
            }
            portStatus[selectedPort] = true;
        } else {
            LWARN("MainWindow::onOpenPortButtonClicked", "端口COM%d已经打开，无需再次打开", selectedPort);
            QMessageBox::warning(this, "错误", 
                fmt::format("端口COM{}已经打开，无需再次打开", selectedPort).c_str());
            return;
        }
        LINFO("MainWindow::onOpenPortButtonClicked", "端口COM%d打开成功", selectedPort);
    }

    void onClosePortButtonClicked() {
        int selectedPort = getSelectedPort();
        if (selectedPort == -1) {
            LWARN("MainWindow::onClosePortButtonClicked", "未选择端口，无法关闭");
            QMessageBox::warning(this, "错误", "未选择端口，无法关闭");
            return;
        }
        if (portStatus[selectedPort]) {
            LINFO("MainWindow::onClosePortButtonClicked", "尝试关闭端口COM%d", selectedPort);
            int ret = portMap[selectedPort].Close();
            if (ret != ERROR_SUCCESS) {
                LWARN("MainWindow::onClosePortButtonClicked", "关闭端口COM%d失败，错误代码%d", selectedPort, ret);
                QMessageBox::warning(this, "错误", 
                    fmt::format("关闭端口COM{}失败，错误信息: {}", 
                        selectedPort, string_utils::wstr2str(getErrorDescription(ret))).c_str());
                return;
            }
        } else {
            LWARN("MainWindow::onClosePortButtonClicked", "端口COM%d已经关闭，无需再次关闭", selectedPort);
            QMessageBox::warning(this, "错误",
                fmt::format("端口COM{}已经关闭，无需再次关闭", selectedPort).c_str());
            return;
        }
        portStatus[selectedPort] = false;
        LINFO("MainWindow::onClosePortButtonClicked", "端口COM%d关闭成功", selectedPort);
    }

    void updateComboBox() {
        isRefreshingComboBox = true;
        auto ports = usb_utils::GetCOMPorts();
        int selectedPort = comboBox->currentData().toInt();
        comboBox->clear();
        comboBox->addItem("<未选择>", -1);
        bool stillHasSelectedPort = false;
        if (selectedPort == -1) stillHasSelectedPort = true; // "未选择"这个选项是一直存在的
        for (auto port : ports) {
            if (port.portNumber == selectedPort)
                stillHasSelectedPort = true;
            comboBox->addItem(
                QString::fromStdString(
                    fmt::sprintf("COM%-2d %s", port.portNumber, port.deviceName)
                ),
                port.portNumber
            );
        }
        if (stillHasSelectedPort) {
            for (int i = 0; i < comboBox->count(); i++) {
                if (comboBox->itemData(i).toInt() == selectedPort) {
                    comboBox->setCurrentIndex(i);
                    break;
                }
            }
        } else {
            if (!isFirstComboBoxUpdate) {
                LWARN("MainWindow::updateComboBox",
                    fmt::format("之前选择的端口 (COM{}) 不再可用，重置为未选择状态", selectedPort));
                isRefreshingComboBox = false;
                comboBox->setCurrentIndex(0);
            }
        }
        isFirstComboBoxUpdate = false;
        isRefreshingComboBox = false;

    }

    int getSelectedPort() {
        return comboBox->currentData().toInt();
    }
};

int main(int argc, char* argv[]) {
    #if 1
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("串口调试工具");
    QCoreApplication::setApplicationVersion("11.45.14");
    MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
    #else
    return emmcdl_main(argc, argv);
    #endif
    return 0;
}
