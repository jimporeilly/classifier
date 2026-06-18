#ifndef GUI_HANDLER_H
#define GUI_HANDLER_H

#include "shared_data.h"
#include "nano_trigger.h"
#include <opencv2/opencv.hpp>
#include <memory>

class GUIHandler {
private:
    std::shared_ptr<AppState> state_;
    NanoTrigger* trigger_;
    const std::string window_name_;

    cv::Mat control_panel_;
    int panel_width_;
    int panel_height_;
    int stats_height_;
    int frame_display_width_;
    int frame_display_height_;
    int actual_display_height_;

    // Trackbar variables
    static int correlation_trackbar_value_;

    // Active-hours range bar (1-24): two draggable windows during which auto-trigger may fire
    enum class HourBarDrag { NONE, WIN1_START, WIN1_END, WIN1_MOVE, WIN2_START, WIN2_END, WIN2_MOVE };
    HourBarDrag hour_bar_drag_ = HourBarDrag::NONE;
    int hour_bar_drag_anchor_x_ = 0;
    int hour_bar_drag_anchor_start_ = 0;
    int hour_bar_drag_anchor_end_ = 0;

    cv::Rect hourBarRect() const;
    int hourFromX(int x) const;
    int xFromHour(int hour) const;
    void drawHourBar(cv::Mat& img);
    void handleHourBarMouse(int event, int x, int y);

    void createControlPanel();
    void updateControlPanel();
    void drawButton(cv::Mat& img, const cv::Rect& rect, const std::string& text,
                   const cv::Scalar& color);
    void handleMouseClick(int event, int x, int y);
    static void mouseCallback(int event, int x, int y, int flags, void* userdata);
    void disableCloseButton();

public:
    GUIHandler(std::shared_ptr<AppState> state, NanoTrigger* trigger);
    ~GUIHandler();

    void initialize();
    void displayFrame(const cv::Mat& frame);
    void processEvents();
    void cleanup();
};

#endif // GUI_HANDLER_H
