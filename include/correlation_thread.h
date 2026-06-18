#ifndef CORRELATION_THREAD_H
#define CORRELATION_THREAD_H

#include "shared_data.h"
#include "nano_trigger.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <deque>

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
    std::vector<cv::Rect> detectLocalMotion(const cv::Mat& prev, const cv::Mat& curr, double threshold,
                                             std::vector<double>* cell_diffs_out = nullptr);  // CHANGE
    int grid_cols_ = 16;
    int grid_rows_ = 6;
    static constexpr size_t max_trigger_hot_cells_ = 10;  // suppress trigger if more cells than this go hot at once
    static constexpr size_t calm_history_size_ = 10;  // number of calm frames averaged for the elevated-cell baseline
    std::deque<std::vector<double>> calm_cell_diff_history_;  // per-cell diffs from the last calm_history_size_ calm frames (no cell over threshold)

private:
    double calculateCorrelation(const cv::Mat& img1, const cv::Mat& img2);
    cv::Mat createDifferenceImage(const cv::Mat& prev, const cv::Mat& curr, const std::vector<cv::Rect>& hot_cells);
    bool hasAdjacentHotCells(const std::vector<cv::Rect>& hot_cells, int cell_w, int cell_h, int roi_y) const;
    NanoTrigger* trigger_;
    std::shared_ptr<AppState> app_state_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    cv::Mat previous_frame_;
    bool has_previous_frame_{false};
};

#endif // CORRELATION_THREAD_H
