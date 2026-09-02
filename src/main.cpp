#include "shared_data.h"
#include "camera_thread.h"
#include "classifier_thread.h"
#include "correlation_thread.h"
#include "gui_handler.h"
#include "stepper_controller.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  --camera <id>        Left CSI sensor-id for the stereo pair (default: 0, right = id+1)\n"
              << "  --model <path>       Path to TorchScript model file\n"
              << "  --help               Show this help message\n"
              << "\nKeyboard shortcuts:\n"
              << "  ESC - Exit application\n"
              << "  P   - Pause/Resume processing\n"
              << "  C   - Capture current frame\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Multi-threaded Camera Classifier ===" << std::endl;

    // Parse command line arguments
    int camera_id = 0;
    std::string model_path;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--camera" && i + 1 < argc) {
            camera_id = std::atoi(argv[++i]);
        } else if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        }
    }

    int right_camera_id = camera_id + 1;
    std::cout << "Stereo CSI sensors: left=" << camera_id << " right=" << right_camera_id << std::endl;
    if (!model_path.empty()) {
        std::cout << "Model path: " << model_path << std::endl;
    } else {
        std::cout << "No model specified. Running without classification." << std::endl;
        std::cout << "Use --model <path> to load a TorchScript model." << std::endl;
    }

    // Create shared state
    auto state = std::make_shared<AppState>();
    state->camera_id = camera_id;
    state->status_message = "Initializing...";

    StepperController steppers("/dev/ttyACM0", state);  // Mega running mega_stepper_controller.ino
    if (!steppers.isOpen()) {
        printf("[main] Failed to open Mega serial port — continuing anyway\n");
    } else {
        steppers.requestStatus();  // '?' handshake; reply logged via [Mega] console lines

        // Startup homing sequence. No position sensors exist on the Mega
        // side (see mega_stepper_controller.ino), so each step blocks for
        // the full/half travel time before the next command, matching the
        // Mega's MAX_ACTUATOR_ON_MS/HALF_ACTUATOR_ON_MS - this is what lets
        // actuatorLastDir be trusted when M2/M3 infer their mid-move
        // direction below. Target layout: L0/L1 extended, L2/L3 mid,
        // L4/L5 retracted.
        std::cout << "[main] Running actuator homing sequence..." << std::endl;
        steppers.setMotorsEnabled(true);
        steppers.retractAllActuators();
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        steppers.extendActuator(0);
        steppers.extendActuator(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        steppers.moveActuatorToMid(2);
        steppers.moveActuatorToMid(3);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        std::cout << "[main] Homing sequence complete." << std::endl;
    }

    // Create thread objects
    CameraThread camera_thread(state, camera_id, right_camera_id);
    ClassifierThread classifier_thread(state);
    CorrelationThread correlation_thread(state, nullptr);  // Nano retired; no motion-trigger board wired up

    GUIHandler gui(state, &steppers);

    // Load model if specified
    if (!model_path.empty()) {
        if (!classifier_thread.loadModel(model_path)) {
            std::cerr << "Failed to load model. Continuing without classification." << std::endl;
        } else {
            // Example class names - replace with your actual classes
            classifier_thread.loadClassNames({
                "cat", "dog", "bird", "car", "person"
            });
        }
    }

    // Initialize GUI
    gui.initialize();

    // Start threads
    std::cout << "Starting threads..." << std::endl;
    std::thread camera_worker([&camera_thread]() {
        camera_thread.run();
    });

    std::thread classifier_worker([&classifier_thread]() {
        classifier_thread.run();
    });

    std::thread correlation_worker([&correlation_thread]() {
        correlation_thread.run();
    });

    // Main GUI loop (runs in main thread)
    std::cout << "Starting main loop..." << std::endl;
    state->status_message = "Running...";

    cv::Mat display_frame;
    while (state->running) {
        // Get latest frame for display
        if (state->frame_queue.pop(display_frame, 10)) {
            gui.displayFrame(display_frame);
        }

        // Process GUI events
        gui.processEvents();
    }

    // Cleanup
    std::cout << "Shutting down..." << std::endl;
    state->running = false;

    // Wait for threads to finish
    if (camera_worker.joinable()) {
        camera_worker.join();
    }
    if (classifier_worker.joinable()) {
        classifier_worker.join();
    }
    if (correlation_worker.joinable()) {
        correlation_worker.join();
    }

    gui.cleanup();

    std::cout << "Application terminated successfully" << std::endl;
    return 0;
}
