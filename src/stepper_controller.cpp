// stepper_controller.cpp
#include "stepper_controller.h"
#include "shared_data.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <chrono>

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

// ── StepperController ───────────────────────────────────────────────────────

StepperController::StepperController(const std::string& port,
                                      std::shared_ptr<AppState> state,
                                      int baud)
    : fd_(-1), state_(std::move(state))
{
    if (!openPort(port, baud)) {
        std::cerr << "[StepperController] Failed to open " << port << "\n";
        return;
    }
    // The Mega resets on USB connect; give it time to boot.
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << "[StepperController] Connected on " << port << "\n";

    running_ = true;
    read_thread_ = std::thread(&StepperController::readLoop, this);
}

StepperController::~StepperController() {
    running_ = false;
    if (read_thread_.joinable()) read_thread_.join();
    if (fd_ >= 0) close(fd_);
}

bool StepperController::openPort(const std::string& port, int baud) {
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

    tty.c_cc[VMIN]  = 0;   // non-blocking-ish read
    tty.c_cc[VTIME] = 2;   // 0.2 s read timeout per call

    tcsetattr(fd_, TCSANOW, &tty);
    tcflush(fd_, TCIFLUSH);
    return true;
}

void StepperController::sendCommand(const std::string& cmd) {
    if (fd_ < 0) return;
    std::string msg = cmd + "\n";
    write(fd_, msg.c_str(), msg.size());
}

bool StepperController::isValidIndex(int idx) {
    return idx >= 0 && idx < kNumLegs; // kNumLegs == kNumActuators == 6
}

bool StepperController::isOpen() const { return fd_ >= 0; }

void StepperController::requestStatus() { sendCommand("?"); }
void StepperController::requestBattery() { sendCommand("BATT?"); }

void StepperController::setMotorsEnabled(bool enabled) {
    sendCommand(enabled ? "ON" : "OFF");
    if (state_) state_->steppers_enabled = enabled;
}

void StepperController::moveLegForward(int leg) {
    if (!isValidIndex(leg)) return;
    sendCommand("F" + std::to_string(leg));
}

void StepperController::moveLegBack(int leg) {
    if (!isValidIndex(leg)) return;
    sendCommand("B" + std::to_string(leg));
}

void StepperController::moveAllForward() { sendCommand("FA"); }
void StepperController::moveAllBack() { sendCommand("BA"); }

void StepperController::moveLegToMid(int leg) {
    if (!isValidIndex(leg)) return;
    sendCommand("H" + std::to_string(leg));
}

void StepperController::moveActuatorToMid(int act) {
    if (!isValidIndex(act)) return;
    sendCommand("M" + std::to_string(act));
}

void StepperController::extendActuator(int act) {
    if (!isValidIndex(act)) return;
    sendCommand("E" + std::to_string(act));
}

void StepperController::retractActuator(int act) {
    if (!isValidIndex(act)) return;
    sendCommand("R" + std::to_string(act));
}

void StepperController::stopActuator(int act) {
    if (!isValidIndex(act)) return;
    sendCommand("S" + std::to_string(act));
}

void StepperController::extendAllActuators() { sendCommand("EA"); }
void StepperController::retractAllActuators() { sendCommand("RA"); }
void StepperController::stopAllActuators() { sendCommand("SA"); }

void StepperController::handleLine(const std::string& line) {
    if (line.rfind("BATT:", 0) == 0) {
        float volts = 0.0f;
        try {
            volts = std::stof(line.substr(5));
        } catch (const std::exception&) {
            std::cout << "[Mega] " << line << "\n";
            return;
        }
        if (state_) {
            state_->battery_voltage = volts;
            // Cleared each report cycle; a following BATT_WARN line (same
            // ~2s tick on the Mega side) re-sets it if still in range.
            state_->battery_warning = false;
        }
        std::cout << "[Mega] " << line << "\n";
        return;
    }

    if (line.rfind("BATT_WARN", 0) == 0) {
        if (state_) state_->battery_warning = true;
        std::cout << "[Mega] " << line << "\n";
        return;
    }

    // SAFETY: ... and everything else (OK/ERR/status dumps) - just log it.
    std::cout << "[Mega] " << line << "\n";
}

void StepperController::readLoop() {
    std::string buf;
    char chunk[128];

    while (running_) {
        if (fd_ < 0) break;
        int n = read(fd_, chunk, sizeof(chunk));
        if (n > 0) {
            buf.append(chunk, n);
            size_t nl;
            while ((nl = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, nl);
                buf.erase(0, nl + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) handleLine(line);
            }
        }
    }
}
