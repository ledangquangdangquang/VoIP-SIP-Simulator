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
        ep.libDestroy();
    } catch (Error& err) {}
    delete ui;
}

void MainWindow::initPjsip() {
    try {
        ep.libCreate();
        EpConfig ep_cfg;
        ep_cfg.logConfig.level = 4; // Giữ log để xem các dòng chữ INVITE chạy
        ep.libInit(ep_cfg);

        // Ứng dụng Qt của bạn sẽ chiếm giữ cổng UDP 5060
        TransportConfig t_cfg;
        t_cfg.port = 5061;
        ep.transportCreate(PJSIP_TRANSPORT_UDP, t_cfg);

        ep.libStart();
        qDebug() << "Lõi PJSIP đã bật trên cổng 5061!";
    } catch (Error& err) {
        qDebug() << "Lỗi khởi tạo:" << err.info().c_str();
    }
}

// Bấm nút Gọi: Bắn gói tin thẳng sang cổng 5062 của Linphone trên cùng máy ảo
void MainWindow::on_btnCall_clicked() {
    try {
        // Tạo một tài khoản ảo chạy ngầm trong bộ nhớ để làm bệ phóng cuộc gọi
        AccountConfig a_cfg;
        a_cfg.idUri = "sip:101@127.0.0.1";
        Account *local_acc = new Account();
        local_acc->create(a_cfg);

        currentCall = new MyCall(*local_acc);
        CallOpParam prm(true); // Tự động bật Micro và Loa (RTP)

        // ĐIỂM CHỐT: Gọi thẳng tới localhost (127.0.0.1) tại cổng 5062 của Linphone
        currentCall->makeCall("sip:102@127.0.0.1:5062", prm);
        qDebug() << "Đang bắn lệnh INVITE tới Linphone...";
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
            qDebug() << "Đã dập máy (Gửi lệnh BYE)!";
        } catch (Error& err) {}
    }
}

// Hàm xử lý trạng thái cuộc gọi
void MyCall::onCallState(OnCallStateParam &prm) {
    CallInfo ci = getInfo();
    if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        qDebug() << ">>> KẾT NỐI THÀNH CÔNG! Luồng âm thanh RTP đang chạy trực tiếp.";
    }
}
