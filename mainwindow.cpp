#include "mainwindow.h"
#include "ui_mainwindow.h"

using namespace pj;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initPjsip();
}

MainWindow::~MainWindow() {
    try {
        if (currentCall) delete currentCall;
        if (account) delete account;
        ep.libDestroy();
    } catch (Error& err) {}
    delete ui;
}

void MainWindow::initPjsip() {
    try {
        ep.libCreate();
        EpConfig ep_cfg;
        ep_cfg.logConfig.level = 4;
        ep.libInit(ep_cfg);

        TransportConfig t_cfg;
        t_cfg.port = 5060;
        ep.transportCreate(PJSIP_TRANSPORT_UDP, t_cfg);

        ep.libStart();
        qDebug() << "Lõi PJSIP đã sẵn sàng!";
    } catch (Error& err) {
        qDebug() << "Lỗi khởi tạo PJSIP:" << err.info().c_str();
    }
}

void MainWindow::on_btnRegister_clicked() {
    try {
        AccountConfig acfg;
        acfg.idUri = "sip:101@192.168.1.100";
        acfg.regConfig.registrarUri = "sip:192.168.1.100";

        AuthCredInfo cred("digest", "*", "101", 0, "mat_khau_cua_ban");
        acfg.sipConfig.authCreds.push_back(cred);

        account = new MyAccount();
        account->create(acfg);
    } catch (Error& err) {
        qDebug() << "Lỗi đăng ký tài khoản:" << err.info().c_str();
    }
}

void MainWindow::on_btnCall_clicked() {
    if (!account) return;
    try {
        currentCall = new MyCall(*account);
        CallOpParam prm(true);
        currentCall->makeCall("sip:102@192.168.1.100", prm);
    } catch (Error& err) {
        qDebug() << "Lỗi thực hiện cuộc gọi:" << err.info().c_str();
    }
}

void MainWindow::on_btnHangup_clicked() {
    if (currentCall) {
        try {
            CallOpParam prm;
            prm.statusCode = PJSIP_SC_DECLINE;
            currentCall->hangup(prm);
            delete currentCall;
            currentCall = nullptr;
        } catch (Error& err) {}
    }
}

// ---- Hiện thực các Callback ----
void MyAccount::onRegState(OnRegStateParam &prm) {
    AccountInfo ai = getInfo();
    if (ai.regIsActive) {
        qDebug() << ">>> Đăng ký SIP thành công!";
    }
}

void MyAccount::onIncomingCall(OnIncomingCallParam &prm) {
    qDebug() << ">>> Có cuộc gọi đến!";
}

void MyCall::onCallState(OnCallStateParam &prm) {
    CallInfo ci = getInfo();
    if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        qDebug() << ">>> Đã kết nối thoại (RTP ok)!";
    }
}
