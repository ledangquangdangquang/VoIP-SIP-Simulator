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
        if (currentCall) {
            // Kiểm tra trạng thái nếu đang kết nối thì dập máy trước khi hủy app
            CallOpParam prm;
            currentCall->hangup(prm);
            delete currentCall;
        }
        if (account) delete account;
        ep.libDestroy();
    } catch (Error& err) {
        qDebug() << "Lỗi khi hủy thư viện PJSIP:" << err.info().c_str();
    }
    delete ui;
}

void MainWindow::initPjsip() {
    try {
        // 1. Tạo lõi thực thi PJSIP Endpoint
        ep.libCreate();

        EpConfig ep_cfg;
        ep_cfg.logConfig.level = 4; // Bật log bắt các bản tin SIP chạy ngầm
        ep.libInit(ep_cfg);

        // 2. Tạo Transport cổng mạng (Ứng dụng chiếm cổng 5061, nhường 5060 cho Server Asterisk)
        TransportConfig t_cfg;
        t_cfg.port = 5061;
        TransportId tid = ep.transportCreate(PJSIP_TRANSPORT_UDP, t_cfg);

        // 3. Khởi động thư viện lõi
        ep.libStart();
        qDebug() << "Lõi PJSIP của Client đã bật trên cổng 5061 mạng thật!";

        // 4. [SỬA LỖI TẠI ĐÂY]: Lấy thông tin địa chỉ string trực tiếp từ localName của PJSUA2
        TransportInfo t_info = ep.transportGetInfo(tid);
        QString localIpPort = QString::fromStdString(t_info.localName); // localName bản chất đã là chuỗi "IP:Port"

        if (ui->lblIP) {
            ui->lblIP->setText(localIpPort);
        }

        // 5. Cấu hình gửi lệnh Đăng ký tài khoản 1001 lên tổng đài Asterisk
        AccountConfig acfg;
        acfg.idUri = "sip:1001@192.168.1.8";                  // Địa chỉ IP máy ảo Ubuntu hiện tại
        acfg.regConfig.registrarUri = "sip:192.168.1.8:5060";     // Cổng 5060 của Asterisk chạy ngầm

        // Xác thực MD5 Khớp cấu hình Realm="asterisk" của tổng đài
        AuthCredInfo cred("digest", "asterisk", "1001", 0, "123456");
        acfg.sipConfig.authCreds.push_back(cred);

        // Tạo thực thể tài khoản kết nối ngầm
        account = new MyAccount();
        account->create(acfg);

    } catch (Error& err) {
        qDebug() << "Lỗi khởi tạo hệ thống VoIP:" << err.info().c_str();
    }
}

// Bấm nút Gọi: Bắn gói tin INVITE lên Asterisk để nhờ nối máy sang số 1002
void MainWindow::on_btnCall_clicked() {
    if (!account) return;
    try {
        currentCall = new MyCall(*account);
        CallOpParam prm(true); // Tự động kích hoạt loa và mic mạng thật (RTP Audio)

        // Khi bấm gọi, lệnh INVITE gửi thẳng lên Asterisk (cổng 5060)
        currentCall->makeCall("sip:1002@192.168.1.8:5060", prm);
        qDebug() << "Đang bắn lệnh INVITE lên Server Asterisk để tìm số 1002...";
    } catch (Error& err) {
        qDebug() << "Lỗi gọi đi:" << err.info().c_str();
    }
}

// Bấm nút dập máy: Ra lệnh ngắt kết nối mạng an toàn (Không tự ý delete luôn tránh crash app)
void MainWindow::on_btnHangup_clicked() {
    if (currentCall) {
        try {
            CallOpParam prm;
            prm.statusCode = PJSIP_SC_DECLINE;
            currentCall->hangup(prm);
            qDebug() << "Đang gửi lệnh yêu cầu dập máy...";
        } catch (Error& err) {
            qDebug() << "Lỗi dập máy:" << err.info().c_str();
        }
    }
}

// ==================== ĐỊNH NGHĨA CÁC HÀM XỬ LÝ CALLBACK SỰ KIỆN ====================

// Callback theo dõi trạng thái Đăng ký (ONLINE/OFFLINE)
void MyAccount::onRegState(OnRegStateParam &prm) {
    AccountInfo ai = getInfo();

    // Tìm kiếm con trỏ Main GUI Window đang hoạt động động để cập nhật trạng thái trực quan
    MainWindow *mainWin = qobject_cast<MainWindow*>(QApplication::activeWindow());

    if (ai.regIsActive) {
        qDebug() << ">>> [THÀNH CÔNG] Asterisk đã chấp nhận tài khoản 1001! (Nhận bản tin 200 OK)";
        if (mainWin) {
            QLabel* lblStatus = mainWin->findChild<QLabel*>("lblStatus");
            if (lblStatus) {
                lblStatus->setText("Đang hoạt động (ONLINE)");
                lblStatus->setStyleSheet("color: green; font-weight: bold;");
            }
        }
    }
    else if (ai.regStatus >= 300) { // Chỉ báo lỗi thực tế khi bắt tay hoàn tất thất bại (Tránh mã 401 tạm thời)
        qDebug() << ">>> [THẤT BẠI] Đăng ký lên Asterisk không thành công, mã lỗi:" << ai.regStatus;
        if (mainWin) {
            QLabel* lblStatus = mainWin->findChild<QLabel*>("lblStatus");
            if (lblStatus) {
                lblStatus->setText("Mất kết nối (Lỗi " + QString::number(ai.regStatus) + ")");
                lblStatus->setStyleSheet("color: red; font-weight: bold;");
            }
        }
    }
}

// Callback lắng nghe, bóc tách và in dòng chảy gói tin SIP thời gian thực lên TextEdit của App
// [SỬA LỖI TẠI ĐÂY]: Thay thế toàn bộ hàm onIncomingRequest cũ bằng hàm chuẩn này
void MyAccount::onIncomingCall(OnIncomingCallParam &prm) {
    // Tạo đối tượng cuộc gọi mới từ tham số truyền vào
    MyCall *call = new MyCall(*this, prm.callId);
    CallInfo ci = call->getInfo();

    // Lấy thông tin người gọi tới
    QString fromUri = QString::fromStdString(ci.remoteUri);

    // In thông tin lên ô log giao diện UI của App Qt
    MainWindow *mainWin = qobject_cast<MainWindow*>(QApplication::activeWindow());
    if (mainWin) {
        QTextEdit *logBox = mainWin->findChild<QTextEdit*>("txtPacketLog");
        if (logBox) {
            QString logLine = QString("[%1] Nhận tín hiệu gọi tới (INVITE) từ: %2")
                                  .arg(QTime::currentTime().toString("hh:mm:ss"))
                                  .arg(fromUri);
            logBox->append(logLine);
        }
    }
}
// Callback theo dõi trạng thái Cuộc gọi thoại
void MyCall::onCallState(OnCallStateParam &prm) {
    CallInfo ci = getInfo();
    if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        qDebug() << ">>> ĐÃ THÔNG CUỘC GỌI VỚI MÁY 1002 QUA SERVER ASTERISK!";
    }
    // [TỐI ƯU BỘ NHỚ]: Tự động giải phóng con trỏ cuộc gọi khi bản tin kết thúc cuộc gọi hoàn thành hẳn
    else if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        qDebug() << ">>> Cuộc gọi kết thúc hoàn toàn (Đã nhận bản tin BYE/CANCEL).";
        delete this;
    }
}
