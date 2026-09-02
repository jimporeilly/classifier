#ifndef GUI_HANDLER_H
#define GUI_HANDLER_H

#include "shared_data.h"
#include "stepper_controller.h"
#include <opencv2/opencv.hpp>
#include <memory>

class GUIHandler {
private:
    std::shared_ptr<AppState> state_;
    const std::string window_name_;
    StepperController* stepper_;
    cv::Mat control_panel_;
    int panel_width_;
    int panel_height_;
    int stats_height_;
    int frame_display_width_;
    int frame_display_height_;
    int actual_display_height_;

    // Manual control panel row y-positions (shared by draw + hit-test so
    // button geometry can't drift between the two).
    static constexpr int kManualToggleRowY = 165;
    static constexpr int kManualLegLabelY  = 205;
    static constexpr int kManualLegRowY    = 215;
    static constexpr int kManualActLabelY  = 250;
    static constexpr int kManualActRowY    = 260;
    static constexpr int kManualActAllRowY = 296;

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

    // Manual hexapod control panel: geometry helpers (used for both drawing
    // and click hit-testing) plus the draw/click handlers themselves.
    cv::Rect motorsToggleRect() const;
    cv::Rect legButtonRect(int leg, int which) const;          // which: 0=B, 1=F
    cv::Rect actuatorButtonRect(int act, int which) const;    // which: 0=E, 1=R, 2=S
    cv::Rect extendAllFeetRect() const;
    cv::Rect retractAllFeetRect() const;
    void drawManualControlPanel();
    bool handleManualControlClick(int x, int y);

public:
    GUIHandler(std::shared_ptr<AppState> state, StepperController* stepper);
    ~GUIHandler();

    void initialize();
    void displayFrame(const cv::Mat& frame);
    void processEvents();
    void cleanup();
};

#endif // GUI_HANDLER_H
