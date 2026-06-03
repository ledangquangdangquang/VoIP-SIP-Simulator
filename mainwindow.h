#pragma once

#include <QMainWindow>
#include <QDebug>
#include <pjsua2.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 1. Kế thừa lớp Call của PJSIP để quản lý cuộc gọi
class MyCall : public pj::Call {
public:
    MyCall(pj::Account &acc, int call_id = PJSUA_INVALID_ID) : pj::Call(acc, call_id) {}

    // Callback này tự động chạy khi cuộc gọi thay đổi trạng thái (đổ chuông, nhấc máy, dập máy)
    virtual void onCallState(pj::OnCallStateParam &prm) override;
};

// 2. Kế thừa lớp Account của PJSIP để quản lý tài khoản Đăng ký (Register)
class MyAccount : public pj::Account {
public:
    MyAccount() {}

    // Callback này tự động chạy khi trạng thái Đăng ký lên Asterisk thay đổi
    virtual void onRegState(pj::OnRegStateParam &prm) override;

    // Callback này tự động chạy khi có người khác gọi đến số của bạn (INVITE đến)
    virtual void onIncomingCall(pj::OnIncomingCallParam &prm) override;
};

// 3. Lớp giao diện MainWindow chính của Qt
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Các Slot xử lý khi người dùng bấm nút trên giao diện Qt
    void on_btnRegister_clicked();
    void on_btnCall_clicked();
    void on_btnHangup_clicked();

private:
    Ui::MainWindow *ui;

    // Khai báo các đối tượng của PJSIP
    pj::Endpoint ep;
    MyAccount* account = nullptr;
    MyCall* currentCall = nullptr;

    void initPjsip(); // Hàm khởi tạo lõi PJSIP
};
