#include "mainwindow.h"
#include "networkstatswidget.h"
#include "sipsequencewidget.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QMetaObject>
#include <QNetworkInterface>
#include <QScrollBar>
#include <QTextStream>
#include <QThread>
#include <QUdpSocket>

using namespace pj;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), localIpTimer(this),
      networkStatsTimer(this)
{
    ui->setupUi(this);
    localIpTimer.setInterval(2000);
    connect(&localIpTimer, &QTimer::timeout, this, &MainWindow::updateLocalIp);
    updateLocalIp();
    localIpTimer.start();
    networkStatsTimer.setInterval(1000);
    connect(&networkStatsTimer, &QTimer::timeout, this, &MainWindow::updateNetworkStats);
    statusBar()->showMessage("Dashboard sẵn sàng");
    initPjsip();
}

void MainWindow::updateLocalIp() {
    QHostAddress localAddress;

    // A connected UDP socket reveals the address selected by the current
    // default route without sending any packet.
    QUdpSocket routeProbe;
    routeProbe.connectToHost(QHostAddress("8.8.8.8"), 53);
    if (routeProbe.waitForConnected(100)) {
        localAddress = routeProbe.localAddress();
    }

    // Fall back to the first active non-loopback IPv4 interface when there is
    // no default route (for example, on an isolated SIP lab network).
    if (localAddress.isNull() || localAddress.isLoopback()) {
        const auto interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &networkInterface : interfaces) {
            const auto flags = networkInterface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp)
                || !flags.testFlag(QNetworkInterface::IsRunning)
                || flags.testFlag(QNetworkInterface::IsLoopBack)) {
                continue;
            }

            for (const QNetworkAddressEntry &entry : networkInterface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    localAddress = entry.ip();
                    break;
                }
            }
            if (!localAddress.isNull()) break;
        }
    }

    const QString displayAddress = localAddress.isNull()
        ? QStringLiteral("Không có kết nối mạng")
        : QStringLiteral("%1:5061").arg(localAddress.toString());
    if (ui->lblIP->text() != displayAddress) {
        ui->lblIP->setText(displayAddress);
    }
}

void MainWindow::startNetworkStats() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { startNetworkStats(); }, Qt::QueuedConnection);
        return;
    }
    if (shuttingDown) return;

    previousTxBytes = 0;
    previousRxBytes = 0;
    statsSampleTimer.restart();
    ui->networkStats->resetStats();
    networkStatsTimer.start();
    updateNetworkStats();
}

void MainWindow::updateNetworkStats() {
    if (!currentCall || !currentCall->isActive()) return;

    try {
        const CallInfo callInfo = currentCall->getInfo();
        for (const CallMediaInfo &mediaInfo : callInfo.media) {
            if (mediaInfo.type != PJMEDIA_TYPE_AUDIO
                || mediaInfo.status != PJSUA_CALL_MEDIA_ACTIVE) {
                continue;
            }

            const StreamStat stats = currentCall->getStreamStat(mediaInfo.index);
            const StreamInfo streamInfo = currentCall->getStreamInfo(mediaInfo.index);
            const quint64 txBytes = stats.rtcp.txStat.bytes;
            const quint64 rxBytes = stats.rtcp.rxStat.bytes;
            const qint64 elapsedMs = qMax<qint64>(1, statsSampleTimer.restart());

            NetworkStatsSample sample;
            if (previousTxBytes > 0 && txBytes >= previousTxBytes) {
                sample.uploadKbps = (txBytes - previousTxBytes) * 8.0 / elapsedMs;
            }
            if (previousRxBytes > 0 && rxBytes >= previousRxBytes) {
                sample.downloadKbps = (rxBytes - previousRxBytes) * 8.0 / elapsedMs;
            }
            previousTxBytes = txBytes;
            previousRxBytes = rxBytes;

            sample.txPackets = stats.rtcp.txStat.pkt;
            sample.rxPackets = stats.rtcp.rxStat.pkt;
            const quint64 expectedRx = stats.rtcp.rxStat.pkt + stats.rtcp.rxStat.loss;
            if (expectedRx > 0) {
                sample.packetLossPercent = stats.rtcp.rxStat.loss * 100.0 / expectedRx;
            }
            sample.jitterMs = stats.rtcp.rxStat.jitterUsec.mean / 1000.0;
            sample.rttMs = stats.rtcp.rttUsec.mean / 1000.0;
            sample.codec = QString::fromStdString(streamInfo.codecName);
            const long duration = callInfo.connectDuration.sec;
            sample.duration = QString("%1:%2")
                                  .arg(duration / 60, 2, 10, QLatin1Char('0'))
                                  .arg(duration % 60, 2, 10, QLatin1Char('0'));
            ui->networkStats->updateStats(sample);
            return;
        }
    } catch (Error &err) {
        qDebug() << "Không thể đọc RTP statistics:" << err.info().c_str();
    }
}

void MainWindow::appendLog(const QString &category, const QString &message) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, category, message]() {
            appendLog(category, message);
        }, Qt::QueuedConnection);
        return;
    }
    if (shuttingDown) return;

    const QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    logEntries.append({category, QString("[%1] [%2] %3").arg(timestamp, category, message)});
    refreshLogView();
}

void MainWindow::appendSequenceEvent(const QString &from, const QString &to,
                                     const QString &label, const QString &kind) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, from, to, label, kind]() {
            appendSequenceEvent(from, to, label, kind);
        }, Qt::QueuedConnection);
        return;
    }
    if (shuttingDown) return;

    ui->sipSequence->addMessage(from, to, label, kind);
    ui->sequenceScrollArea->verticalScrollBar()->setValue(
        ui->sequenceScrollArea->verticalScrollBar()->maximum());
}

void MainWindow::updateRegistrationState(bool active, int statusCode) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, active, statusCode]() {
            updateRegistrationState(active, statusCode);
        }, Qt::QueuedConnection);
        return;
    }
    if (shuttingDown) return;

    if (active) {
        ui->lblStatus->setText("Đang hoạt động (ONLINE)");
        ui->lblStatus->setStyleSheet("background: #dcfce7; color: #15803d; border-radius: 9px; padding: 5px 10px; font-weight: 700;");
        appendLog("SIP", "REGISTER thành công — Asterisk phản hồi 200 OK");
        appendSequenceEvent("Asterisk", "1001", "200 OK / REGISTER", "Response");
    } else if (statusCode == PJSIP_SC_UNAUTHORIZED
               || statusCode == PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED) {
        ui->lblStatus->setText("ĐANG XÁC THỰC");
        ui->lblStatus->setStyleSheet("background: #fef3c7; color: #b45309; border-radius: 9px; padding: 5px 10px; font-weight: 700;");
        appendLog("SIP", QString::number(statusCode) + " Digest challenge — đang gửi lại thông tin xác thực");
        appendSequenceEvent("Asterisk", "1001", QString::number(statusCode) + " Digest challenge", "Challenge");
        appendSequenceEvent("1001", "Asterisk", "REGISTER + Authorization", "Request");
    } else {
        ui->lblStatus->setText("Mất kết nối (Lỗi " + QString::number(statusCode) + ")");
        ui->lblStatus->setStyleSheet("background: #fee2e2; color: #b91c1c; border-radius: 9px; padding: 5px 10px; font-weight: 700;");
        appendLog("ERROR", "REGISTER thất bại với mã " + QString::number(statusCode));
        appendSequenceEvent("Asterisk", "1001", QString::number(statusCode) + " / REGISTER", "Error");
    }
}

void MainWindow::adoptIncomingCall(MyCall *call) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, call]() { adoptIncomingCall(call); },
                                  Qt::QueuedConnection);
        return;
    }
    if (shuttingDown) {
        delete call;
        return;
    }

    if (currentCall && currentCall != call) {
        currentCall->detachWindow();
        delete currentCall;
    }
    currentCall = call;
}

void MainWindow::releaseCall(MyCall *call) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, call]() { releaseCall(call); },
                                  Qt::QueuedConnection);
        return;
    }

    if (currentCall == call) {
        networkStatsTimer.stop();
        ui->networkStats->setInactive();
        currentCall = nullptr;
    }
    call->detachWindow();
    delete call;
}

void MainWindow::refreshLogView() {
    ui->txtPacketLog->clear();

    for (const LogEntry &entry : logEntries) {
        const bool visible = (entry.category == "SIP" && ui->chkSip->isChecked())
            || (entry.category == "RTP" && ui->chkRtp->isChecked())
            || (entry.category == "ERROR" && ui->chkError->isChecked());
        if (!visible) continue;

        QString color = "#bfdbfe";
        if (entry.category == "RTP") color = "#86efac";
        if (entry.category == "ERROR") color = "#fca5a5";
        ui->txtPacketLog->append(QString("<span style=\"color:%1;\">%2</span>")
                                     .arg(color, entry.text.toHtmlEscaped()));
    }
}

MainWindow::~MainWindow() {
    shuttingDown = true;
    try {
        if (currentCall) {
            currentCall->detachWindow();
            if (currentCall->isActive()) {
                CallOpParam prm;
                currentCall->hangup(prm);
            }
            delete currentCall;
            currentCall = nullptr;
        }
        if (account) {
            account->detachWindow();
            delete account;
            account = nullptr;
        }
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
        ep.transportCreate(PJSIP_TRANSPORT_UDP, t_cfg);

        // 3. Khởi động thư viện lõi
        ep.libStart();
        qDebug() << "Lõi PJSIP của Client đã bật trên cổng 5061 mạng thật!";
        appendLog("SIP", "PJSIP endpoint đã khởi động qua UDP cổng 5061");

        // 4. Cấu hình gửi lệnh Đăng ký tài khoản 1001 lên tổng đài Asterisk
        AccountConfig acfg;
        acfg.idUri = "sip:1001@192.168.1.8";                  // Địa chỉ IP máy ảo Ubuntu hiện tại
        acfg.regConfig.registrarUri = "sip:192.168.1.8:5060";     // Cổng 5060 của Asterisk chạy ngầm

        // Xác thực MD5 Khớp cấu hình Realm="asterisk" của tổng đài
        AuthCredInfo cred("digest", "asterisk", "1001", 0, "123456");
        acfg.sipConfig.authCreds.push_back(cred);

        // Tạo thực thể tài khoản kết nối ngầm
        account = new MyAccount(this);
        account->create(acfg);
        appendLog("SIP", "Đang đăng ký extension 1001 với 192.168.1.8:5060");
        appendSequenceEvent("1001", "Asterisk", "REGISTER", "Request");

    } catch (Error& err) {
        qDebug() << "Lỗi khởi tạo hệ thống VoIP:" << err.info().c_str();
        appendLog("ERROR", QString("Không thể khởi tạo PJSIP: %1").arg(err.info().c_str()));
    }
}

// Bấm nút Gọi: Bắn gói tin INVITE lên Asterisk để nhờ nối máy sang số 1002
void MainWindow::on_btnCall_clicked() {
    if (!account) return;
    if (currentCall && currentCall->isActive()) {
        appendLog("ERROR", "Đang có một cuộc gọi khác hoạt động");
        return;
    }
    try {
        if (currentCall) {
            currentCall->detachWindow();
            delete currentCall;
        }
        currentCall = new MyCall(*account, this);
        CallOpParam prm(true); // Tự động kích hoạt loa và mic mạng thật (RTP Audio)

        // Khi bấm gọi, lệnh INVITE gửi thẳng lên Asterisk (cổng 5060)
        currentCall->makeCall("sip:1002@192.168.1.8:5060", prm);
        qDebug() << "Đang bắn lệnh INVITE lên Server Asterisk để tìm số 1002...";
        appendLog("SIP", "Đã gửi INVITE tới sip:1002@192.168.1.8:5060");
        appendSequenceEvent("1001", "Asterisk", "INVITE sip:1002", "Request");
    } catch (Error& err) {
        qDebug() << "Lỗi gọi đi:" << err.info().c_str();
        appendLog("ERROR", QString("Không thể thực hiện cuộc gọi: %1").arg(err.info().c_str()));
        if (currentCall) {
            currentCall->detachWindow();
            delete currentCall;
            currentCall = nullptr;
        }
    }
}

// Bấm nút dập máy: Ra lệnh ngắt kết nối mạng an toàn (Không tự ý delete luôn tránh crash app)
void MainWindow::on_btnHangup_clicked() {
    if (currentCall) {
        try {
            CallOpParam prm;
            prm.statusCode = PJSIP_SC_DECLINE;
            currentCall->markLocalHangup();
            currentCall->hangup(prm);
            qDebug() << "Đang gửi lệnh yêu cầu dập máy...";
            appendLog("SIP", "Đã gửi yêu cầu kết thúc cuộc gọi");
            appendSequenceEvent("1001", "Asterisk", "BYE", "Request");
        } catch (Error& err) {
            qDebug() << "Lỗi dập máy:" << err.info().c_str();
            appendLog("ERROR", QString("Không thể kết thúc cuộc gọi: %1").arg(err.info().c_str()));
        }
    }
}

void MainWindow::on_btnClearLog_clicked() {
    logEntries.clear();
    refreshLogView();
    statusBar()->showMessage("Đã xóa log", 3000);
}

void MainWindow::on_btnClearSequence_clicked() {
    ui->sipSequence->clearMessages();
    statusBar()->showMessage("Đã xóa sequence diagram", 3000);
}

void MainWindow::on_btnExportLog_clicked() {
    const QString fileName = QFileDialog::getSaveFileName(
        this, "Xuất SIP log", "sip-debug.log", "Log files (*.log);;Text files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Không thể xuất log", "Không thể ghi vào tệp đã chọn.");
        return;
    }

    QTextStream stream(&file);
    for (const LogEntry &entry : logEntries) stream << entry.text << '\n';
    statusBar()->showMessage("Đã xuất log tới " + fileName, 5000);
}

void MainWindow::on_chkSip_toggled(bool) {
    refreshLogView();
}

void MainWindow::on_chkRtp_toggled(bool) {
    refreshLogView();
}

void MainWindow::on_chkError_toggled(bool) {
    refreshLogView();
}

// ==================== ĐỊNH NGHĨA CÁC HÀM XỬ LÝ CALLBACK SỰ KIỆN ====================

// Callback theo dõi trạng thái Đăng ký (ONLINE/OFFLINE)
void MyAccount::onRegState(OnRegStateParam &prm) {
    AccountInfo ai = getInfo();

    if (ai.regIsActive) {
        qDebug() << ">>> [THÀNH CÔNG] Asterisk đã chấp nhận tài khoản 1001! (Nhận bản tin 200 OK)";
        if (mainWindow) mainWindow->updateRegistrationState(true, ai.regStatus);
    }
    else if (ai.regStatus >= 300) { // Chỉ báo lỗi thực tế khi bắt tay hoàn tất thất bại (Tránh mã 401 tạm thời)
        qDebug() << ">>> [THẤT BẠI] Đăng ký lên Asterisk không thành công, mã lỗi:" << ai.regStatus;
        if (mainWindow) mainWindow->updateRegistrationState(false, ai.regStatus);
    }
}

// Callback lắng nghe, bóc tách và in dòng chảy gói tin SIP thời gian thực lên TextEdit của App
// [SỬA LỖI TẠI ĐÂY]: Thay thế toàn bộ hàm onIncomingRequest cũ bằng hàm chuẩn này
void MyAccount::onIncomingCall(OnIncomingCallParam &prm) {
    MyCall *call = new MyCall(*this, mainWindow.data(), prm.callId, true);
    try {
        if (!mainWindow) {
            CallOpParam rejectParam;
            rejectParam.statusCode = PJSIP_SC_TEMPORARILY_UNAVAILABLE;
            call->answer(rejectParam);
            delete call;
            return;
        }

        const CallInfo ci = call->getInfo();
        mainWindow->adoptIncomingCall(call);
        mainWindow->appendLog("SIP", "Nhận INVITE từ " + QString::fromStdString(ci.remoteUri));
        mainWindow->appendSequenceEvent("Asterisk", "1001", "INVITE từ 1002", "Request");

        CallOpParam answerParam;
        answerParam.statusCode = PJSIP_SC_OK;
        call->answer(answerParam);
        mainWindow->appendSequenceEvent("1001", "Asterisk", "200 OK", "Response");
    } catch (Error &err) {
        if (mainWindow) {
            mainWindow->appendLog("ERROR", QString("Không thể nhận cuộc gọi: %1").arg(err.info().c_str()));
            mainWindow->releaseCall(call);
        } else {
            delete call;
        }
    }
}
// Callback theo dõi trạng thái Cuộc gọi thoại
void MyCall::onCallState(OnCallStateParam &prm) {
    Q_UNUSED(prm)
    CallInfo ci = getInfo();

    if (!incoming && ci.lastStatusCode > 0 && ci.lastStatusCode != lastVisualizedStatus
        && ci.state != PJSIP_INV_STATE_DISCONNECTED) {
        lastVisualizedStatus = ci.lastStatusCode;
        const QString reason = QString::fromStdString(ci.lastReason);
        const QString label = QString::number(ci.lastStatusCode) + " " + reason;
        const QString kind = ci.lastStatusCode >= 400 ? "Error" : "Response";
        if (mainWindow) mainWindow->appendSequenceEvent("Asterisk", "1001", label, kind);
    }

    if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        wasConfirmed = true;
        qDebug() << ">>> ĐÃ THÔNG CUỘC GỌI VỚI MÁY 1002 QUA SERVER ASTERISK!";
        if (mainWindow) {
            mainWindow->startNetworkStats();
            mainWindow->appendLog("SIP", "Cuộc gọi đã kết nối");
            if (incoming) {
                mainWindow->appendSequenceEvent("Asterisk", "1001", "ACK", "Request");
            } else {
                mainWindow->appendSequenceEvent("1001", "Asterisk", "ACK", "Request");
            }
        }
    }
    // [TỐI ƯU BỘ NHỚ]: Tự động giải phóng con trỏ cuộc gọi khi bản tin kết thúc cuộc gọi hoàn thành hẳn
    else if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        qDebug() << ">>> Cuộc gọi kết thúc hoàn toàn (Đã nhận bản tin BYE/CANCEL).";
        if (mainWindow) {
            mainWindow->appendLog("SIP", "Cuộc gọi đã ngắt (BYE/CANCEL)");
            if (wasConfirmed && localHangup) {
                mainWindow->appendSequenceEvent("Asterisk", "1001", "200 OK / BYE", "Response");
            } else if (wasConfirmed) {
                mainWindow->appendSequenceEvent("Asterisk", "1001", "BYE", "Request");
                mainWindow->appendSequenceEvent("1001", "Asterisk", "200 OK", "Response");
            } else if (ci.lastStatusCode > 0 && ci.lastStatusCode != lastVisualizedStatus) {
                const QString kind = ci.lastStatusCode >= 400 ? "Error" : "Response";
                mainWindow->appendSequenceEvent(
                    "Asterisk", "1001",
                    QString::number(ci.lastStatusCode) + " " + QString::fromStdString(ci.lastReason),
                    kind);
            }
            mainWindow->releaseCall(this);
        }
    }
}

void MyCall::onCallMediaState(OnCallMediaStateParam &prm) {
    Q_UNUSED(prm)

    try {
        const CallInfo ci = getInfo();
        for (const CallMediaInfo &mediaInfo : ci.media) {
            if (mediaInfo.type != PJMEDIA_TYPE_AUDIO
                || mediaInfo.status != PJSUA_CALL_MEDIA_ACTIVE) {
                continue;
            }

            AudioMedia callAudio = getAudioMedia(static_cast<int>(mediaInfo.index));
            AudDevManager &audioDevices = Endpoint::instance().audDevManager();
            audioDevices.getCaptureDevMedia().startTransmit(callAudio);
            callAudio.startTransmit(audioDevices.getPlaybackDevMedia());
            if (mainWindow) {
                mainWindow->appendLog("RTP", "Luồng âm thanh RTP đã kết nối với mic và loa");
                mainWindow->appendSequenceEvent("1001", "1002", "RTP audio (UDP)", "Media");
            }
        }
    } catch (Error &err) {
        if (mainWindow) {
            mainWindow->appendLog("ERROR", QString("Không thể kết nối thiết bị âm thanh: %1")
                                               .arg(err.info().c_str()));
        }
    }
}
