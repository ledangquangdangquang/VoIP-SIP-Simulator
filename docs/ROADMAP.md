# Project Roadmap

## Now — Stabilize the Call Flow

- [ ] Test outgoing call, answer, two-way audio, and hangup between `1001` and `1002`.
- [ ] Test incoming calls and replace auto-answer with Accept and Reject actions.
- [ ] Compare displayed packet loss, jitter, RTT, and bitrate against Wireshark.
- [ ] Investigate any remaining SIP `481 Call/Transaction Does Not Exist` responses.
- [ ] Move server address and credentials out of `mainwindow.cpp`.

## Next — Improve the Networking Lab

- [ ] Add a settings dialog for account, server, transport, and RTP options.
- [ ] Add a packet inspector showing Via, From, To, Call-ID, CSeq, SDP, IP, and port data.
- [ ] Export network statistics and sequence events to CSV or JSON.
- [ ] Show RTCP discard, duplicate, and out-of-order packet counters.
- [ ] Add CTest coverage for formatting, filtering, and statistics calculations.

## Later — Network Experiments

- [ ] Add controlled delay, jitter, loss, and bandwidth limits using Linux `tc netem`.
- [ ] Visualize private/public addresses and NAT traversal behavior.
- [ ] Compare UDP, TCP, and TLS SIP transports.
- [ ] Save and replay call scenarios for classroom demonstrations.

## Maintenance Rule

Move finished items to `PROGRESS.md` with the verification performed. Keep roadmap entries small enough to implement and test in one focused change.
