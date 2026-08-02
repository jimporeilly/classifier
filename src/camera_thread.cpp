#include "camera_thread.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>

CameraThread::CameraThread(std::shared_ptr<AppState> state, int left_sensor_id, int right_sensor_id)
    : state_(state), left_sensor_id_(left_sensor_id), right_sensor_id_(right_sensor_id) {
}

CameraThread::~CameraThread() {
    cleanup();
}

std::string CameraThread::buildPipeline(int sensor_id) const {
    return "nvarguscamerasrc sensor-id=" + std::to_string(sensor_id) +
           " ! video/x-raw(memory:NVMM), width=" + std::to_string(kCaptureWidth) +
           ", height=" + std::to_string(kCaptureHeight) +
           ", framerate=" + std::to_string(kFps) + "/1, format=NV12" +
           " ! nvvidconv ! video/x-raw, width=" + std::to_string(kOutputWidth) +
           ", height=" + std::to_string(kOutputHeight) + ", format=BGRx" +
           " ! videoconvert ! video/x-raw, format=BGR" +
           " ! appsink drop=1 max-buffers=1 sync=0";
}

bool CameraThread::openCamera(cv::VideoCapture& cap, int sensor_id, const char* label) {
    std::string pipeline = buildPipeline(sensor_id);
    cap.open(pipeline, cv::CAP_GSTREAMER);

    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open " << label << " CSI camera (sensor-id "
                   << sensor_id << ")" << std::endl;
        return false;
    }

    std::cout << label << " CSI camera (sensor-id " << sensor_id << ") initialized, "
               << kOutputWidth << "x" << kOutputHeight << " @ " << kFps << "fps" << std::endl;
    return true;
}

bool CameraThread::initialize() {
    bool left_ok = openCamera(capture_left_, left_sensor_id_, "Left");
    bool right_ok = openCamera(capture_right_, right_sensor_id_, "Right");

    if (!left_ok || !right_ok) {
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        state_->status_message = "Stereo camera initialization failed!";
        // Release whichever side did open so we don't leak a half-open pair.
        if (capture_left_.isOpened()) capture_left_.release();
        if (capture_right_.isOpened()) capture_right_.release();
        return false;
    }

    std::lock_guard<std::mutex> lock(state_->message_mutex);
    state_->status_message = "Stereo cameras initialized successfully";
    std::cout << "Stereo pair initialized (sensor-id " << left_sensor_id_
               << " / " << right_sensor_id_ << ")" << std::endl;
    return true;
}

void CameraThread::run() {
    if (!initialize()) {
        return;
    }

    cv::Mat left_frame, right_frame, stitched_frame;
    int frame_count = 0;
    auto fps_start = std::chrono::steady_clock::now();
    auto last_capture_time = std::chrono::steady_clock::now();
    bool warned_grab_fail = false;

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

        // Grab both sensors back-to-back before decoding either, to minimize
        // the temporal gap between the two frames (no hardware sync line on
        // this carrier). Only after both grabs succeed do we retrieve/decode.
        bool grabbed_left = capture_left_.grab();
        bool grabbed_right = capture_right_.grab();

        if (!grabbed_left || !grabbed_right) {
            if (!warned_grab_fail) {
                std::cerr << "Error: stereo grab failed (left=" << grabbed_left
                           << " right=" << grabbed_right << ")" << std::endl;
                warned_grab_fail = true;
            }
            // Unblock correlation so we don't deadlock on a dropped frame
            {
                std::lock_guard<std::mutex> lock(state_->correlation_sync_mutex);
                state_->correlation_done = true;
            }
            state_->correlation_cv.notify_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        warned_grab_fail = false;

        capture_left_.retrieve(left_frame);
        capture_right_.retrieve(right_frame);

        if (left_frame.empty() || right_frame.empty()) {
            std::cerr << "Error: Empty frame from stereo pair (left empty="
                       << left_frame.empty() << ", right empty=" << right_frame.empty()
                       << ")" << std::endl;
            {
                std::lock_guard<std::mutex> lock(state_->correlation_sync_mutex);
                state_->correlation_done = true;
            }
            state_->correlation_cv.notify_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Match heights defensively before stitching (should already be equal
        // since both pipelines request the same output size).
        if (left_frame.rows != right_frame.rows) {
            cv::resize(right_frame, right_frame, left_frame.size());
        }
        cv::hconcat(left_frame, right_frame, stitched_frame);

        // Push to display/classifier queue and dedicated correlation queue
        state_->frame_queue.push(stitched_frame.clone());
        state_->correlation_queue.push(stitched_frame.clone());

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
            cv::imwrite(filename, stitched_frame);
            std::lock_guard<std::mutex> lock(state_->message_mutex);
            state_->status_message = "Frame saved: " + filename;
            std::cout << "Frame saved: " << filename << std::endl;
        }
    }

    cleanup();
}

void CameraThread::cleanup() {
    if (capture_left_.isOpened()) {
        capture_left_.release();
        std::cout << "Left CSI camera released" << std::endl;
    }
    if (capture_right_.isOpened()) {
        capture_right_.release();
        std::cout << "Right CSI camera released" << std::endl;
    }
}
