#include "correlation_thread.h"
#include <iostream>
#include <ctime>
#include <sys/stat.h>

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

    // Draw mean pixel-diff value at the center of every cell (bottom half only)
    cv::Mat gray_prev, gray_curr_mat;
    cv::cvtColor(prev, gray_prev, cv::COLOR_BGR2GRAY);
    cv::cvtColor(curr, gray_curr_mat, cv::COLOR_BGR2GRAY);
    int roi_y   = curr.rows / 2;
    int cell_w  = curr.cols / grid_cols_;
    int cell_h  = (curr.rows / 2) / grid_rows_;

    for (int row = 0; row < grid_rows_; row++) {
        for (int col = 0; col < grid_cols_; col++) {
            cv::Rect region(col * cell_w, roi_y + row * cell_h, cell_w, cell_h);
            cv::Mat cell_diff;
            cv::absdiff(gray_prev(region), gray_curr_mat(region), cell_diff);
            double mean_diff = cv::mean(cell_diff)[0];

            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", mean_diff);

            int baseline = 0;
            cv::Size ts = cv::getTextSize(buf, cv::FONT_HERSHEY_SIMPLEX, 0.4, 1, &baseline);
            cv::Point origin(region.x + (region.width - ts.width) / 2,
                             region.y + (region.height + ts.height) / 2);
            // Black outline then white text for visibility on any background
            cv::putText(display, buf, origin, cv::FONT_HERSHEY_SIMPLEX, 0.4,
                        cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
            cv::putText(display, buf, origin, cv::FONT_HERSHEY_SIMPLEX, 0.4,
                        cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        }
    }

    return display;
}

bool CorrelationThread::hasAdjacentHotCells(const std::vector<cv::Rect>& hot_cells,
                                              int cell_w, int cell_h, int roi_y) const {
    for (size_t i = 0; i < hot_cells.size(); i++) {
        int ri = (hot_cells[i].y - roi_y) / cell_h;
        int ci = hot_cells[i].x / cell_w;
        for (size_t j = i + 1; j < hot_cells.size(); j++) {
            int rj = (hot_cells[j].y - roi_y) / cell_h;
            int cj = hot_cells[j].x / cell_w;
            if (std::abs(ri - rj) + std::abs(ci - cj) == 1)
                return true;
        }
    }
    return false;
}

double CorrelationThread::calculateCellCorrelation(const cv::Mat& a, const cv::Mat& b, const cv::Rect& region) {
    cv::Mat cell_a = a(region);
    cv::Mat cell_b = b(region);
    // Reuse your existing calculateCorrelation logic on the sub-region
    return calculateCorrelation(cell_a, cell_b);
}

std::vector<cv::Rect> CorrelationThread::detectLocalMotion(const cv::Mat& prev, const cv::Mat& curr, double threshold,
                                                             std::vector<double>* cell_diffs_out) {
    // threshold = mean absolute pixel difference (0–255) required to flag a cell
    // Analysis uses bottom half of frame only
    cv::Mat gray_prev, gray_curr;
    cv::cvtColor(prev, gray_prev, cv::COLOR_BGR2GRAY);
    cv::cvtColor(curr, gray_curr, cv::COLOR_BGR2GRAY);

    int roi_y  = prev.rows / 2;
    int cell_w = prev.cols / grid_cols_;
    int cell_h = (prev.rows / 2) / grid_rows_;
    std::vector<cv::Rect> hot_cells;

    for (int row = 0; row < grid_rows_; row++) {
        for (int col = 0; col < grid_cols_; col++) {
            cv::Rect region(col * cell_w, roi_y + row * cell_h, cell_w, cell_h);
            cv::Mat diff;
            cv::absdiff(gray_prev(region), gray_curr(region), diff);
            double mean_diff = cv::mean(diff)[0];
            if (cell_diffs_out)
                cell_diffs_out->push_back(mean_diff);
            if (mean_diff > threshold)
                hot_cells.push_back(region);
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
                    // Calculate correlation on bottom half only
                    int roi_y = current_frame.rows / 2;
                    cv::Rect bottom_half(0, roi_y, current_frame.cols, current_frame.rows - roi_y);
                    double correlation = calculateCorrelation(previous_frame_(bottom_half), current_frame(bottom_half));

                    app_state_->correlation_value = correlation;

                    // Use local motion detection — threshold is mean pixel diff (0–255)
                    double threshold = app_state_->correlation_threshold.load();
                    std::vector<double> cell_diffs;
                    std::vector<cv::Rect> hot_cells = detectLocalMotion(previous_frame_, current_frame, threshold, &cell_diffs);

                    // Suppress trigger if more than half of the cells spike to 2x or more of
                    // the running average from the last calm_history_size_ calm frames (frames
                    // where no cell was over threshold) — usually a global lighting/exposure
                    // shift rather than localized motion.
                    bool too_many_elevated = false;
                    if (!calm_cell_diff_history_.empty())
                    {
                        std::vector<double> baseline(cell_diffs.size(), 0.0);
                        for (const auto& frame_diffs : calm_cell_diff_history_)
                            for (size_t i = 0; i < frame_diffs.size(); i++)
                                baseline[i] += frame_diffs[i];
                        for (double& b : baseline)
                            b /= calm_cell_diff_history_.size();

                        size_t elevated_count = 0;
                        for (size_t i = 0; i < cell_diffs.size(); i++)
                        {
                            if (cell_diffs[i] >= 2.0 * baseline[i])
                                elevated_count++;
                        }
                        too_many_elevated = elevated_count > cell_diffs.size() / 2;
                    }

                    if (hot_cells.empty())
                    {
                        calm_cell_diff_history_.push_back(cell_diffs);
                        if (calm_cell_diff_history_.size() > calm_history_size_)
                            calm_cell_diff_history_.pop_front();
                    }

                    // Auto-trigger only fires inside the GUI's active-hours windows (1-24 local hour)
                    std::time_t now = std::time(nullptr);
                    std::tm local_tm;
                    localtime_r(&now, &local_tm);
                    int cur_hour = local_tm.tm_hour + 1;
                    bool in_window1 = cur_hour >= app_state_->active_window1_start.load() &&
                                       cur_hour <= app_state_->active_window1_end.load();
                    bool in_window2 = cur_hour >= app_state_->active_window2_start.load() &&
                                       cur_hour <= app_state_->active_window2_end.load();

                    int cell_w = current_frame.cols / grid_cols_;
                    int cell_h = (current_frame.rows / 2) / grid_rows_;
                    // Core motion-threshold check, independent of the alarm enable button
                    // and the active-hours windows — those only gate the physical D2 alarm,
                    // not whether evidence gets captured.
                    bool threshold_exceeded = hot_cells.size() > 1 &&
                                     hot_cells.size() <= max_trigger_hot_cells_ &&
                                     !too_many_elevated &&
                                     hasAdjacentHotCells(hot_cells, cell_w, cell_h, roi_y);
                    bool do_trigger = threshold_exceeded && (in_window1 || in_window2);
                    app_state_->trigger_active = threshold_exceeded;

                // Build annotated diff image (cell values + hot cell overlay)
                cv::Mat diff_image = createDifferenceImage(previous_frame_, current_frame, hot_cells);

                if (threshold_exceeded)
                {
                    // Always save evidence when the threshold is surpassed, even if the
                    // alarm button is off or the current time falls outside the active-hours
                    // windows — those should only suppress the physical D2 alarm pulse.
                    mkdir(app_state_->save_dir.c_str(), 0755);
                    std::string filename = app_state_->save_dir + "/trigger_" +
                                           std::to_string(std::time(nullptr)) + ".jpg";
                    cv::imwrite(filename, diff_image);

                    // Build thumbnail from annotated image
                    int tw = AppState::THUMBNAIL_H * diff_image.cols / diff_image.rows;
                    cv::Mat thumb;
                    cv::resize(diff_image, thumb, cv::Size(tw, AppState::THUMBNAIL_H));
                    {
                        std::lock_guard<std::mutex> lock(app_state_->thumbnail_mutex);
                        int idx = app_state_->thumbnail_write_idx;
                        if ((int)app_state_->trigger_thumbnails.size() <= idx)
                            app_state_->trigger_thumbnails.resize(idx + 1);
                        app_state_->trigger_thumbnails[idx] = thumb;
                        app_state_->thumbnail_write_idx =
                            (idx + 1) % AppState::MAX_THUMBNAILS;
                        if (app_state_->thumbnail_count < AppState::MAX_THUMBNAILS)
                            app_state_->thumbnail_count++;
                    }

                    if (app_state_->trigger_enabled && do_trigger && trigger_) {
                        trigger_->manualTrigger();
                    }
                }

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
