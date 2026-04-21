#include "gui_handler.h"
#include <iostream>

// Static member initialization
int GUIHandler::confidence_trackbar_value_ = 50;
int GUIHandler::correlation_trackbar_value_ = 10;  // ADD THIS LINE

GUIHandler::GUIHandler(std::shared_ptr<AppState> state)
    : state_(state),
      window_name_("Camera Classifier"),
      panel_width_(1280),
      panel_height_(180),
      stats_height_(30),
      frame_display_width_(640),
      frame_display_height_(480),
      actual_display_height_(480) {
}

GUIHandler::~GUIHandler() {
    cleanup();
}

void GUIHandler::onConfidenceChange(int value, void* userdata) {
    GUIHandler* gui = static_cast<GUIHandler*>(userdata);
    gui->state_->confidence_threshold = value / 100.0f;
}

void GUIHandler::initialize() {
    cv::namedWindow(window_name_, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(window_name_, mouseCallback, this);

    confidence_trackbar_value_ = static_cast<int>(state_->confidence_threshold * 100);
    cv::createTrackbar("Confidence %", window_name_,
                      &confidence_trackbar_value_, 100,
                      onConfidenceChange, this);

    cv::createTrackbar("Change Threshold %", window_name_,
                      &correlation_trackbar_value_, 100, nullptr);

    createControlPanel();
    std::cout << "GUI initialized" << std::endl;
}

void GUIHandler::createControlPanel() {
    control_panel_ = cv::Mat(panel_height_, panel_width_, CV_8UC3, cv::Scalar(50, 50, 50));
}

void GUIHandler::drawButton(cv::Mat& img, const cv::Rect& rect,
                           const std::string& text, const cv::Scalar& color) {
    // Draw button background
    cv::rectangle(img, rect, color, -1);
    cv::rectangle(img, rect, cv::Scalar(100, 100, 100), 2);

    // Draw button text
    int baseline = 0;
    cv::Size text_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX,
                                         0.35, 1, &baseline);
    cv::Point text_origin(
        rect.x + (rect.width - text_size.width) / 2,
        rect.y + (rect.height + text_size.height) / 2
    );
    cv::putText(img, text, text_origin, cv::FONT_HERSHEY_SIMPLEX,
                0.35, cv::Scalar(0, 0, 0), 1);
}

void GUIHandler::updateControlPanel() {
    control_panel_ = cv::Mat(panel_height_, panel_width_, CV_8UC3, cv::Scalar(50, 50, 50));

    state_->correlation_threshold = correlation_trackbar_value_;

    // --- Buttons row (left-aligned, small) ---
    const int btn_w = 130, btn_h = 30, btn_y = 20, btn_gap = 8, btn_x0 = 10;

    std::string pause_text = state_->paused ? "RESUME" : "PAUSE";
    cv::Scalar pause_color = state_->paused ? cv::Scalar(100, 200, 100) : cv::Scalar(100, 100, 200);
    drawButton(control_panel_, cv::Rect(btn_x0,                       btn_y, btn_w, btn_h), pause_text,    pause_color);
    drawButton(control_panel_, cv::Rect(btn_x0 +   btn_w + btn_gap,   btn_y, btn_w, btn_h), "CAPTURE",     cv::Scalar(150, 150, 255));
    drawButton(control_panel_, cv::Rect(btn_x0 + 2*(btn_w + btn_gap), btn_y, btn_w, btn_h), "CLEAR QUEUE", cv::Scalar(200, 200, 100));
    drawButton(control_panel_, cv::Rect(btn_x0 + 3*(btn_w + btn_gap), btn_y, btn_w, btn_h), "EXIT",        cv::Scalar(80, 80, 180));

    // --- Info row ---
    const int info_y = btn_y + btn_h + 25;
    const int col2_x = panel_width_ / 2;
    const cv::Scalar text_color(220, 220, 220);

    std::string status;
    {
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        status = state_->status_message;
    }
    cv::putText(control_panel_, "STATUS: " + status,
                cv::Point(20, info_y), cv::FONT_HERSHEY_SIMPLEX, 0.5, text_color, 1);

    double correlation = state_->correlation_value.load();
    int correlation_percent = static_cast<int>(correlation * 100);
    int threshold = state_->correlation_threshold.load();
    cv::putText(control_panel_,
                "Correlation: " + std::to_string(correlation_percent) + "% (Threshold: " + std::to_string(threshold) + "%)",
                cv::Point(20, info_y + 30), cv::FONT_HERSHEY_SIMPLEX, 0.5, text_color, 1);

    std::string result;
    float confidence;
    {
        std::lock_guard<std::mutex> lock(state_->result_mutex);
        result = state_->classification_result;
        confidence = state_->classification_confidence;
    }
    cv::putText(control_panel_, "CLASSIFICATION:", cv::Point(col2_x, info_y),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, text_color, 1);
    if (!result.empty()) {
        cv::putText(control_panel_, result, cv::Point(col2_x, info_y + 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(100, 255, 100), 2);
        char conf_buf[32];
        snprintf(conf_buf, sizeof(conf_buf), "Confidence: %.1f%%", confidence * 100);
        cv::putText(control_panel_, conf_buf, cv::Point(col2_x, info_y + 60),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, text_color, 1);
    }
}

void GUIHandler::handleMouseClick(int event, int x, int y) {
    if (event != cv::EVENT_LBUTTONDOWN) return;
    if (y < actual_display_height_ + stats_height_) return;  // click in video or stats area, ignore

    const int panel_y = y - actual_display_height_ - stats_height_;
    const int btn_w = 130, btn_h = 30, btn_y = 20, btn_gap = 8, btn_x0 = 10;

    if (cv::Rect(btn_x0,                       btn_y, btn_w, btn_h).contains(cv::Point(x, panel_y))) {
        state_->paused = !state_->paused;
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        state_->status_message = state_->paused ? "Processing paused" : "Processing resumed";
        return;
    }
    if (cv::Rect(btn_x0 +   btn_w + btn_gap,   btn_y, btn_w, btn_h).contains(cv::Point(x, panel_y))) {
        state_->save_frame = true;
        return;
    }
    if (cv::Rect(btn_x0 + 2*(btn_w + btn_gap), btn_y, btn_w, btn_h).contains(cv::Point(x, panel_y))) {
        state_->frame_queue.clear();
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        state_->status_message = "Frame queue cleared";
        return;
    }
    if (cv::Rect(btn_x0 + 3*(btn_w + btn_gap), btn_y, btn_w, btn_h).contains(cv::Point(x, panel_y))) {
        state_->running = false;
        return;
    }
}

void GUIHandler::mouseCallback(int event, int x, int y, int flags, void* userdata) {
    GUIHandler* gui = static_cast<GUIHandler*>(userdata);
    gui->handleMouseClick(event, x, y);
}

void GUIHandler::displayFrame(const cv::Mat& frame) {
    if (frame.empty()) return;

    // Compute display height preserving aspect ratio
    actual_display_height_ = frame.rows * frame_display_width_ / frame.cols;

    // Left panel: camera feed with classification overlay
    cv::Mat left;
    cv::resize(frame, left, cv::Size(frame_display_width_, actual_display_height_));

    std::string result;
    float confidence;
    {
        std::lock_guard<std::mutex> lock(state_->result_mutex);
        result = state_->classification_result;
        confidence = state_->classification_confidence;
    }
    if (!result.empty()) {
        char text[128];
        snprintf(text, sizeof(text), "%s (%.1f%%)", result.c_str(), confidence * 100);
        cv::putText(left, text, cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
    }
    cv::putText(left, "Camera Feed", cv::Point(10, actual_display_height_ - 10),
               cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200), 1);

    // Right panel: difference view
    cv::Mat right;
    cv::Mat diff_image;
    if (state_->diff_image_queue.pop(diff_image, 0) && !diff_image.empty()) {
        cv::resize(diff_image, right, cv::Size(frame_display_width_, actual_display_height_));
    } else {
        right = cv::Mat(actual_display_height_, frame_display_width_, CV_8UC3, cv::Scalar(30, 30, 30));
        cv::putText(right, "Waiting for diff...",
                   cv::Point(frame_display_width_ / 2 - 90, actual_display_height_ / 2),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(100, 100, 100), 1);
    }
    cv::putText(right, "Difference View", cv::Point(10, actual_display_height_ - 10),
               cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200), 1);

    // Stats strip: snap interval under left, processing time under right
    const int total_width = frame_display_width_ * 2;
    cv::Mat stats_row(stats_height_, total_width, CV_8UC3, cv::Scalar(40, 40, 40));
    const cv::Scalar stats_color(220, 220, 220);

    char snap_buf[64], proc_buf[64];
    snprintf(snap_buf, sizeof(snap_buf), "Snap interval: %d ms", state_->snap_interval_ms.load());
    snprintf(proc_buf, sizeof(proc_buf), "Processing time: %d ms", state_->processing_time_ms.load());
    cv::putText(stats_row, snap_buf, cv::Point(10, 20),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, stats_color, 1);
    cv::putText(stats_row, proc_buf, cv::Point(frame_display_width_ + 10, 20),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, stats_color, 1);

    // Composite: video row, stats strip, control panel
    cv::Mat composite(actual_display_height_ + stats_height_ + panel_height_, total_width, CV_8UC3);
    left.copyTo(composite(cv::Rect(0, 0, frame_display_width_, actual_display_height_)));
    right.copyTo(composite(cv::Rect(frame_display_width_, 0, frame_display_width_, actual_display_height_)));
    stats_row.copyTo(composite(cv::Rect(0, actual_display_height_, total_width, stats_height_)));

    updateControlPanel();
    control_panel_.copyTo(composite(cv::Rect(0, actual_display_height_ + stats_height_, total_width, panel_height_)));

    cv::imshow(window_name_, composite);
}

void GUIHandler::processEvents() {
    int key = cv::waitKey(1);

    if (key == 27) { // ESC key
        state_->running = false;
    } else if (key == 'p' || key == 'P') {
        state_->paused = !state_->paused;
    } else if (key == 'c' || key == 'C') {
        state_->save_frame = true;
    }
}

void GUIHandler::cleanup() {
    cv::destroyAllWindows();
    std::cout << "GUI cleaned up" << std::endl;
}
