# Tài liệu học tập VoIP SIP Simulator

Tài liệu này giúp đọc code, hiểu kiến thức mạng đứng sau chương trình và chuẩn bị
cho phần demo, bảo vệ hoặc phỏng vấn. Nội dung mô tả đúng phiên bản code hiện tại.

## 1. Chương trình dùng để làm gì?

Đây là một SIP User Agent (UA) có giao diện Qt. Ứng dụng đóng vai extension
`1001`, đăng ký với tổng đài Asterisk, gọi extension `1002`, nhận cuộc gọi, nối
âm thanh từ microphone/loa vào luồng RTP và hiển thị thống kê mạng.

Ba thành phần trong mô hình:

```text
Qt/PJSUA2 client 1001       Asterisk             SIP client 1002
    UDP 5061          <-->  UDP 5060  <-->        cổng SIP của client
       microphone/loa <====== RTP/RTCP ======> microphone/loa
```

- **Qt** tạo giao diện, nút bấm, timer và vẽ biểu đồ.
- **PJSUA2/PJSIP** thực hiện SIP, SDP, RTP, codec và truy cập thiết bị âm thanh.
- **Asterisk** là SIP registrar và tổng đài định tuyến cuộc gọi.
- **SIP** điều khiển phiên gọi; **RTP** mang âm thanh; **RTCP** báo cáo chất lượng.

> Lưu ý: sơ đồ sequence trên giao diện là hình minh họa được tạo từ callback,
> không phải công cụ bắt từng packet. Wireshark mới phản ánh chính xác toàn bộ gói
> tin trên đường truyền.

## 2. Đọc dự án từ đâu?

| File | Vai trò |
| --- | --- |
| `main.cpp` | Tạo `QApplication`, tạo và hiển thị `MainWindow`, chạy event loop |
| `mainwindow.ui` | Bố cục giao diện được Qt Designer sinh ra |
| `mainwindow.h` | Khai báo `MainWindow`, `MyAccount`, `MyCall` và trạng thái chương trình |
| `mainwindow.cpp` | Khởi tạo PJSIP, xử lý nút bấm và các callback SIP/cuộc gọi |
| `sipsequencewidget.*` | Widget tự vẽ sequence diagram SIP bằng `QPainter` |
| `networkstatswidget.*` | Widget tự vẽ chỉ số và lịch sử RTP/RTCP |
| `CMakeLists.txt` | Cấu hình C++17, Qt, PJSIP, OpenSSL, ALSA và pthread |

Thứ tự đọc dễ hiểu nhất:

1. `main.cpp` để biết vòng đời ứng dụng.
2. `mainwindow.h` để thấy ba lớp chính và dữ liệu mà chúng giữ.
3. `MainWindow::initPjsip()` để hiểu quá trình đăng ký.
4. `on_btnCall_clicked()`, `on_btnHangup_clicked()` để hiểu thao tác người dùng.
5. Các callback `MyAccount` và `MyCall` để hiểu phản hồi bất đồng bộ.
6. Hai widget tự vẽ để hiểu phần trực quan hóa.

## 3. Kiến trúc các lớp

### `MainWindow`

`MainWindow` vừa là controller giao diện vừa giữ các đối tượng PJSUA2 chính:

- `pj::Endpoint ep`: vòng đời của thư viện PJSIP.
- `MyAccount *account`: tài khoản SIP `1001`.
- `MyCall *currentCall`: cuộc gọi hiện tại; chương trình chỉ quản lý một cuộc gọi.
- `QTimer networkStatsTimer`: đọc thống kê mạng mỗi giây.
- `logEntries`: lưu log gốc để có thể lọc SIP/RTP/ERROR và xuất file.
- `shuttingDown`: ngăn callback cập nhật UI trong lúc ứng dụng đang đóng.

### `MyAccount`

Lớp này kế thừa `pj::Account` và nhận sự kiện cấp tài khoản:

- `onRegState()`: Asterisk phản hồi việc đăng ký.
- `onIncomingCall()`: có INVITE gọi tới `1001`.

### `MyCall`

Lớp này kế thừa `pj::Call` và nhận sự kiện cấp cuộc gọi:

- `onCallState()`: cuộc gọi đổi trạng thái như gọi đi, xác nhận hoặc ngắt.
- `onCallMediaState()`: media audio đã sẵn sàng để nối mic và loa.
- `incoming`, `localHangup`, `wasConfirmed`: giúp vẽ đúng chiều và loại bản tin.
- `QPointer<MainWindow>`: tự trở thành null nếu cửa sổ đã bị hủy, giảm nguy cơ
  callback truy cập vùng nhớ không còn hợp lệ.

## 4. Luồng khởi động và REGISTER

Constructor của `MainWindow` thực hiện lần lượt:

1. `ui->setupUi(this)` tạo các widget từ file `.ui`.
2. Cấu hình timer cập nhật thống kê mỗi `1000 ms`.
3. Gọi `initPjsip()`.

Trong `initPjsip()`:

1. `localIpFromHostname()` chạy `hostname -I`, chọn IPv4 đầu tiên không phải
   loopback.
2. `ep.libCreate()` tạo endpoint.
3. `ep.libInit()` cấu hình thư viện và mức log.
4. `transportCreate(PJSIP_TRANSPORT_UDP, ...)` bind client vào UDP `5061`.
5. `ep.libStart()` chạy PJSIP.
6. Tạo `AccountConfig` với URI `sip:1001@<ip>`.
7. Cấu hình registrar `sip:<ip>:5060` và Digest credential.
8. `account->create(acfg)` bắt đầu REGISTER bất đồng bộ.

Luồng đăng ký SIP thường gặp:

```text
1001                         Asterisk
 |-------- REGISTER ----------->|
 |<------- 401 Unauthorized ----|  challenge, chưa phải lỗi mật khẩu
 |-- REGISTER + Authorization ->|
 |<---------- 200 OK -----------|
```

Trong Digest authentication, client không cần gửi mật khẩu dạng rõ trong bản tin
SIP. Client dùng username, realm, password, method, URI, nonce và các tham số liên
quan để tính response hash. Tuy nhiên code vẫn hard-code mật khẩu `123456`, chỉ phù
hợp phòng lab.

### Giả định địa chỉ mạng trong code hiện tại

Code gán:

```text
localIp = IPv4 lấy từ hostname -I
client  = localIp:5061
server  = localIp:5060
```

Điều này chỉ đúng khi Asterisk chạy trên cùng máy/IP với ứng dụng. Nếu Asterisk
chạy trên máy khác, cần có hai cấu hình riêng, ví dụ:

```text
Client:   192.168.1.20:5061
Asterisk: 192.168.1.10:5060
```

Không được lấy IP client để mặc nhiên thay cho IP server.

## 5. Luồng gọi đi

Khi bấm `Call 1002`, slot `on_btnCall_clicked()`:

1. Kiểm tra tài khoản tồn tại và chưa có cuộc gọi hoạt động.
2. Tạo `MyCall` gắn với tài khoản `1001`.
3. Tạo đích `sip:1002@<server>:5060`.
4. Gọi `makeCall()`. Từ đây PJSIP xử lý SIP/SDP bất đồng bộ.

Luồng thành công được đơn giản hóa:

```text
1001                     Asterisk                       1002
 |---- INVITE + SDP -------->|---- INVITE + SDP -------->|
 |<---- 100 Trying ----------|                            |
 |<---- 180 Ringing ---------|<---- 180 Ringing ---------|
 |<---- 200 OK + SDP --------|<---- 200 OK + SDP --------|
 |-------- ACK ------------->|-------- ACK ------------->|
 |<================ RTP audio hai chiều ================>|
```

- `INVITE` yêu cầu tạo cuộc gọi và thường mang SDP offer.
- `100 Trying` cho biết server đang xử lý.
- `180 Ringing` cho biết máy đích đang đổ chuông.
- `200 OK` chấp nhận cuộc gọi và thường mang SDP answer.
- `ACK` xác nhận phản hồi cuối của INVITE.
- SDP thương lượng IP, port media, codec; SDP không tự mang âm thanh.

Khi trạng thái thành `PJSIP_INV_STATE_CONFIRMED`, `onCallState()` bắt đầu timer
thống kê và ghi ACK lên sơ đồ.

## 6. Nhận cuộc gọi và kết thúc cuộc gọi

Khi nhận INVITE, `MyAccount::onIncomingCall()`:

1. Tạo `MyCall` bằng `callId` do PJSIP cung cấp.
2. Đưa cuộc gọi vào `MainWindow::currentCall`.
3. Đọc thông tin người gọi.
4. Trả lời ngay `200 OK`.

Vì trả lời tự động nên ứng dụng hiện không có trạng thái đổ chuông và nút Accept.
Nếu cửa sổ không còn tồn tại, callback trả `480 Temporarily Unavailable`.

Khi bấm Hangup, code gọi `hangup()` và đánh dấu `localHangup`. Với cuộc gọi đã
thiết lập, bản tin kết thúc thông thường là:

```text
Bên dập máy                     Bên còn lại
    |------------- BYE ------------>|
    |<------------ 200 OK -----------|
```

Nếu cuộc gọi chưa được trả lời thì SIP thường dùng `CANCEL`; phản hồi từ chối có
thể là `486 Busy Here`, `603 Decline`, v.v. Tham số `603 Decline` trong slot hangup
chủ yếu có ý nghĩa khi từ chối một INVITE chưa được xác nhận; PJSIP sẽ lựa chọn
hành động phù hợp với trạng thái dialog.

Khi trạng thái thành `DISCONNECTED`, `releaseCall()` dừng timer, đánh dấu widget
thống kê không hoạt động rồi xóa đối tượng cuộc gọi. Không xóa `MyCall` ngay sau
`hangup()` vì callback PJSIP vẫn có thể cần đối tượng cho đến khi kết thúc dialog.

## 7. RTP, RTCP và thiết bị âm thanh

Khi audio media chuyển sang `PJSUA_CALL_MEDIA_ACTIVE`, `onCallMediaState()` nối:

```text
microphone -> capture media -> callAudio -> RTP gửi đi
RTP nhận về -> callAudio -> playback media -> loa
```

`startTransmit()` chỉ thiết lập hướng truyền trong media graph của PJSUA2. Hai
lệnh là cần thiết vì âm thanh có hai chiều độc lập.

### SIP và RTP khác nhau thế nào?

| SIP | RTP |
| --- | --- |
| Báo hiệu và điều khiển phiên | Chở mẫu âm thanh/video thời gian thực |
| REGISTER, INVITE, ACK, BYE | Sequence number, timestamp, payload |
| Thường dùng UDP/TCP 5060, TLS 5061 | Thường dùng dải cổng UDP động |
| Không chứa tiếng nói trong mô hình thông thường | Chứa media đã mã hóa bằng codec |

Port `5061` trong chương trình chỉ là cổng UDP SIP cục bộ do code chọn. Theo quy
ước phổ biến, `5061` thường dành cho SIP over TLS, nhưng code này tạo
`PJSIP_TRANSPORT_UDP`, nên **không có TLS chỉ vì số cổng là 5061**.

RTCP đi kèm RTP, cung cấp báo cáo packet loss, jitter, RTT và số packet. RTCP không
chở âm thanh.

## 8. Các chỉ số mạng trên dashboard

`updateNetworkStats()` chạy mỗi giây, tìm audio stream đang active rồi đọc
`StreamStat` và `StreamInfo`.

- **Upload/download (kbps):** tốc độ bit trung bình giữa hai lần lấy mẫu.

  ```text
  kbps = (bytes_mới - bytes_cũ) * 8 / thời_gian_ms
  ```

  Vì `bit/ms` có cùng giá trị số với `kbit/s`, công thức cho ra kbps.

- **Packet loss (%):**

  ```text
  expected = received + lost
  loss %   = lost * 100 / expected
  ```

- **Jitter:** độ dao động của thời gian packet đến, không phải độ trễ tuyệt đối.
- **RTT:** thời gian khứ hồi từ một đầu đến đầu kia rồi quay lại.
- **Codec:** thuật toán mã hóa audio đã thương lượng, ví dụ PCMU, PCMA hoặc Opus.
- **TX/RX packets:** tổng packet gửi/nhận của stream.

Widget coi chất lượng tốt khi loss `< 1%`, jitter `< 30 ms`, RTT `< 150 ms`. Đây
là ngưỡng hiển thị do dự án chọn, không phải cam kết chất lượng tuyệt đối.
Biểu đồ giữ tối đa 60 mẫu, tương đương khoảng một phút khi timer chạy mỗi giây.

## 9. Kiến thức mạng cần nắm

### Địa chỉ IP và port

IP xác định host/interface trong mạng; port xác định tiến trình hoặc dịch vụ trên
host. Cặp `192.168.1.10:5060` đầy đủ hơn chỉ ghi IP. UDP phân biệt luồng bằng IP
nguồn, port nguồn, IP đích, port đích và protocol.

`127.0.0.1` là loopback, chỉ truy cập cùng máy. Địa chỉ `192.168.x.x`, `10.x.x.x`
và `172.16.x.x` đến `172.31.x.x` là địa chỉ private, thường cần NAT để ra Internet.

### UDP và TCP

UDP không thiết lập kết nối, overhead thấp và không tự truyền lại packet. TCP đảm
bảo thứ tự và truyền lại nhưng có thể tăng độ trễ khi mất gói. SIP có thể chạy trên
UDP/TCP/TLS; RTP thoại thường chạy trên UDP vì packet đến muộn cũng ít hữu ích.

### NAT và firewall

SIP/SDP có thể chứa IP/port bên trong payload, nên chỉ NAT header IP là chưa đủ.
Qua Internet thường cần cấu hình Asterisk `external_media_address`,
`external_signaling_address`, `local_net`, mở dải RTP, hoặc dùng STUN/TURN/ICE tùy
mô hình. SIP ALG trên router đôi khi sửa packet sai và gây lỗi một chiều tiếng.

### Codec và băng thông

Codec đổi audio thành payload số. Băng thông thực tế lớn hơn bitrate codec vì mỗi
packet còn có header Ethernet/IP/UDP/RTP. Packetization interval ngắn làm nhiều
packet hơn, giảm độ trễ nhưng tăng overhead.

### Vì sao có tiếng một chiều?

Các nguyên nhân phổ biến: SDP quảng bá sai IP, NAT/firewall chặn một chiều, mở sai
dải RTP, Asterisk cấu hình external address sai hoặc chọn sai thiết bị audio. SIP
đăng ký thành công không chứng minh RTP chắc chắn đi được.

## 10. Qt, callback và an toàn luồng

Qt yêu cầu widget chỉ được cập nhật từ GUI thread. Callback của PJSIP có thể chạy
ở worker thread. Vì thế các hàm như `appendLog()` kiểm tra:

```cpp
if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(..., Qt::QueuedConnection);
    return;
}
```

`QueuedConnection` đưa công việc vào event queue của GUI thread. Nếu sửa UI trực
tiếp trong callback, chương trình có thể cảnh báo, race condition hoặc crash không
ổn định.

Qt tự nối slot có tên `on_<objectName>_<signal>()`. Ví dụ
`on_btnCall_clicked()` tự nối với signal `clicked()` của `btnCall` sau
`setupUi()`.

## 11. Hai widget tự vẽ

### `SipSequenceWidget`

- Lưu tối đa 200 message.
- Ánh xạ `1001` ở trái, `Asterisk` giữa, `1002` ở phải.
- Dùng màu theo loại Request/Response/Challenge/Media/Error.
- Dùng `QPainter` vẽ lifeline, mũi tên, timestamp và label.
- RTP được vẽ nét đứt và hai đầu mũi tên để biểu diễn media hai chiều.

### `NetworkStatsWidget`

- Nhận một `NetworkStatsSample` từ controller.
- Vẽ năm metric card và hai đường upload/download.
- Gọi `update()` để yêu cầu Qt phát sinh `paintEvent()` ở vòng event kế tiếp.
- Tự co từ năm cột thành ba cột khi chiều rộng nhỏ hơn 780 px.

## 12. Build và kiểm thử

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --clean-first
./build/SIP
```

Kịch bản kiểm thử thủ công:

1. Khởi động Asterisk và đăng ký `1002` bằng Linphone.
2. Mở ứng dụng, kiểm tra REGISTER rồi ONLINE.
3. Gọi `1002`, kiểm tra INVITE, ringing, 200 OK, ACK.
4. Nói hai chiều, kiểm tra RTP và các chỉ số thay đổi.
5. Dập máy từ mỗi phía, kiểm tra BYE/200 OK.
6. Thử sai mật khẩu, tắt Asterisk và chặn port để quan sát lỗi.
7. Dùng Wireshark filter `sip || rtp || rtcp` để đối chiếu.

Các lệnh Asterisk hữu ích:

```bash
sudo asterisk -rvvv
pjsip show endpoints
pjsip show contacts
pjsip set logger on
rtp set debug on
```

## 13. Hạn chế và hướng phát triển

- IP Asterisk đang bị đồng nhất với IP local; nên đưa server IP ra cấu hình.
- Extension, realm và password đang hard-code; nên dùng form/config và không lưu
  mật khẩu thật trong Git.
- Chỉ hỗ trợ một `currentCall`.
- Cuộc gọi đến được tự động trả lời, chưa có Accept/Reject và ringtone.
- Sequence diagram là diễn giải callback, chưa phải packet capture đầy đủ.
- Chưa có TLS/SRTP; SIP và audio phòng lab không được mã hóa.
- Chưa có test tự động; phần PJSIP phụ thuộc server và thiết bị audio thật.
- `hostname -I` phụ thuộc Linux và có thể chọn nhầm interface khi máy có Ethernet,
  Wi-Fi, VPN hoặc Docker cùng lúc.

## 14. Câu hỏi có thể bị hỏi và câu trả lời gợi ý

### Nhóm tổng quan và code

**1. Đề tài giải quyết bài toán gì?**  
Xây dựng SIP softphone phục vụ học tập: đăng ký với Asterisk, gọi/nhận cuộc gọi,
truyền audio RTP và trực quan hóa báo hiệu cùng chất lượng mạng.

**2. Vì sao dùng Qt và PJSUA2?**  
Qt phù hợp ứng dụng desktop đa nền tảng; PJSUA2 là API C++ mức cao của PJSIP,
đã cung cấp SIP dialog, SDP, RTP, codec và audio device.

**3. Ba lớp chính là gì?**  
`MainWindow` điều phối UI và vòng đời; `MyAccount` nhận callback đăng ký/cuộc gọi
đến; `MyCall` nhận callback trạng thái call và media.

**4. Tại sao callback dùng `override`?**  
Để compiler kiểm tra hàm thật sự ghi đè virtual method đúng chữ ký, tránh lỗi viết
sai tên hoặc sai tham số.

**5. Tại sao dùng `QPointer<MainWindow>`?**  
Nó tự thành null khi QObject bị hủy, giúp callback không truy cập cửa sổ đã chết.

**6. Vì sao không delete cuộc gọi ngay khi bấm Hangup?**  
Hangup là bất đồng bộ; PJSIP còn xử lý transaction và callback. Đối tượng chỉ được
giải phóng khi trạng thái `DISCONNECTED`.

**7. Vì sao phải dùng `QueuedConnection`?**  
Callback PJSIP có thể không chạy trên GUI thread, trong khi Qt widget chỉ an toàn
khi cập nhật ở GUI thread.

**8. Signal và slot là gì?**  
Signal thông báo một sự kiện; slot là hàm nhận sự kiện. Cơ chế này giúp các QObject
giao tiếp ít phụ thuộc và có thể chuyển công việc giữa thread.

### Nhóm SIP

**9. SIP là gì?**  
SIP là giao thức báo hiệu dùng để tạo, thay đổi và kết thúc phiên truyền thông; nó
không phải giao thức chở audio chính.

**10. REGISTER có tác dụng gì?**  
Nó báo cho registrar biết extension hiện có thể nhận SIP request tại contact nào.

**11. 401 Unauthorized có luôn là lỗi không?**  
Không. Trong Digest flow, server thường gửi 401 kèm challenge để client tính
Authorization rồi REGISTER lại. Chỉ thất bại nếu xác thực tiếp tục không thành công.

**12. INVITE, ACK và BYE dùng làm gì?**  
INVITE tạo/đàm phán phiên, ACK xác nhận phản hồi cuối thành công của INVITE, BYE
kết thúc dialog đã thiết lập.

**13. CANCEL khác BYE thế nào?**  
CANCEL hủy INVITE đang chờ trước khi cuộc gọi được thiết lập; BYE kết thúc dialog
đã được thiết lập.

**14. SDP là gì?**  
SDP mô tả media: IP, port, codec, hướng truyền. Nó được mang trong SIP body nhưng
không phải SIP method và không chứa audio.

**15. Asterisk đóng vai trò gì?**  
Trong mô hình này, nó là registrar và SIP server/B2BUA định tuyến cuộc gọi giữa
`1001` và `1002`, có thể neo hoặc điều khiển media tùy cấu hình.

**16. Extension có phải số điện thoại thật không?**  
Không nhất thiết. Nó là định danh nội bộ trong dial plan/tài khoản SIP; tổng đài có
thể ánh xạ nó sang số điện thoại hoặc route khác.

### Nhóm RTP và mạng

**17. Vì sao RTP thường dùng UDP?**  
Thoại ưu tiên thời gian thực; truyền lại packet cũ thường làm tăng trễ và không còn
hữu ích. Ứng dụng chấp nhận một ít mất gói để giữ độ trễ thấp.

**18. RTCP có tác dụng gì?**  
Trao đổi báo cáo chất lượng và timing như packet loss, jitter và RTT; không mang
nội dung audio.

**19. Jitter khác latency thế nào?**  
Latency là thời gian packet đi từ nguồn tới đích; jitter là độ biến thiên của thời
gian đó giữa các packet.

**20. Packet loss ảnh hưởng ra sao?**  
Mất ít có thể được codec hoặc packet-loss concealment che giấu; mất nhiều gây rè,
đứt tiếng và giảm khả năng hiểu lời nói.

**21. RTT 150 ms có phải one-way delay 150 ms không?**  
Không. RTT là khứ hồi. One-way delay cần đồng hồ hai đầu được đồng bộ để đo chính
xác và không nhất thiết bằng đúng RTT/2 nếu route bất đối xứng.

**22. Vì sao bitrate đo được lớn hơn bitrate codec?**  
Do overhead của RTP, UDP, IP, lớp liên kết và có thể cả RTCP; packet càng nhỏ thì
tỷ lệ overhead càng lớn.

**23. SIP chạy port nào, RTP chạy port nào?**  
SIP thường 5060 cho UDP/TCP và 5061 cho TLS theo quy ước. RTP dùng port động được
thương lượng qua SDP. Riêng code này dùng UDP SIP local port 5061, không phải TLS.

**24. Đăng ký ONLINE nhưng không có tiếng, tại sao?**  
REGISTER chỉ chứng minh signaling tới registrar hoạt động. RTP có thể bị NAT,
firewall, SDP sai IP/port, cấu hình Asterisk hoặc audio device chặn.

**25. NAT gây khó gì cho SIP?**  
Ngoài header mạng, SIP/SDP còn chứa địa chỉ bên trong payload; địa chỉ private đó
có thể không truy cập được từ Internet nếu không được xử lý đúng.

**26. Làm sao kiểm tra packet thật?**  
Dùng Wireshark với `sip || rtp || rtcp`, hoặc bật `pjsip set logger on` và
`rtp set debug on` trong Asterisk CLI.

### Nhóm phản biện thiết kế

**27. Tại sao lấy `hostname -I` có thể sai?**  
Máy có thể có nhiều interface; IPv4 đầu tiên có thể thuộc Docker, VPN hoặc mạng
không đi tới Asterisk. Cấu hình interface/server rõ ràng đáng tin cậy hơn.

**28. Tại sao cổng 5061 ở đây không có nghĩa là bảo mật?**  
Bảo mật do transport TLS và certificate quyết định. Code tạo UDP transport; số
cổng tự nó không mã hóa dữ liệu.

**29. Ứng dụng hiện có bảo mật thoại không?**  
Không. Code chưa cấu hình SIP TLS hoặc SRTP; credential Digest không đồng nghĩa
toàn bộ signaling và audio được mã hóa.

**30. Sơ đồ trên UI có thay Wireshark được không?**  
Không. Nó chỉ thêm các event mà code chủ động mô hình hóa, có thể bỏ qua retransmit,
header, transaction chi tiết hoặc packet không tạo callback tương ứng.

**31. Nếu muốn hỗ trợ nhiều cuộc gọi thì sửa gì?**  
Thay `currentCall` bằng collection quản lý theo call ID, tách UI từng call, xác định
call active/held và quản lý ownership cùng callback đồng thời.

**32. Nếu muốn triển khai thực tế thì ưu tiên sửa gì?**  
Tách cấu hình server/local interface, bỏ credential hard-code, thêm TLS/SRTP, xử lý
NAT/ICE, Accept/Reject, retry/error rõ ràng, logging có redaction và test.

## 15. Mẫu trình bày ngắn khi demo

“Ứng dụng của em là một SIP User Agent viết bằng Qt C++17 và PJSUA2. Khi khởi
động, client bind UDP port 5061, đăng ký extension 1001 với Asterisk port 5060 bằng
Digest authentication. Khi gọi 1002, SIP thực hiện INVITE/response/ACK và SDP
thương lượng codec cùng địa chỉ RTP. Khi media active, chương trình nối microphone
và loa với audio media của PJSIP. Timer mỗi giây đọc thống kê RTP/RTCP để hiển thị
bitrate, packet loss, jitter và RTT. Callback PJSIP được chuyển về GUI thread bằng
QueuedConnection để cập nhật Qt an toàn. Phiên bản hiện tại phục vụ lab, còn hạn
chế ở cấu hình IP hard-code theo máy local, một cuộc gọi, chưa có TLS/SRTP và chưa
có kiểm thử tự động.”

