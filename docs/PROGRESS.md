# Project Progress

Last updated: 2026-06-20

## Current Status

The project builds successfully as a Qt 6/C++17 desktop application with PJSUA2. Core SIP calling, debugging UI, packet visualization, and RTCP statistics are implemented. End-to-end call behavior still requires manual verification with Asterisk and extension `1002`.

## Completed

- [x] Register extension `1001` with Asterisk using Digest authentication.
- [x] Start outgoing calls to `1002` and send hangup requests.
- [x] Receive and automatically answer incoming calls.
- [x] Connect active RTP audio to the microphone and playback device.
- [x] Move PJSIP callback UI updates onto the Qt GUI thread.
- [x] Manage call cleanup without dangling `currentCall` pointers.
- [x] Add a responsive two-column debugging dashboard.
- [x] Add categorized SIP, RTP, and error logs with filtering and export.
- [x] Add a SIP sequence diagram for REGISTER, INVITE, responses, ACK, RTP, and BYE.
- [x] Add live RTCP metrics: bitrate, packets, loss, jitter, RTT, codec, and duration.

## Verification

The current source passes:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The last clean build completed successfully. The remaining compiler warning originates from `pj/compat/limits.h` in the local PJSIP installation.

## Known Limitations

- SIP addresses, extensions, and demo credentials are hard-coded.
- There is no automated test suite.
- Incoming calls are auto-answered; accept/reject controls are not yet available.
- RTCP metrics and audio quality need comparison with a Wireshark capture.
- Qt Creator must use a build directory under the current repository, not the older `/home/quang/Projects/SIP` path.

Update this file whenever a feature is implemented or a limitation is resolved. Mark runtime-sensitive work complete only after testing it against Asterisk.
