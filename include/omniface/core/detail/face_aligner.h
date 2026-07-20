#pragma once

#include <opencv2/calib3d.hpp>
#include <opencv2/core/types.hpp>
#include <vector>

namespace omniface::detail {

inline constexpr int kLeftEyeIndices[16] = {33,  7,   163, 144, 145, 153, 154, 155,
                                            133, 173, 157, 158, 159, 160, 161, 246};
inline constexpr int kRightEyeIndices[16] = {362, 382, 381, 380, 374, 373, 390, 249,
                                             263, 466, 388, 387, 386, 385, 384, 398};

inline cv::Point2f EyeCenter(const std::vector<cv::Point2f>& landmarks, bool left) {
    const int* idx = left ? kLeftEyeIndices : kRightEyeIndices;
    float sx = 0, sy = 0;
    for (int i = 0; i < 16; ++i) {
        sx += landmarks[idx[i]].x;
        sy += landmarks[idx[i]].y;
    }
    return {sx / 16.0f, sy / 16.0f};
}

inline cv::Point2f MidEyeCenter(const std::vector<cv::Point2f>& landmarks) {
    auto le = EyeCenter(landmarks, true);
    auto re = EyeCenter(landmarks, false);
    return {(le.x + re.x) * 0.5f, (le.y + re.y) * 0.5f};
}

inline cv::Mat AlignFaceSimilarity(const std::vector<cv::Point2f>& landmarks, int target_size,
                                   float left_eye_x, float left_eye_y, float right_eye_x,
                                   float right_eye_y) {
    auto le = EyeCenter(landmarks, true);
    auto re = EyeCenter(landmarks, false);

    std::vector<cv::Point2f> src = {le, re};
    std::vector<cv::Point2f> dst = {{left_eye_x * target_size, left_eye_y * target_size},
                                    {right_eye_x * target_size, right_eye_y * target_size}};

    cv::Mat affine = cv::estimateAffinePartial2D(src, dst, cv::noArray(), cv::RANSAC, 3.0);
    return affine;
}

inline cv::Mat ApplyAlignment(const cv::Mat& frame, const cv::Mat& affine_2x3, int target_size) {
    if (affine_2x3.empty()) return {};
    cv::Mat aligned;
    cv::warpAffine(frame, aligned, affine_2x3, cv::Size(target_size, target_size), cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    return aligned;
}

}  // namespace omniface::detail
