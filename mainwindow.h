#pragma once

#include <QMainWindow>
#include <QDebug>
#include <QTime>
#include <QLabel>
#include <QTextEdit>
#include <QApplication>
#include <QPointer>
#include <QString>
#include <QVector>
#include <pjsua2.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MyAccount;
class MyCall;

// ==================== LỚP GIAO DIỆN CHÍNH (MAINWINDOW) ====================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Các hàm này an toàn khi được gọi từ callback thread của PJSIP.
    void appendLog(const QString &category, const QString &message);
    void appendSequenceEvent(const QString &from, const QString &to,
                             const QString &label, const QString &kind);
    void updateRegistrationState(bool active, int statusCode);
    void adoptIncomingCall(MyCall *call);
    void releaseCall(MyCall *call);

private slots:
    void on_btnCall_clicked();
    void on_btnHangup_clicked();
    void on_btnClearLog_clicked();
    void on_btnExportLog_clicked();
    void on_btnClearSequence_clicked();
    void on_chkSip_toggled(bool checked);
    void on_chkRtp_toggled(bool checked);
    void on_chkError_toggled(bool checked);

private:
    struct LogEntry {
        QString category;
        QString text;
    };

    Ui::MainWindow *ui;
    pj::Endpoint ep;
    MyAccount* account = nullptr;
    MyCall* currentCall = nullptr;
    QVector<LogEntry> logEntries;
    bool shuttingDown = false;

    void initPjsip();
    void refreshLogView();
};

// ==================== LỚP XỬ LÝ CUỘC GỌI (CALL CALLBACK) ====================
class MyCall : public pj::Call {
public:
    MyCall(pj::Account &acc, MainWindow *window, int callId = PJSUA_INVALID_ID,
           bool incomingCall = false)
        : pj::Call(acc, callId), mainWindow(window), incoming(incomingCall) {}

    void detachWindow() { mainWindow.clear(); }
    void markLocalHangup() { localHangup = true; }
    void onCallState(pj::OnCallStateParam &prm) override;
    void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

private:
    QPointer<MainWindow> mainWindow;
    int lastVisualizedStatus = 0;
    bool incoming = false;
    bool localHangup = false;
    bool wasConfirmed = false;
};

// ==================== LỚP XỬ LÝ TÀI KHOẢN (ACCOUNT CALLBACK) ====================
class MyAccount : public pj::Account {
public:
    explicit MyAccount(MainWindow *window) : mainWindow(window) {}

    void detachWindow() { mainWindow.clear(); }
    void onRegState(pj::OnRegStateParam &prm) override;
    void onIncomingCall(pj::OnIncomingCallParam &prm) override;

private:
    QPointer<MainWindow> mainWindow;
};
