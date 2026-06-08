#ifndef CORRELATION_THREAD_H
#define CORRELATION_THREAD_H

#include "shared_data.h"
#include "nano_trigger.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <memory>

class CorrelationThread {
public:
    CorrelationThread(std::shared_ptr<AppState> state, NanoTrigger* trigger);
    ~CorrelationThread();

    void start();
    void stop();
    void join();
    void run();

private:
    //double calculateCorrelation(const cv::Mat& a, const cv::Mat& b);
    double calculateCellCorrelation(const cv::Mat& a, const cv::Mat& b, const cv::Rect& region);  // ADD
    //bool detectLocalMotion(const cv::Mat& prev, const cv::Mat& curr, double threshold);            // ADD
    std::vector<cv::Rect> detectLocalMotion(const cv::Mat& prev, const cv::Mat& curr, double threshold);  // CHANGE
    int grid_cols_ = 6;   // ADD — tune these to taste
    int grid_rows_ = 4;   // ADD

private:
    double calculateCorrelation(const cv::Mat& img1, const cv::Mat& img2);
   cv::Mat createDifferenceImage(const cv::Mat& prev, const cv::Mat& curr, const std::vector<cv::Rect>& hot_cells);  // CHANGE
    NanoTrigger* trigger_;
    std::shared_ptr<AppState> app_state_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    cv::Mat previous_frame_;
    bool has_previous_frame_{false};
};

#endif // CORRELATION_THREAD_H
