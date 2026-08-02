#ifndef CAMERA_THREAD_H
#define CAMERA_THREAD_H

#include "shared_data.h"
#include <opencv2/opencv.hpp>
#include <memory>
#include <string>

// Handles the two on-board CSI cameras (Yahboom Jetson Orin carrier, IMX219 x2)
// as a synced stereo pair. Both sensors are captured back-to-back each cycle
// (grab() then grab(), then retrieve() then retrieve()) to minimize temporal
// skew, since this carrier has no hardware frame-sync line between the two
// CSI connectors. The two frames are horizontally stitched into a single
// cv::Mat so the rest of the pipeline (classifier/correlation/GUI) doesn't
// need to change yet.
class CameraThread {
private:
    std::shared_ptr<AppState> state_;
    int left_sensor_id_;
    int right_sensor_id_;
    cv::VideoCapture capture_left_;
    cv::VideoCapture capture_right_;

    // Sensor capture resolution (native Argus mode) vs. output resolution
    // (post nvvidconv scale-down, what actually gets pushed to the queues).
    static constexpr int kCaptureWidth  = 1280;
    static constexpr int kCaptureHeight = 720;
    static constexpr int kOutputWidth   = 640;
    static constexpr int kOutputHeight  = 480;
    static constexpr int kFps           = 30;

    std::string buildPipeline(int sensor_id) const;
    bool openCamera(cv::VideoCapture& cap, int sensor_id, const char* label);

public:
    // left_sensor_id / right_sensor_id correspond to nvarguscamerasrc's
    // sensor-id (0 and 1 on this carrier, confirmed via v4l2-ctl/gst-launch).
    CameraThread(std::shared_ptr<AppState> state, int left_sensor_id = 0, int right_sensor_id = 1);
    ~CameraThread();

    void run();
    bool initialize();
    void cleanup();
};

#endif // CAMERA_THREAD_H
