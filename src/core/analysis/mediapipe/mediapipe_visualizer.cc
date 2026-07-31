#include "omniface/core/analysis/mediapipe/mediapipe_visualizer.h"

#include <cstdio>

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include "omniface/core/analysis/mediapipe/mediapipe_pose_estimator.h"

namespace omniface::analysis::mediapipe {

namespace {

const cv::Mat& ZeroDist() {
    static const cv::Mat dist = cv::Mat::zeros(4, 1, CV_64F);
    return dist;
}

struct Contour {
    const char* name;
    std::vector<int> indices;
    cv::Scalar color;
};

const std::vector<Contour>& GetFaceMeshContours() {
    static const std::vector<Contour> contours = {
        {"face_oval",
         {10,  338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288,
          397, 365, 379, 378, 400, 377, 152, 148, 176, 149, 150, 136,
          172, 58,  132, 93,  234, 127, 162, 21,  54,  103, 67,  109},
         {255, 255, 255}},
        {"left_eye",
         {33, 7, 163, 144, 145, 153, 154, 155, 133, 173, 157, 158, 159, 160,
          161, 246},
         {255, 0, 0}},
        {"right_eye",
         {362, 382, 381, 380, 374, 373, 390, 249, 263, 466, 388, 387, 386, 385,
          384, 398},
         {255, 0, 0}},
        {"left_eyebrow", {46, 53, 52, 65, 55}, {255, 255, 0}},
        {"right_eyebrow", {276, 283, 282, 295, 285}, {255, 255, 0}},
        {"nose_bridge", {168, 6, 197, 195, 5, 4}, {0, 255, 0}},
        {"nose_bottom", {4, 1, 19, 94, 2}, {0, 255, 0}},
        {"lips_outer",
         {61, 146, 91, 181, 84, 17, 314, 405, 321, 375, 291, 409, 270, 269, 267,
          0, 37, 39, 40, 185},
         {0, 0, 255}},
    };
    return contours;
}

void DrawPoseText(cv::Mat& image, const HeadPose& pose, const cv::Point& offset) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "P:%.0f Y:%.0f R:%.0f",
                  pose.pitch, pose.yaw, pose.roll);
    cv::putText(image, buf, offset,
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);
}

}  // namespace

void MediaPipeVisualizer::DrawLandmarks(cv::Mat& image,
                                    const std::vector<cv::Point2f>& landmarks) {
    if (image.empty() || landmarks.empty()) return;

    for (const auto& pt : landmarks) {
        cv::circle(image,
                   cv::Point(static_cast<int>(pt.x), static_cast<int>(pt.y)),
                   1,
                   cv::Scalar(0, 255, 0),
                   -1,
                   cv::LINE_AA);
    }
}

void MediaPipeVisualizer::DrawProjectedModel(cv::Mat& image,
                                         const PoseEstimationResult& result,
                                         const std::vector<cv::Point3f>& canonical_model) {
    if (!result.valid || image.empty()) return;

    const cv::Mat& dist = ZeroDist();
    std::vector<cv::Point2f> projected;
    cv::projectPoints(canonical_model, result.rvec,
                      result.tvec, result.camera_matrix, dist, projected);

    const auto& contours = GetFaceMeshContours();
    for (const auto& contour : contours) {
        const auto& idx = contour.indices;
        for (size_t i = 0; i + 1 < idx.size(); ++i) {
            cv::Point p1(static_cast<int>(projected[idx[i]].x),
                         static_cast<int>(projected[idx[i]].y));
            cv::Point p2(static_cast<int>(projected[idx[i + 1]].x),
                         static_cast<int>(projected[idx[i + 1]].y));
            cv::line(image, p1, p2, contour.color, 1, cv::LINE_AA);
        }
    }
}

void MediaPipeVisualizer::DrawFaceNormal(cv::Mat& image,
                                     const PoseEstimationResult& result,
                                     float face_size,
                                     const cv::Point3f& nose_canonical) {
    if (!result.valid || image.empty()) return;

    cv::Mat R_pnp;
    cv::Rodrigues(result.rvec, R_pnp);

    cv::Mat nose_canonical_mat =
        (cv::Mat_<double>(3, 1) << nose_canonical.x, nose_canonical.y, nose_canonical.z);
    cv::Mat nose_cam = R_pnp * nose_canonical_mat + result.tvec;

    cv::Mat canonical_forward = (cv::Mat_<double>(3, 1) << 0.0, 0.0, 1.0);
    cv::Mat normal_cam = R_pnp * canonical_forward;

    float arrow_len = face_size * 0.8f;

    cv::Point3f arrow_start(static_cast<float>(nose_cam.at<double>(0)),
                            static_cast<float>(nose_cam.at<double>(1)),
                            static_cast<float>(nose_cam.at<double>(2)));
    cv::Point3f arrow_end(static_cast<float>(nose_cam.at<double>(0) +
                                             normal_cam.at<double>(0) * arrow_len),
                          static_cast<float>(nose_cam.at<double>(1) +
                                             normal_cam.at<double>(1) * arrow_len),
                          static_cast<float>(nose_cam.at<double>(2) +
                                             normal_cam.at<double>(2) * arrow_len));

    std::vector<cv::Point3f> arrow_3d = {arrow_start, arrow_end};
    const cv::Mat& dist = ZeroDist();
    static const cv::Mat zero_rvec = cv::Mat::zeros(3, 1, CV_64F);
    static const cv::Mat zero_tvec = cv::Mat::zeros(3, 1, CV_64F);
    std::vector<cv::Point2f> arrow_2d;
    cv::projectPoints(arrow_3d, zero_rvec, zero_tvec, result.camera_matrix,
                      dist, arrow_2d);

    cv::Point start(static_cast<int>(arrow_2d[0].x), static_cast<int>(arrow_2d[0].y));
    cv::Point end(static_cast<int>(arrow_2d[1].x), static_cast<int>(arrow_2d[1].y));

    cv::arrowedLine(image, start, end, cv::Scalar(255, 0, 255), 3, cv::LINE_AA, 0, 0.18);
}

}  // namespace omniface::analysis::mediapipe
