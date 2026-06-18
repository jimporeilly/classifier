// nano_trigger.cpp
#include "nano_trigger.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>

// ── helpers ──────────────────────────────────────────────────────────────────

static speed_t baudToSpeed(int baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B9600;
    }
}

// ── NanoTrigger ───────────────────────────────────────────────────────────────

NanoTrigger::NanoTrigger(const std::string& port, double threshold, int baud)
    : fd_(-1), threshold_(threshold), triggered_(false),
      last_trigger_time_(std::chrono::steady_clock::now() - std::chrono::seconds(COOLDOWN_SECONDS))
{
    if (!openPort(port, baud)) {
        std::cerr << "[NanoTrigger] Failed to open " << port << "\n";
        return;
    }
    // The Nano resets on USB connect; give it time to boot.
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << "[NanoTrigger] Connected on " << port << "\n";
}

NanoTrigger::~NanoTrigger() {
    if (fd_ >= 0) close(fd_);
}

bool NanoTrigger::openPort(const std::string& port, int baud) {
    fd_ = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_ < 0) return false;

    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) return false;

    speed_t speed = baudToSpeed(baud);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
    tty.c_cflag |= (CLOCAL | CREAD);              // ignore modem, enable read
    tty.c_cflag &= ~(PARENB | PARODD);            // no parity
    tty.c_cflag &= ~CSTOPB;                        // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                       // no hardware flow control

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);       // no software flow control
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_oflag = 0;
    tty.c_lflag = 0;

    tty.c_cc[VMIN]  = 0;   // non-blocking read
    tty.c_cc[VTIME] = 5;   // 0.5 s read timeout

    tcsetattr(fd_, TCSANOW, &tty);
    tcflush(fd_, TCIFLUSH);
    return true;
}

bool NanoTrigger::manualTrigger() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_trigger_time_).count();
    if (elapsed < COOLDOWN_SECONDS) {
        std::cout << "[NanoTrigger] Cooldown active (" << (COOLDOWN_SECONDS - elapsed) << "s remaining)\n";
        return false;
    }
    last_trigger_time_ = now;
    sendCommand("TRIGGER");
    std::cout << "[NanoTrigger] Trigger → D2 pulse\n";
    return true;
}

void NanoTrigger::sendCommand(const std::string& cmd) {
    if (fd_ < 0) return;
    std::string msg = cmd + "\n";
    write(fd_, msg.c_str(), msg.size());
}

void NanoTrigger::update(double correlation) {
    if (fd_ < 0) return;

    if (correlation > threshold_) {
        if (!triggered_) {           // only fire on rising edge
            triggered_ = true;
            sendCommand("TRIGGER");
            std::cout << "[NanoTrigger] Movement detected (corr="
                      << correlation << ") → D2 pulse\n";
        }
    } else {
        triggered_ = false;          // reset; ready for next event
    }
}

void NanoTrigger::setThreshold(double threshold) {
    threshold_ = threshold;
    std::cout << "[NanoTrigger] Threshold set to " << threshold_ << "\n";
}

double NanoTrigger::getThreshold() const { return threshold_; }

bool NanoTrigger::ping() {
    if (fd_ < 0) return false;
    sendCommand("XXXX");
    char buf[16]{};
    int n = read(fd_, buf, sizeof(buf) - 1);
    if (n > 0) {
        std::string reply(buf, n);
        return reply.find("OK") != std::string::npos;
    }
    return false;
}

bool NanoTrigger::isOpen() const { return fd_ >= 0; }
