#ifndef CORRELATION_THREAD_H
#define CORRELATION_THREAD_H

#include "shared_data.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <memory>

class CorrelationThread {
public:
    CorrelationThread(std::shared_ptr<AppState> state);
    ~CorrelationThread();

    void start();
    void stop();
    void join();
    void run();

private:
    double calculateCorrelation(const cv::Mat& img1, const cv::Mat& img2);
    cv::Mat createDifferenceImage(const cv::Mat& img1, const cv::Mat& img2, int threshold_percent);  // ADD THIS

    std::shared_ptr<AppState> app_state_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    cv::Mat previous_frame_;
    bool has_previous_frame_{false};
};

#endif // CORRELATION_THREAD_H
