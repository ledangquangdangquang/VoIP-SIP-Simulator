#pragma once

#include <QMainWindow>
#include <QDebug>
#include <QTime>
#include <QLabel>
#include <QTextEdit>
#include <QApplication>
#include <pjsua2.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ==================== LỚP XỬ LÝ CUỘC GỌI (CALL CALLBACK) ====================
class MyCall : public pj::Call {
public:
    MyCall(pj::Account &acc, int call_id = PJSUA_INVALID_ID) : pj::Call(acc, call_id) {}
    virtual void onCallState(pj::OnCallStateParam &prm) override;
};

// ==================== LỚP XỬ LÝ TÀI KHOẢN (ACCOUNT CALLBACK) ====================
class MyAccount : public pj::Account {
public:
    MyAccount() : pj::Account() {}
    virtual void onRegState(pj::OnRegStateParam &prm) override;

    // ĐỔI THÀNH HÀM NÀY: Hàm chuẩn PJSUA2 xử lý sự kiện khi có thực thể khác tương tác cuộc gọi tới
    virtual void onIncomingCall(pj::OnIncomingCallParam &prm) override;
};

// ==================== LỚP GIAO DIỆN CHÍNH (MAINWINDOW) ====================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnCall_clicked();
    void on_btnHangup_clicked();

private:
    Ui::MainWindow *ui;
    pj::Endpoint ep;
    MyAccount* account = nullptr;
    MyCall* currentCall = nullptr;

    void initPjsip();
};
