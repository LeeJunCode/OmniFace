#pragma once

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include "omniface/types.h"

namespace omniface::recognition {

inline cv::Mat AlignFace(const cv::Mat& frame, const BoundingBox& box, int target_size = 112) {
    const float scale = static_cast<float>(target_size) / 112.0f;
    std::vector<cv::Point2f> dst_pts = {
        {38.3f * scale, 51.7f * scale}, {73.5f * scale, 51.5f * scale},
        {56.0f * scale, 71.7f * scale}, {41.5f * scale, 92.4f * scale},
        {70.7f * scale, 92.2f * scale},
    };

    std::vector<cv::Point2f> src_pts(5);
    for (int i = 0; i < 5; ++i)
        src_pts[i] = cv::Point2f(box.keypoints[i * 2], box.keypoints[i * 2 + 1]);

    cv::Mat affine = cv::estimateAffinePartial2D(src_pts, dst_pts, cv::noArray(), cv::RANSAC);
    cv::Mat aligned;
    if (affine.empty()) {
        int x = std::max(0, static_cast<int>(box.x1));
        int y = std::max(0, static_cast<int>(box.y1));
        int w = std::min(frame.cols - x, static_cast<int>(box.x2 - box.x1));
        int h = std::min(frame.rows - y, static_cast<int>(box.y2 - box.y1));
        if (w <= 0 || h <= 0) return {};
        cv::resize(frame(cv::Rect(x, y, w, h)), aligned, cv::Size(target_size, target_size), 0, 0,
                   cv::INTER_LINEAR);
    } else {
        cv::warpAffine(frame, aligned, affine, cv::Size(target_size, target_size), cv::INTER_LINEAR,
                       cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    }
    return aligned;
}

}  // namespace omniface::recognition
