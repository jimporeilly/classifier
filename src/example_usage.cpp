// example_usage.cpp  — shows how to wire NanoTrigger into your classifier

#include "nano_trigger.h"

int main() {
    // --- Setup ---
    // Adjust port as needed: /dev/ttyUSB0 or /dev/ttyACM0
    // Find yours with:  ls /dev/tty{USB,ACM}*
    NanoTrigger trigger("/dev/ttyACM0", /*threshold=*/0.75);

    if (!trigger.isOpen()) return 1;
    if (!trigger.ping()) {
        // Not fatal, but worth logging; Nano may still be booting
        printf("[main] Nano ping failed — continuing anyway\n");
    }

    // --- Runtime threshold control example ---
    // In your real code, drive this from a UI slider / config file / CLI arg.
    // trigger.setThreshold(0.85);

    // --- Classifier loop ---
    while (true) {
        // ... your existing frame capture and processing ...

        double correlation = calculateCorrelation(previous_frame_, current_frame);

        // Drop this single line into your loop — that's it.
        trigger.update(correlation);

        // ... rest of classifier logic ...
    }

    return 0;
}
