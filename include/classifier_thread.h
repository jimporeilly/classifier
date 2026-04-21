#ifndef CLASSIFIER_THREAD_H
#define CLASSIFIER_THREAD_H

#include "shared_data.h"
#include <torch/torch.h>
#include <torch/script.h>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <string>

class ClassifierThread {
private:
    std::shared_ptr<AppState> state_;
    torch::jit::script::Module model_;
    torch::Device device_;
    bool model_loaded_;
    std::vector<std::string> class_names_;
    
    torch::Tensor preprocess(const cv::Mat& image);
    ClassificationResult postprocess(const torch::Tensor& output);

public:
    ClassifierThread(std::shared_ptr<AppState> state);
    ~ClassifierThread();
    
    bool loadModel(const std::string& model_path);
    void loadClassNames(const std::vector<std::string>& names);
    void run();
    ClassificationResult classify(const cv::Mat& frame);
};

#endif // CLASSIFIER_THREAD_H
