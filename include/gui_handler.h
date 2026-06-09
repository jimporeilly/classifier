#ifndef GUI_HANDLER_H
#define GUI_HANDLER_H

#include "shared_data.h"
#include "nano_trigger.h"
#include <opencv2/opencv.hpp>
#include <memory>

class GUIHandler {
private:
    std::shared_ptr<AppState> state_;
    const std::string window_name_;
NanoTrigger* trigger_;
    cv::Mat control_panel_;
    int panel_width_;
    int panel_height_;
    int stats_height_;
    int frame_display_width_;
    int frame_display_height_;
    int actual_display_height_;

    // Trackbar variables
    static int confidence_trackbar_value_;
    static int correlation_trackbar_value_;  // ADD THIS LINE
    static void onConfidenceChange(int value, void* userdata);

    void createControlPanel();
    void updateControlPanel();
    void drawButton(cv::Mat& img, const cv::Rect& rect, const std::string& text,
                   const cv::Scalar& color);
    void handleMouseClick(int event, int x, int y);
    static void mouseCallback(int event, int x, int y, int flags, void* userdata);

public:
    GUIHandler(std::shared_ptr<AppState> state, NanoTrigger* trigger);
    ~GUIHandler();

    void initialize();
    void displayFrame(const cv::Mat& frame);
    void processEvents();
    void cleanup();
};

#endif // GUI_HANDLER_H
