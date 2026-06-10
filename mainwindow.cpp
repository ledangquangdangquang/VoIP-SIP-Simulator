#include "mainwindow.h"
#include "ui_mainwindow.h"

using namespace pj;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
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

        // Ứng dụng Qt Client chiếm cổng 5061 (Cổng 5060 đã nhường cho Server Asterisk chạy ngầm)
        TransportConfig t_cfg;
        t_cfg.port = 5061;
        ep.transportCreate(PJSIP_TRANSPORT_UDP, t_cfg);

        ep.libStart();
        qDebug() << "Lõi PJSIP của Client đã bật trên cổng 5061 mạng thật!";

        // --- GỬI LỆNH ĐĂNG KÝ TÀI KHOẢN 1001 VÀO TỔNG ĐÀI ASTERISK ---
        AccountConfig acfg;
        acfg.idUri = "sip:1001@192.168.1.8";              // Điền IP thật của máy ảo bạn (192.168.1.8)
        acfg.regConfig.registrarUri = "sip:192.168.1.8:5060"; // Chỉ rõ đích đến là cổng 5060 của Asterisk

        // AuthCredInfo cred("digest", "*", "1001", 0, "123456");
        AuthCredInfo cred("digest", "asterisk", "1001", 0, "123456");
        acfg.sipConfig.authCreds.push_back(cred);

        account = new MyAccount(); // Đảm bảo lớp MyAccount đã khai báo callback onRegState
        account->create(acfg);
    } catch (Error& err) {
        qDebug() << "Lỗi khởi tạo:" << err.info().c_str();
    }
}

// Bấm nút Gọi: Bắn gói tin INVITE lên Asterisk để nhờ nối máy sang số 1002
void MainWindow::on_btnCall_clicked() {
    if (!account) return;
    try {
        currentCall = new MyCall(*account);
        CallOpParam prm(true);

        // Khi bấm gọi, lệnh INVITE sẽ gửi lên Server Asterisk (cổng 5060) để nhờ chuyển tiếp sang máy 1002
        currentCall->makeCall("sip:1002@192.168.1.8:5060", prm);
        qDebug() << "Đang bắn lệnh INVITE lên Server Asterisk để tìm số 1002...";
    } catch (Error& err) {
        qDebug() << "Lỗi gọi:" << err.info().c_str();
    }
}

void MainWindow::on_btnHangup_clicked() {
    if (currentCall) {
        try {
            CallOpParam prm;
            currentCall->hangup(prm);
            delete currentCall;
            currentCall = nullptr;
            qDebug() << "Đã dập máy!";
        } catch (Error& err) {}
    }
}

// ---- CALLBACK THEO DÕI TRẠNG THÁI TỪ SERVER ----

void MyAccount::onRegState(OnRegStateParam &prm) {
    AccountInfo ai = getInfo();
    if (ai.regIsActive) {
        qDebug() << ">>> [THÀNH CÔNG] Asterisk đã chấp nhận tài khoản 1001! (Nhận bản tin 200 OK)";
    } else {
        qDebug() << ">>> [THẤT BẠI] Đăng ký lên Asterisk không thành công, mã lỗi:" << ai.regStatus;
    }
}

void MyCall::onCallState(OnCallStateParam &prm) {
    CallInfo ci = getInfo();
    if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        qDebug() << ">>> ĐÃ THÔNG CUỘC GỌI VỚI MÁY 1002 QUA SERVER ASTERISK!";
    }
}
