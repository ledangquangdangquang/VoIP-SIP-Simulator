# Repository Guidelines

## Project Structure & Module Organization

This repository contains a small Qt Widgets SIP client written in C++17. `main.cpp` starts the application; `mainwindow.h` and `mainwindow.cpp` contain the UI controller and PJSUA2 account/call callbacks. Edit the form in `mainwindow.ui`, preferably with Qt Designer. Build configuration lives in `CMakeLists.txt`, and `Demo/sip.gif` illustrates the application in `README.md`.

Treat `build-SIP-Desktop-Debug/` as generated output; do not hand-edit it. New code may remain at the root while the project is small; if it grows, group classes under `src/` and mirror them under `tests/`.

## Build, Test, and Development Commands

The project requires Qt 5 or 6 with Widgets and Network, plus PJSIP/PJSUA2 installed under `/usr/local`.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/SIP
```

These commands configure, compile, and launch an out-of-tree debug build. Use `cmake --build build --clean-first` when generated Qt files become stale. Qt Creator users can open `CMakeLists.txt` directly.

## Coding Style & Naming Conventions

Use four spaces, C++17, and the brace style already present. Classes use PascalCase (`MainWindow`, `MyCall`); functions and local variables use camelCase (`initPjsip`, `currentCall`). Qt auto-connected slots follow `on_<object>_<signal>()`, such as `on_btnCall_clicked()`. Keep PJSIP callbacks declared with `override`, prefer scoped ownership over new raw pointers, and use `qDebug()` for diagnostic output. No formatter or linter is configured, so keep changes consistent with adjacent code.

## Testing Guidelines

There is currently no automated test suite or coverage requirement. Before submitting, build from a clean directory and manually verify registration, outgoing call, hangup, and incoming-call logging against an Asterisk server. If adding tests, integrate them with CTest, place them in `tests/`, and name files `<component>_test.cpp`; run them with `ctest --test-dir build --output-on-failure`.

## Commit & Pull Request Guidelines

History uses short, task-focused subjects (for example, `install Wireshark` and `demo:`). Prefer a concise imperative subject that identifies the change, such as `Handle incoming SIP calls`. Keep each commit focused. Pull requests should explain behavior changes, list manual test steps and environment details, link relevant issues, and include a screenshot or GIF for UI changes.

## Security & Configuration

SIP server addresses and demo credentials currently appear in source and documentation. Do not commit real passwords, private server addresses, packet captures, or local Qt files such as `CMakeLists.txt.user`. Keep machine-specific settings outside version control and redact SIP logs before sharing them.
