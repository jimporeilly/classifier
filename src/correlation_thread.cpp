#include "correlation_thread.h"
#include <iostream>

CorrelationThread::CorrelationThread(std::shared_ptr<AppState> state)
    : app_state_(state) {
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

cv::Mat CorrelationThread::createDifferenceImage(const cv::Mat& img1, const cv::Mat& img2, int threshold_percent) {
    cv::Mat gray1, gray2;
    if (img1.channels() == 3) {
        cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
    } else {
        gray1 = img1.clone();
    }

    if (img2.channels() == 3) {
        cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);
    } else {
        gray2 = img2.clone();
    }

    if (gray1.size() != gray2.size()) {
        cv::resize(gray2, gray2, gray1.size());
    }

    // Calculate absolute difference
    cv::Mat diff;
    cv::absdiff(gray1, gray2, diff);

    // Create output image (start with current frame)
    cv::Mat result = img2.clone();

    // Calculate threshold value (percentage of 255)
    double threshold_value = (threshold_percent / 100.0) * 255.0;

    // Paint red pixels where difference exceeds threshold
    for (int y = 0; y < diff.rows; y++) {
        for (int x = 0; x < diff.cols; x++) {
            if (diff.at<uchar>(y, x) > threshold_value) {
                // Paint this pixel red
                if (result.channels() == 3) {
                    result.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 255); // BGR format: red
                } else {
                    result.at<uchar>(y, x) = 255;
                }
            }
        }
    }

    return result;
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

                    // Create difference image with red highlights
                    int threshold = app_state_->correlation_threshold.load();
                    cv::Mat diff_image = createDifferenceImage(previous_frame_, current_frame, threshold);

                    // Push to diff queue
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
