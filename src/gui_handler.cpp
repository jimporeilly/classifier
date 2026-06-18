#include "gui_handler.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

namespace {
    const std::string kAppVersion = "v1.2";

    Window findXWindowByTitle(Display* dpy, Window root, const std::string& title) {
        char* name = nullptr;
        if (XFetchName(dpy, root, &name) && name) {
            bool match = (title == name);
            XFree(name);
            if (match) return root;
        }
        Window dummy, *children = nullptr;
        unsigned int n = 0;
        if (!XQueryTree(dpy, root, &dummy, &dummy, &children, &n)) return 0;
        Window found = 0;
        for (unsigned int i = 0; i < n && !found; i++)
            found = findXWindowByTitle(dpy, children[i], title);
        if (children) XFree(children);
        return found;
    }
}

// Static member initialization
int GUIHandler::correlation_trackbar_value_ = 5;

GUIHandler::GUIHandler(std::shared_ptr<AppState> state, NanoTrigger* trigger)
    : state_(state),
      trigger_(trigger),
      window_name_("Camera Classifier"),
      panel_width_(1280),
      panel_height_(240 + AppState::THUMBNAIL_H + 4),
      stats_height_(30),
      frame_display_width_(640),
      frame_display_height_(480),
      actual_display_height_(480) {
}

GUIHandler::~GUIHandler() {
    cleanup();
}

void GUIHandler::disableCloseButton() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return;

    cv::waitKey(100);  // let the window render before we search for it
    Window win = findXWindowByTitle(dpy, DefaultRootWindow(dpy), window_name_);
    if (win) {
        // Motif WM hints: allow resize/move/minimize/maximize but NOT close
        struct MWMHints {
            unsigned long flags, functions, decorations;
            long          input_mode;
            unsigned long status;
        } hints = {};
        hints.flags     = 1UL;                              // MWM_HINTS_FUNCTIONS
        hints.functions = (1UL<<1)|(1UL<<2)|(1UL<<3)|(1UL<<4); // resize|move|minimize|maximize
        Atom atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        XChangeProperty(dpy, win, atom, atom, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&hints), 5);
        XFlush(dpy);
    }
    XCloseDisplay(dpy);
}

void GUIHandler::initialize() {
    cv::namedWindow(window_name_, cv::WINDOW_AUTOSIZE);
    disableCloseButton();
    cv::setMouseCallback(window_name_, mouseCallback, this);

    cv::createTrackbar("Trigger Threshold %", window_name_,
                      &correlation_trackbar_value_, 100, nullptr);

    createControlPanel();
    std::cout << "GUI initialized" << std::endl;
}

cv::Rect GUIHandler::hourBarRect() const {
    const int btn_y = 20, btn_h = 30;
    const int info_y = btn_y + btn_h + 25;
    const int bar_x0 = 20;
    const int bar_y  = info_y + 85;
    return cv::Rect(bar_x0, bar_y, panel_width_ - 2 * bar_x0, 22);
}

int GUIHandler::xFromHour(int hour) const {
    cv::Rect r = hourBarRect();
    hour = std::max(1, std::min(25, hour));
    return r.x + static_cast<int>(std::lround((hour - 1) * (r.width / 24.0)));
}

int GUIHandler::hourFromX(int x) const {
    cv::Rect r = hourBarRect();
    double seg = r.width / 24.0;
    int hour = static_cast<int>((x - r.x) / seg) + 1;
    return std::max(1, std::min(24, hour));
}

void GUIHandler::drawHourBar(cv::Mat& img) {
    cv::Rect r = hourBarRect();

    cv::putText(img, "Active Hours (local time) - auto-trigger only fires inside these windows",
                cv::Point(r.x, r.y - 8), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(220, 220, 220), 1);

    cv::rectangle(img, r, cv::Scalar(70, 70, 70), -1);
    cv::rectangle(img, r, cv::Scalar(120, 120, 120), 1);

    for (int h = 1; h <= 24; h++) {
        int x = xFromHour(h);
        cv::line(img, cv::Point(x, r.y), cv::Point(x, r.y + r.height), cv::Scalar(100, 100, 100), 1);
        if (h % 2 == 1) {
            cv::putText(img, std::to_string(h), cv::Point(x + 2, r.y + r.height + 14),
                        cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(180, 180, 180), 1);
        }
    }

    int w1s = state_->active_window1_start.load();
    int w1e = state_->active_window1_end.load();
    int w2s = state_->active_window2_start.load();
    int w2e = state_->active_window2_end.load();

    cv::Rect band1(xFromHour(w1s), r.y, xFromHour(w1e + 1) - xFromHour(w1s), r.height);
    cv::Rect band2(xFromHour(w2s), r.y, xFromHour(w2e + 1) - xFromHour(w2s), r.height);

    cv::rectangle(img, band1, cv::Scalar(0, 165, 255), -1);
    cv::rectangle(img, band2, cv::Scalar(255, 100, 0), -1);
    cv::rectangle(img, band1, cv::Scalar(255, 255, 255), 1);
    cv::rectangle(img, band2, cv::Scalar(255, 255, 255), 1);

    std::time_t now = std::time(nullptr);
    std::tm local_tm;
    localtime_r(&now, &local_tm);
    int cur_hour = local_tm.tm_hour + 1;
    int cx = (xFromHour(cur_hour) + xFromHour(cur_hour + 1)) / 2;
    cv::line(img, cv::Point(cx, r.y - 4), cv::Point(cx, r.y + r.height + 4), cv::Scalar(0, 255, 0), 2);
}

void GUIHandler::handleHourBarMouse(int event, int x, int y) {
    const int edge_tol = 6;

    if (event == cv::EVENT_LBUTTONUP) {
        hour_bar_drag_ = HourBarDrag::NONE;
        return;
    }

    if (event == cv::EVENT_LBUTTONDOWN) {
        cv::Rect r = hourBarRect();
        cv::Rect hit(r.x - edge_tol, r.y - edge_tol, r.width + 2 * edge_tol, r.height + 2 * edge_tol);
        if (!hit.contains(cv::Point(x, y))) return;

        int w1s = state_->active_window1_start.load();
        int w1e = state_->active_window1_end.load();
        int w2s = state_->active_window2_start.load();
        int w2e = state_->active_window2_end.load();

        int x_w1s = xFromHour(w1s), x_w1e = xFromHour(w1e + 1);
        int x_w2s = xFromHour(w2s), x_w2e = xFromHour(w2e + 1);

        if (std::abs(x - x_w1s) <= edge_tol)      hour_bar_drag_ = HourBarDrag::WIN1_START;
        else if (std::abs(x - x_w1e) <= edge_tol) hour_bar_drag_ = HourBarDrag::WIN1_END;
        else if (x > x_w1s && x < x_w1e)          hour_bar_drag_ = HourBarDrag::WIN1_MOVE;
        else if (std::abs(x - x_w2s) <= edge_tol) hour_bar_drag_ = HourBarDrag::WIN2_START;
        else if (std::abs(x - x_w2e) <= edge_tol) hour_bar_drag_ = HourBarDrag::WIN2_END;
        else if (x > x_w2s && x < x_w2e)          hour_bar_drag_ = HourBarDrag::WIN2_MOVE;
        else return;

        hour_bar_drag_anchor_x_ = x;
        hour_bar_drag_anchor_start_ = (hour_bar_drag_ == HourBarDrag::WIN1_MOVE) ? w1s :
                                       (hour_bar_drag_ == HourBarDrag::WIN2_MOVE) ? w2s : 0;
        hour_bar_drag_anchor_end_   = (hour_bar_drag_ == HourBarDrag::WIN1_MOVE) ? w1e :
                                       (hour_bar_drag_ == HourBarDrag::WIN2_MOVE) ? w2e : 0;
        return;
    }

    if (event == cv::EVENT_MOUSEMOVE && hour_bar_drag_ != HourBarDrag::NONE) {
        int hour = hourFromX(x);

        switch (hour_bar_drag_) {
            case HourBarDrag::WIN1_START: {
                int end = state_->active_window1_end.load();
                state_->active_window1_start = std::min(hour, end);
                break;
            }
            case HourBarDrag::WIN1_END: {
                int start = state_->active_window1_start.load();
                state_->active_window1_end = std::max(hour, start);
                break;
            }
            case HourBarDrag::WIN2_START: {
                int end = state_->active_window2_end.load();
                state_->active_window2_start = std::min(hour, end);
                break;
            }
            case HourBarDrag::WIN2_END: {
                int start = state_->active_window2_start.load();
                state_->active_window2_end = std::max(hour, start);
                break;
            }
            case HourBarDrag::WIN1_MOVE: {
                int delta = hourFromX(x) - hourFromX(hour_bar_drag_anchor_x_);
                int width = hour_bar_drag_anchor_end_ - hour_bar_drag_anchor_start_;
                int new_start = std::max(1, std::min(24 - width, hour_bar_drag_anchor_start_ + delta));
                state_->active_window1_start = new_start;
                state_->active_window1_end = new_start + width;
                break;
            }
            case HourBarDrag::WIN2_MOVE: {
                int delta = hourFromX(x) - hourFromX(hour_bar_drag_anchor_x_);
                int width = hour_bar_drag_anchor_end_ - hour_bar_drag_anchor_start_;
                int new_start = std::max(1, std::min(24 - width, hour_bar_drag_anchor_start_ + delta));
                state_->active_window2_start = new_start;
                state_->active_window2_end = new_start + width;
                break;
            }
            default: break;
        }
    }
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

    double correlation = state_->correlation_value.load();
    int correlation_percent = static_cast<int>(correlation * 100);
    int corr_threshold = state_->correlation_threshold.load();

    // --- Buttons row (left-aligned, small) ---
    const int btn_w = 130, btn_h = 30, btn_y = 20, btn_gap = 8, btn_x0 = 10;

    std::string auto_text = state_->trigger_enabled ? "AUTO: ON" : "AUTO: OFF";
    cv::Scalar auto_color = state_->trigger_enabled ? cv::Scalar(0, 200, 0) : cv::Scalar(0, 0, 180);
    std::string pause_text = state_->paused ? "RESUME" : "PAUSE";
    cv::Scalar pause_color = state_->paused ? cv::Scalar(100, 200, 100) : cv::Scalar(100, 100, 200);
    drawButton(control_panel_, cv::Rect(btn_x0,                       btn_y, btn_w, btn_h), pause_text,    pause_color);
    drawButton(control_panel_, cv::Rect(btn_x0 +   btn_w + btn_gap,   btn_y, btn_w, btn_h), "CAPTURE",     cv::Scalar(150, 150, 255));
    drawButton(control_panel_, cv::Rect(btn_x0 + 2*(btn_w + btn_gap), btn_y, btn_w, btn_h), "CLEAR QUEUE", cv::Scalar(200, 200, 100));
    drawButton(control_panel_, cv::Rect(btn_x0 + 3*(btn_w + btn_gap), btn_y, btn_w, btn_h), "EXIT",        cv::Scalar(80, 80, 180));
    drawButton(control_panel_, cv::Rect(btn_x0 + 4*(btn_w + btn_gap), btn_y, btn_w, btn_h), "TRIGGER D2",  cv::Scalar(0, 140, 255));
    drawButton(control_panel_, cv::Rect(btn_x0 + 5*(btn_w + btn_gap), btn_y, btn_w, btn_h), auto_text,     auto_color);

    bool trig_exceeded = state_->trigger_active.load();
    cv::Scalar trig_color = trig_exceeded ? cv::Scalar(0, 0, 200) : cv::Scalar(0, 180, 0);
    std::string trig_text  = trig_exceeded ? "TRIG: ACTIVE"       : "TRIG: READY";
    drawButton(control_panel_, cv::Rect(btn_x0 + 6*(btn_w + btn_gap), btn_y, btn_w, btn_h), trig_text, trig_color);

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

    cv::putText(control_panel_,
                "Correlation: " + std::to_string(correlation_percent) + "% (Threshold: " + std::to_string(corr_threshold) + "%)",
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

    drawHourBar(control_panel_);

    // --- Thumbnail strip ---
    {
        std::lock_guard<std::mutex> lock(state_->thumbnail_mutex);
        int n = state_->thumbnail_count;
        if (n > 0) {
            const int strip_y = panel_height_ - AppState::THUMBNAIL_H - 2;
            int x = 2;
            for (int i = 0; i < n && x < panel_width_; i++) {
                const cv::Mat& thumb = state_->trigger_thumbnails[i];
                if (thumb.empty()) continue;
                cv::Rect dst(x, strip_y, thumb.cols, thumb.rows);
                if (dst.x + dst.width <= panel_width_ && dst.y + dst.height <= panel_height_)
                    thumb.copyTo(control_panel_(dst));
                x += thumb.cols + 2;
            }
        }
    }
}

void GUIHandler::handleMouseClick(int event, int x, int y) {
    const int panel_y = y - actual_display_height_ - stats_height_;

    handleHourBarMouse(event, x, panel_y);
    if (hour_bar_drag_ != HourBarDrag::NONE) return;  // active-hours bar owns this gesture

    if (event != cv::EVENT_LBUTTONDOWN) return;
    if (panel_y < 0) return;  // click in video or stats area, ignore

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
    if (cv::Rect(btn_x0 + 4*(btn_w + btn_gap), btn_y, btn_w, btn_h).contains(cv::Point(x, panel_y))) {
    if (trigger_ && trigger_->isOpen()) {
        trigger_->manualTrigger();
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        state_->status_message = "D2 triggered manually";
    }
    return;
}
if (cv::Rect(btn_x0 + 5*(btn_w + btn_gap), btn_y, btn_w, btn_h).contains(cv::Point(x, panel_y))) {
        state_->trigger_enabled = !state_->trigger_enabled;
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        state_->status_message = state_->trigger_enabled ? "Auto-trigger ON" : "Auto-trigger OFF";
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

    int baseline = 0;
    cv::Size version_size = cv::getTextSize(kAppVersion, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
    cv::putText(composite, kAppVersion,
                cv::Point(composite.cols - version_size.width - 10, version_size.height + 8),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200), 1);

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
