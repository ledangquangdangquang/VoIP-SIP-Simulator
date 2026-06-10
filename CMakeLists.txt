cmake_minimum_required(VERSION 3.16)

project(SIP VERSION 0.1 LANGUAGES CXX)

# Tự động xử lý các file giao diện và meta-object của Qt
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Tìm các thành phần Qt (Đã thêm Network để xử lý Socket cho VoIP)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

# 2. Khai báo danh sách các file mã nguồn của dự án
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

# 3. Tạo Target thực thi (Executable) dựa trên phiên bản Qt bạn đang dùng
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(SIP
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(SIP SHARED ${PROJECT_SOURCES})
    else()
        add_executable(SIP ${PROJECT_SOURCES})
    endif()
endif()

# 4. KẾT NỐI TỚI THƯ VIỆN PJSIP TỰ COMPILE BẰNG TAY (Đã đồng bộ sang target "SIP")
# Trỏ trực tiếp tới thư mục /usr/local (Nơi lệnh 'sudo make install' của PJSIP cài vào)
target_include_directories(SIP PRIVATE /usr/local/include)
target_link_directories(SIP PRIVATE /usr/local/lib)
# Liên kết toàn bộ module mạng của Qt và lõi VoIP PJSIP vào target "SIP"
# Liên kết toàn bộ module mạng của Qt và lõi VoIP PJSIP vào target "SIP"
target_link_libraries(SIP PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    pjsua2
    pjsua
    pjsip-ua
    pjsip-simple
    pjsip            # Module SDP hiện tại đã nằm trọn vẹn trong này
    pjmedia-codec
    pjmedia
    pjmedia-videodev
    pjmedia-audiodev
    pjlib-util
    pj
    crypto
    ssl
    pthread
    asound
)

# 5. Các thuộc tính bổ sung cho hệ thống
set_target_properties(SIP PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Cấu hình cài đặt đầu ra khi đóng gói sản phẩm
install(TARGETS SIP
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Hoàn tất khởi tạo nếu sử dụng Qt6
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(SIP)
endif()
