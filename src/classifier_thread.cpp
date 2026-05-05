#include "classifier_thread.h"
#include <iostream>
#include <chrono>
#include <thread>

ClassifierThread::ClassifierThread(std::shared_ptr<AppState> state)
    : state_(state), 
      device_(torch::kCPU),
      model_loaded_(false) {
    
    // Use GPU if available
    if (torch::cuda::is_available()) {
        device_ = torch::Device(torch::kCUDA);
        std::cout << "Using CUDA for inference" << std::endl;
    } else {
        std::cout << "CUDA not available, using CPU" << std::endl;
    }
    
    // Default class names (for demo purposes)
    class_names_ = {"Class 0", "Class 1", "Class 2", "Class 3", "Class 4"};
}

ClassifierThread::~ClassifierThread() {
}

bool ClassifierThread::loadModel(const std::string& model_path) {
    try {
        model_ = torch::jit::load(model_path);
        model_.to(device_);
        model_.eval();
        model_loaded_ = true;
        
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        state_->status_message = "Model loaded: " + model_path;
        std::cout << "Model loaded successfully from: " << model_path << std::endl;
        return true;
    } catch (const c10::Error& e) {
        std::cerr << "Error loading model: " << e.what() << std::endl;
        std::lock_guard<std::mutex> lock(state_->message_mutex);
        state_->status_message = "Model loading failed!";
        model_loaded_ = false;
        return false;
    }
}

void ClassifierThread::loadClassNames(const std::vector<std::string>& names) {
    class_names_ = names;
}

torch::Tensor ClassifierThread::preprocess(const cv::Mat& image) {
    // Convert BGR to RGB
    cv::Mat rgb_image;
    cv::cvtColor(image, rgb_image, cv::COLOR_BGR2RGB);
    
    // Resize to expected input size (e.g., 224x224 for many models)
    cv::Mat resized_image;
    cv::resize(rgb_image, resized_image, cv::Size(224, 224));
    
    // Convert to float and normalize to [0, 1]
    resized_image.convertTo(resized_image, CV_32FC3, 1.0 / 255.0);
    
    // Convert to tensor
    torch::Tensor tensor_image = torch::from_blob(
        resized_image.data, 
        {resized_image.rows, resized_image.cols, 3},
        torch::kFloat32
    );
    
    // Permute dimensions from HWC to CHW
    tensor_image = tensor_image.permute({2, 0, 1});
    
    // Normalize with ImageNet mean and std
    tensor_image[0] = tensor_image[0].sub_(0.485).div_(0.229);
    tensor_image[1] = tensor_image[1].sub_(0.456).div_(0.224);
    tensor_image[2] = tensor_image[2].sub_(0.406).div_(0.225);
    
    // Add batch dimension
    tensor_image = tensor_image.unsqueeze(0);
    
    return tensor_image.to(device_);
}

ClassificationResult ClassifierThread::postprocess(const torch::Tensor& output) {
    // Apply softmax to get probabilities
    torch::Tensor probabilities = torch::softmax(output, 1);
    
    // Get the maximum probability and its index
    auto max_result = probabilities.max(1);
    float confidence = std::get<0>(max_result).item<float>();
    int class_id = std::get<1>(max_result).item<int>();
    
    ClassificationResult result;
    result.confidence = confidence;
    result.class_id = class_id;
    
    if (class_id >= 0 && class_id < class_names_.size()) {
        result.class_name = class_names_[class_id];
    } else {
        result.class_name = "Unknown (ID: " + std::to_string(class_id) + ")";
    }
    
    return result;
}

ClassificationResult ClassifierThread::classify(const cv::Mat& frame) {
    ClassificationResult result;
    
    if (!model_loaded_) {
        result.class_name = "No model loaded";
        result.confidence = 0.0f;
        result.class_id = -1;
        return result;
    }
    
    try {
        // Preprocess the frame
        torch::Tensor input_tensor = preprocess(frame);
        
        // Run inference
        std::vector<torch::jit::IValue> inputs;
        inputs.push_back(input_tensor);
        
        torch::NoGradGuard no_grad;
        auto output = model_.forward(inputs).toTensor();
        
        // Postprocess the output
        result = postprocess(output);
        
    } catch (const c10::Error& e) {
        std::cerr << "Error during classification: " << e.what() << std::endl;
        result.class_name = "Classification error";
        result.confidence = 0.0f;
        result.class_id = -1;
    }
    
    return result;
}

void ClassifierThread::run() {
    cv::Mat frame;
    int inference_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (state_->running) {
        if (state_->paused || !model_loaded_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Get frame from queue
        if (!state_->frame_queue.pop(frame, 100)) {
            continue;
        }
        
        // Classify the frame
        auto result = classify(frame);
        
        // Update shared state with results
        if (result.confidence >= state_->confidence_threshold) {
            std::lock_guard<std::mutex> lock(state_->result_mutex);
            state_->classification_result = result.class_name;
            state_->classification_confidence = result.confidence;
        }
        
        // Calculate inference FPS
        inference_count++;
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time).count();
        
        if (elapsed >= 2) {
            float inference_fps = inference_count / static_cast<float>(elapsed);
            std::cout << "Inference FPS: " << inference_fps << std::endl;
            inference_count = 0;
            start_time = current_time;
        }
    }
}
