#pragma once
// nano_trigger.h
// Drop this into your classifier project.
// Requires: termios (Linux, no extra libs needed)

#include <string>

class NanoTrigger {
public:
    // port  : e.g. "/dev/ttyUSB0" or "/dev/ttyACM0"
    // threshold : correlation value above which D2 is pulsed
    NanoTrigger(const std::string& port, double threshold, int baud = 9600);
    ~NanoTrigger();

    // Call this every frame with the current correlation value.
    // Sends 'T' to the Nano when correlation exceeds the threshold
    // and the output isn't already mid-pulse.
    void update(double correlation);
    void manualTrigger();
    // Change the threshold at runtime (e.g. from a UI slider).
    void setThreshold(double threshold);
    double getThreshold() const;

    // Sends 'S' and checks for "OK" — useful for startup handshake.
    bool ping();

    bool isOpen() const;

private:
    int         fd_;           // serial file descriptor
    double      threshold_;
    bool        triggered_;    // debounce: ignore until above→below crossing

    bool openPort(const std::string& port, int baud);
    void sendCommand(const std::string& cmd);
};
