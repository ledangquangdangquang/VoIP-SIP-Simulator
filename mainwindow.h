#pragma once

#include <QMainWindow>
#include <QDebug>
#include <pjsua2.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MyCall : public pj::Call {
public:
    MyCall(pj::Account &acc, int call_id = PJSUA_INVALID_ID) : pj::Call(acc, call_id) {}
    virtual void onCallState(pj::OnCallStateParam &prm) override;
};

// Đã lược bỏ MyAccount vì gọi trực tiếp IP-to-IP không cần tài khoản đăng ký server

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // CHỈ GIỮ LẠI 2 DÒNG NÀY (Đã xóa dòng btnRegister)
    void on_btnCall_clicked();
    void on_btnHangup_clicked();

private:
    Ui::MainWindow *ui;
    pj::Endpoint ep;
    MyCall* currentCall = nullptr;

    void initPjsip();
};
