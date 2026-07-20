#pragma once

#include <memory>
#include <opencv2/core/mat.hpp>
#include <string>

#include "omniface/types.h"

namespace omniface::recognition {

class FaceRecognizer {
public:
    struct Config {
        int input_width = 112;
        int input_height = 112;
        int intra_threads = 1;
    };

    explicit FaceRecognizer(const std::string& model_path, Config config);
    ~FaceRecognizer();

    FaceEmbedding Extract(const cv::Mat& aligned_face_bgr);

private:
    FaceRecognizer(const FaceRecognizer&) = delete;
    FaceRecognizer& operator=(const FaceRecognizer&) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace omniface::recognition
