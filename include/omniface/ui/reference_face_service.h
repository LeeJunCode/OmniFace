#pragma once

#include <string>

#include <opencv2/core/mat.hpp>

#include "omniface/pipeline/config.h"
#include "omniface/types.h"

namespace omniface::ui {

FaceEmbedding ExtractReferenceFace(const std::string& image_path,
                                   const PipelineConfig& config,
                                   bool& out_success);

cv::Mat DrawDetectionsOnImage(const cv::Mat& image, const std::string& model_path,
                              const PipelineConfig& config);

}  // namespace omniface::ui
