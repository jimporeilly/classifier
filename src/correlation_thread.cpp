#include "correlation_thread.h"
#include <iostream>

CorrelationThread::CorrelationThread(std::shared_ptr<AppState> state, NanoTrigger* trigger)
    : app_state_(state),
    trigger_(trigger),
    has_previous_frame_(false)
    {
}

CorrelationThread::~CorrelationThread() {
}

void CorrelationThread::start() {
    if (!running_) {
        running_ = true;
        thread_ = std::thread(&CorrelationThread::run, this);
    }
}

void CorrelationThread::stop() {
    running_ = false;
}

void CorrelationThread::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

double CorrelationThread::calculateCorrelation(const cv::Mat& img1, const cv::Mat& img2) {
    cv::Mat gray1, gray2;
    if (img1.channels() == 3) {
        cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
    } else {
        gray1 = img1;
    }

    if (img2.channels() == 3) {
        cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);
    } else {
        gray2 = img2;
    }

    if (gray1.size() != gray2.size()) {
        cv::resize(gray2, gray2, gray1.size());
    }

    cv::Mat f1, f2;
    gray1.convertTo(f1, CV_32F);
    gray2.convertTo(f2, CV_32F);

    cv::Scalar mean1 = cv::mean(f1);
    cv::Scalar mean2 = cv::mean(f2);

    f1 -= mean1[0];
    f2 -= mean2[0];

    double numerator = f1.dot(f2);
    double denom1 = std::sqrt(f1.dot(f1));
    double denom2 = std::sqrt(f2.dot(f2));

    if (denom1 == 0 || denom2 == 0) {
        return 0.0;
    }

    return numerator / (denom1 * denom2);
}

cv::Mat CorrelationThread::createDifferenceImage(const cv::Mat& prev, const cv::Mat& curr,
                                                  const std::vector<cv::Rect>& hot_cells) {
    // Start with a grey version of current frame so context is visible
    cv::Mat grey;
    cv::cvtColor(curr, grey, cv::COLOR_BGR2GRAY);
    cv::Mat display;
    cv::cvtColor(grey, display, cv::COLOR_GRAY2BGR);

    // Paint only the hot cells red
    for (const cv::Rect& cell : hot_cells) {
        cv::Mat cell_curr = curr(cell);
        cv::Mat cell_prev = prev(cell);

        // Per-pixel absolute difference within the cell
        cv::Mat diff;
        cv::absdiff(cell_curr, cell_prev, diff);
        cv::cvtColor(diff, diff, cv::COLOR_BGR2GRAY);

        // Threshold to get a mask of changed pixels
        cv::Mat mask;
        cv::threshold(diff, mask, 20, 255, cv::THRESH_BINARY);

        // Apply red tint to changed pixels only
        cv::Mat red_cell(cell.height, cell.width, CV_8UC3, cv::Scalar(0, 0, 255));
        red_cell.copyTo(display(cell), mask);
    }

    return display;
}

double CorrelationThread::calculateCellCorrelation(const cv::Mat& a, const cv::Mat& b, const cv::Rect& region) {
    cv::Mat cell_a = a(region);
    cv::Mat cell_b = b(region);
    // Reuse your existing calculateCorrelation logic on the sub-region
    return calculateCorrelation(cell_a, cell_b);
}

std::vector<cv::Rect> CorrelationThread::detectLocalMotion(const cv::Mat& prev, const cv::Mat& curr, double threshold) {
    int cell_w = prev.cols / grid_cols_;
    int cell_h = prev.rows / grid_rows_;
    std::vector<cv::Rect> hot_cells;

    for (int row = 0; row < grid_rows_; row++) {
        for (int col = 0; col < grid_cols_; col++) {
            cv::Rect region(col * cell_w, row * cell_h, cell_w, cell_h);
            double cell_corr = calculateCellCorrelation(prev, curr, region);
            if (cell_corr < threshold) {
                hot_cells.push_back(region);
            }
        }
    }
    return hot_cells;
}

void CorrelationThread::run() {
    std::cout << "Correlation thread started" << std::endl;

    while (app_state_->running) {
        if (app_state_->paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        cv::Mat current_frame;
        if (app_state_->correlation_queue.pop(current_frame, 500)) {
            if (!current_frame.empty()) {
                auto proc_start = std::chrono::steady_clock::now();

                if (has_previous_frame_ && !previous_frame_.empty()) {
                    // Calculate correlation
                    double correlation = calculateCorrelation(previous_frame_, current_frame);

                    app_state_->correlation_value = correlation;

                     // Use local motion detection instead of global correlation
                    double threshold = app_state_->confidence_threshold.load();
                    std::vector<cv::Rect> hot_cells = detectLocalMotion(previous_frame_, current_frame, threshold);

                if (app_state_->trigger_enabled)
                {
                    if (!hot_cells.empty() && trigger_)
                        trigger_->manualTrigger();
                }

                // Pass hot cells into diff image so only they go red
                cv::Mat diff_image = createDifferenceImage(previous_frame_, current_frame, hot_cells);
                app_state_->diff_image_queue.push(diff_image);
                }

                previous_frame_ = current_frame.clone();
                has_previous_frame_ = true;

                auto proc_end = std::chrono::steady_clock::now();
                app_state_->processing_time_ms = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(proc_end - proc_start).count());
            }

            // Signal camera that it can capture the next frame
            {
                std::lock_guard<std::mutex> lock(app_state_->correlation_sync_mutex);
                app_state_->correlation_done = true;
            }
            app_state_->correlation_cv.notify_one();
        }
    }

    std::cout << "Correlation thread stopped" << std::endl;
}
