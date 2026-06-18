#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <opencv2/opencv.hpp>
#include <torch/torch.h>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <cstdlib>

// Thread-safe queue for passing frames between threads
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    size_t max_size_;

public:
    ThreadSafeQueue(size_t max_size = 10) : max_size_(max_size) {}

    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.size() >= max_size_) {
            queue_.pop(); // Drop old frames if queue is full
        }
        queue_.push(std::move(item));
        cond_.notify_one();
    }

    bool pop(T& item, int timeout_ms = 100) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cond_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                          [this] { return !queue_.empty(); })) {
            item = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        return false;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }
};

// Shared application state
struct AppState {
    static constexpr int THUMBNAIL_H = 120;
    static constexpr int MAX_THUMBNAILS = 10;
    std::atomic<bool> running{true};
    std::atomic<bool> paused{false};
    std::atomic<bool> save_frame{false};
    std::atomic<int> camera_id{0};
    std::atomic<double> correlation_value{1.0};
    std::atomic<int> correlation_threshold{5};  // ADD THIS: percentage change threshold

    ThreadSafeQueue<cv::Mat> frame_queue;
    ThreadSafeQueue<cv::Mat> diff_image_queue;  // ADD THIS: for highlighted difference images
    ThreadSafeQueue<cv::Mat> correlation_queue; // dedicated queue for correlation thread

    // Timing stats displayed under each video panel
    std::atomic<int> snap_interval_ms{0};
    std::atomic<int> processing_time_ms{0};

    // Sync: camera waits for correlation to finish before next capture
    std::mutex correlation_sync_mutex;
    std::condition_variable correlation_cv;
    std::atomic<bool> correlation_done{true};
    std::atomic<bool> trigger_enabled{false};  // default OFF on startup
    std::atomic<bool> trigger_active{false};   // true when motion threshold exceeded this frame

    // Hour-of-day windows (1-24) during which auto-trigger is allowed to fire. Two
    // independent windows, adjustable from the GUI's active-hours bar.
    std::atomic<int> active_window1_start{5};
    std::atomic<int> active_window1_end{7};
    std::atomic<int> active_window2_start{20};
    std::atomic<int> active_window2_end{22};

    // Trigger capture thumbnails — ring buffer, written by correlation thread, read by GUI
    std::vector<cv::Mat> trigger_thumbnails;
    std::mutex thumbnail_mutex;
    int thumbnail_write_idx{0};
    int thumbnail_count{0};
    std::string save_dir{[] {
        const char* home = std::getenv("HOME");
        return (home ? std::string(home) : std::string(".")) + "/trigger_captures";
    }()};

    std::mutex message_mutex;
    std::string status_message;

    std::mutex result_mutex;
    std::string classification_result;
    float classification_confidence{0.0f};
};

// Classification result structure
struct ClassificationResult {
    std::string class_name;
    float confidence;
    int class_id;
};

#endif // SHARED_DATA_H
