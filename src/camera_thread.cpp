#include "camera_thread.h"
#include <iostream>
#include <chrono>
#include <thread>

CameraThread::CameraThread(std::shared_ptr<AppState> state, int camera_id)
    : state_(state), camera_id_(camera_id) {
}

CameraThread::~CameraThread() {
    cleanup();
}

bool CameraThread::initialize() {
    capture_.open(camera_id_);

    if (!capture_.isOpened()) {
        std::cerr << "Error: Could not open camera " << camera_id_ << std::endl;
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        state_->status_message = "Camera initialization failed!";
        return false;
    }

    // Set camera properties for better performance
    capture_.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture_.set(cv::CAP_PROP_FPS, 30);

    std::lock_guard<std::mutex> lock(state_->message_mutex);
    state_->status_message = "Camera initialized successfully";

    std::cout << "Camera " << camera_id_ << " initialized successfully" << std::endl;
    return true;
}

void CameraThread::run() {
    if (!initialize()) {
        return;
    }

    cv::Mat frame;
    int frame_count = 0;
    auto fps_start = std::chrono::steady_clock::now();
    auto last_capture_time = std::chrono::steady_clock::now();

    while (state_->running) {
        if (state_->paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Wait for correlation thread to finish processing the previous frame
        {
            std::unique_lock<std::mutex> lock(state_->correlation_sync_mutex);
            state_->correlation_cv.wait_for(lock, std::chrono::milliseconds(1000),
                [this]{ return state_->correlation_done.load() || !state_->running; });
            state_->correlation_done = false;
        }

        if (!state_->running) break;

        // Measure time since last capture
        auto now = std::chrono::steady_clock::now();
        state_->snap_interval_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_capture_time).count());
        last_capture_time = now;

        // Capture frame
        capture_ >> frame;

        if (frame.empty()) {
            std::cerr << "Error: Empty frame captured" << std::endl;
            // Unblock correlation so we don't deadlock on empty frame
            {
                std::lock_guard<std::mutex> lock(state_->correlation_sync_mutex);
                state_->correlation_done = true;
            }
            state_->correlation_cv.notify_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Push to display/classifier queue and dedicated correlation queue
        state_->frame_queue.push(frame.clone());
        state_->correlation_queue.push(frame.clone());

        // Calculate FPS
        frame_count++;
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - fps_start).count();

        if (elapsed >= 1) {
            std::lock_guard<std::mutex> lock(state_->message_mutex);
            state_->status_message = "Camera FPS: " + std::to_string(frame_count);
            frame_count = 0;
            fps_start = current_time;
        }

        // Save frame if requested
        if (state_->save_frame.exchange(false)) {
            std::string filename = "captured_frame_" +
                std::to_string(std::time(nullptr)) + ".jpg";
            cv::imwrite(filename, frame);
            std::lock_guard<std::mutex> lock(state_->message_mutex);
            state_->status_message = "Frame saved: " + filename;
            std::cout << "Frame saved: " << filename << std::endl;
        }
    }

    cleanup();
}



void CameraThread::cleanup() {
    if (capture_.isOpened()) {
        capture_.release();
        std::cout << "Camera released" << std::endl;
    }
}
