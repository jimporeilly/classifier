#ifndef CAMERA_THREAD_H
#define CAMERA_THREAD_H

#include "shared_data.h"
#include <opencv2/opencv.hpp>
#include <memory>

class CameraThread {
private:
    std::shared_ptr<AppState> state_;
    int camera_id_;
    cv::VideoCapture capture_;

public:
    CameraThread(std::shared_ptr<AppState> state, int camera_id = 0);
    ~CameraThread();
    
    void run();
    bool initialize();
    void cleanup();
};

#endif // CAMERA_THREAD_H
