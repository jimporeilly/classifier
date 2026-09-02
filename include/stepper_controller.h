#pragma once
// stepper_controller.h
// Serial link from the classifier app to the Arduino Mega running
// arduino/mega_stepper_controller/mega_stepper_controller.ino, which drives
// 6 L298N-driven hip actuators (timed F/B drive per leg) and 6 L298N-driven
// leg-extend actuators (extend/retract/stop), plus INA219 battery monitoring.
// Hips were originally TMC2209 steppers; rewired to linear actuators, but
// the serial protocol (F/B/FA/BA) is unchanged.
//
// Protocol (9600 baud, newline-terminated ASCII):
//   ON / OFF                 arm / disarm hip + actuator moves
//   F<leg> / B<leg>          drive hip <0-5> forward / back
//   FA / BA                  move all legs forward / back
//   H<leg>                   timed half-travel ("mid") move for hip <0-5>, auto-
//                            stops on the Mega itself (~1s). Direction is inferred
//                            from the last F/B sent for that leg - Mega refuses
//                            with ERR if that leg's position is unknown.
//   M<act>                   timed half-travel ("mid") move for actuator <0-5>
//                            (~1.25s). Same inferred-direction caveat as H<leg>,
//                            based on the last E/R sent for that actuator.
//   E<act> / R<act> / S<act> extend / retract / stop actuator <0-5>
//   EA / RA / SA             extend / retract / stop all actuators
//   BATT?                    request battery voltage on demand
//   ?                        print full status of all legs/actuators/battery
//
// Unsolicited from the Mega (~every 2s): "BATT:<volts>", "BATT_WARN",
// "SAFETY: ...". A background thread reads these continuously (all other
// replies are line-buffered too, so one reader handles everything) and
// mirrors battery_voltage/battery_warning into AppState if given one.

#include <string>
#include <thread>
#include <atomic>
#include <memory>

struct AppState;

class StepperController {
public:
    static constexpr int kNumLegs = 6;
    static constexpr int kNumActuators = 6;

    // port  : e.g. "/dev/ttyACM0" (the Mega).
    // state : if non-null, receives battery_voltage/battery_warning updates
    //         parsed from the Mega's unsolicited BATT:/BATT_WARN lines.
    explicit StepperController(const std::string& port,
                                std::shared_ptr<AppState> state = nullptr,
                                int baud = 9600);
    ~StepperController();

    bool isOpen() const;

    // Sends '?' and relies on the background reader to log the reply -
    // useful as a startup handshake (see AppState-less console log path).
    void requestStatus();
    void requestBattery();

    void setMotorsEnabled(bool enabled); // ON / OFF

    void moveLegForward(int leg);        // F<leg>
    void moveLegBack(int leg);           // B<leg>
    void moveAllForward();               // FA
    void moveAllBack();                  // BA

    // Timed half-travel ("mid") move. Direction is inferred by the Mega from
    // the last F/B (or E/R) sent for that index - see protocol note above.
    void moveLegToMid(int leg);          // H<leg>
    void moveActuatorToMid(int act);     // M<act>

    void extendActuator(int act);        // E<act>
    void retractActuator(int act);       // R<act>
    void stopActuator(int act);          // S<act>
    void extendAllActuators();           // EA
    void retractAllActuators();          // RA
    void stopAllActuators();             // SA

private:
    int fd_;
    std::shared_ptr<AppState> state_;
    std::thread read_thread_;
    std::atomic<bool> running_{false};

    bool openPort(const std::string& port, int baud);
    void sendCommand(const std::string& cmd);
    void readLoop();
    void handleLine(const std::string& line);
    static bool isValidIndex(int idx);
};
