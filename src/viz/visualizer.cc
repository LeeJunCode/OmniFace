#include "omniface/viz/visualizer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

#include "omniface/core/analysis/au_analyzer.h"
#include "omniface/core/analysis/pose_estimator.h"
#include "omniface/viz/au_visualizer.h"
#include "omniface/viz/gaze_visualizer.h"
#include "omniface/viz/pose_visualizer.h"

namespace omniface::viz {

void Visualizer::DrawAnnotations(cv::Mat& frame, const std::vector<TrackedFace>& tracked_faces,
                                 const std::vector<MatchResult>& match_results,
                                 int target_track_id) {
    for (size_t i = 0; i < tracked_faces.size(); ++i) {
        const auto& tf = tracked_faces[i];
        bool is_target = (tf.id == target_track_id && target_track_id > 0);

        cv::Scalar color = is_target ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);
        int thickness = is_target ? 3 : 1;
        cv::Point pt1(static_cast<int>(tf.box.x1), static_cast<int>(tf.box.y1));
        cv::Point pt2(static_cast<int>(tf.box.x2), static_cast<int>(tf.box.y2));
        cv::rectangle(frame, pt1, pt2, color, thickness);

        std::string label = "ID:" + std::to_string(tf.id);
        if (i < match_results.size() && match_results[i].valid) {
            float sim = match_results[i].similarity;
            char buf[32];
            std::snprintf(buf, sizeof(buf), " sim:%.2f", sim);
            label += buf;
            if (match_results[i].is_match) label += " MATCH";
        }
        if (is_target) label = "TARGET " + label;

        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        cv::Point label_pos(pt1.x, pt1.y - 5);
        if (label_pos.y - text_size.height < 0) label_pos.y = pt1.y + text_size.height + 5;

        cv::rectangle(frame, cv::Point(label_pos.x, label_pos.y - text_size.height - baseline),
                      cv::Point(label_pos.x + text_size.width, label_pos.y + baseline), color,
                      cv::FILLED);
        cv::putText(frame, label, label_pos, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }
    DrawStatusBar(frame, target_track_id);
}

void Visualizer::DrawStatusBar(cv::Mat& frame, int target_track_id) {
    std::string status = target_track_id > 0 ? "Target Track ID: " + std::to_string(target_track_id)
                                             : "Target: NOT FOUND";
    cv::putText(frame, status, cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
}

namespace {

const cv::Mat& ZeroDist() {
    static const cv::Mat dist = cv::Mat::zeros(4, 1, CV_64F);
    return dist;
}

struct Contour {
    std::vector<int> indices;
    cv::Scalar color;
};

const std::vector<Contour>& GetFaceMeshContours() {
    static const std::vector<Contour> contours = {
        {{10,  338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288, 397, 365, 379, 378, 400, 377,
          152, 148, 176, 149, 150, 136, 172, 58,  132, 93,  234, 127, 162, 21,  54,  103, 67,  109},
         {255, 255, 255}},
        {{33, 7, 163, 144, 145, 153, 154, 155, 133, 173, 157, 158, 159, 160, 161, 246},
         {255, 0, 0}},
        {{362, 382, 381, 380, 374, 373, 390, 249, 263, 466, 388, 387, 386, 385, 384, 398},
         {255, 0, 0}},
        {{46, 53, 52, 65, 55}, {255, 255, 0}},
        {{276, 283, 282, 295, 285}, {255, 255, 0}},
        {{168, 6, 197, 195, 5, 4}, {0, 255, 0}},
        {{4, 1, 19, 94, 2}, {0, 255, 0}},
        {{61,  146, 91,  181, 84,  17, 314, 405, 321, 375,
          291, 409, 270, 269, 267, 0,  37,  39,  40,  185},
         {0, 0, 255}},
    };
    return contours;
}

const std::vector<int>& LeftEyeIndices() {
    static const std::vector<int> idx = {33,  7,   163, 144, 145, 153, 154, 155,
                                         133, 173, 157, 158, 159, 160, 161, 246};
    return idx;
}
const std::vector<int>& RightEyeIndices() {
    static const std::vector<int> idx = {362, 382, 381, 380, 374, 373, 390, 249,
                                         263, 466, 388, 387, 386, 385, 384, 398};
    return idx;
}

}  // namespace

void PoseVisualizer::DrawLandmarks(cv::Mat& image, const std::vector<cv::Point2f>& landmarks) {
    if (image.empty() || landmarks.empty()) return;
    for (const auto& pt : landmarks)
        cv::circle(image, cv::Point(static_cast<int>(pt.x), static_cast<int>(pt.y)), 1,
                   cv::Scalar(0, 255, 0), -1, cv::LINE_AA);
}

void PoseVisualizer::DrawPoseAxes(cv::Mat& image, const analysis::PoseEstimationResult& result,
                                  const cv::Point3f& nose_canonical) {
    if (!result.valid || image.empty()) return;
    float axis_len = static_cast<float>(cv::norm(result.tvec)) * 0.3f;
    std::vector<cv::Point3f> axes_3d = {
        nose_canonical,
        cv::Point3f(nose_canonical.x + axis_len, nose_canonical.y, nose_canonical.z),
        nose_canonical,
        cv::Point3f(nose_canonical.x, nose_canonical.y + axis_len, nose_canonical.z),
        nose_canonical,
        cv::Point3f(nose_canonical.x, nose_canonical.y, nose_canonical.z - axis_len),
    };
    std::vector<cv::Point2f> axes_2d;
    cv::projectPoints(axes_3d, result.rvec, result.tvec, result.camera_matrix, ZeroDist(), axes_2d);
    cv::Point origin(static_cast<int>(axes_2d[0].x), static_cast<int>(axes_2d[0].y));
    cv::line(image, origin,
             cv::Point(static_cast<int>(axes_2d[1].x), static_cast<int>(axes_2d[1].y)),
             cv::Scalar(0, 0, 255), 2);
    cv::line(image, origin,
             cv::Point(static_cast<int>(axes_2d[3].x), static_cast<int>(axes_2d[3].y)),
             cv::Scalar(0, 255, 0), 2);
    cv::line(image, origin,
             cv::Point(static_cast<int>(axes_2d[5].x), static_cast<int>(axes_2d[5].y)),
             cv::Scalar(255, 0, 0), 2);

    char buf[128];
    std::snprintf(buf, sizeof(buf), "P:%.0f Y:%.0f R:%.0f", result.pose.pitch, result.pose.yaw,
                  result.pose.roll);
    cv::putText(image, buf, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0, 255, 255), 2);
}

void PoseVisualizer::DrawProjectedModel(cv::Mat& image,
                                        const analysis::PoseEstimationResult& result,
                                        const std::vector<cv::Point3f>& canonical_model) {
    if (!result.valid || image.empty()) return;
    std::vector<cv::Point2f> projected;
    cv::projectPoints(canonical_model, result.rvec, result.tvec, result.camera_matrix, ZeroDist(),
                      projected);
    for (const auto& contour : GetFaceMeshContours()) {
        for (size_t i = 0; i + 1 < contour.indices.size(); ++i) {
            cv::Point p1(static_cast<int>(projected[contour.indices[i]].x),
                         static_cast<int>(projected[contour.indices[i]].y));
            cv::Point p2(static_cast<int>(projected[contour.indices[i + 1]].x),
                         static_cast<int>(projected[contour.indices[i + 1]].y));
            cv::line(image, p1, p2, contour.color, 1, cv::LINE_AA);
        }
    }
}

void PoseVisualizer::DrawFaceNormal(cv::Mat& image, const analysis::PoseEstimationResult& result,
                                    float face_size, const cv::Point3f& nose_canonical) {
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
    cv::Point3f arrow_end(
        static_cast<float>(nose_cam.at<double>(0) + normal_cam.at<double>(0) * arrow_len),
        static_cast<float>(nose_cam.at<double>(1) + normal_cam.at<double>(1) * arrow_len),
        static_cast<float>(nose_cam.at<double>(2) + normal_cam.at<double>(2) * arrow_len));
    std::vector<cv::Point3f> arrow_3d = {arrow_start, arrow_end};
    static const cv::Mat zero_rvec = cv::Mat::zeros(3, 1, CV_64F);
    static const cv::Mat zero_tvec = cv::Mat::zeros(3, 1, CV_64F);
    std::vector<cv::Point2f> arrow_2d;
    cv::projectPoints(arrow_3d, zero_rvec, zero_tvec, result.camera_matrix, ZeroDist(), arrow_2d);
    cv::arrowedLine(image,
                    cv::Point(static_cast<int>(arrow_2d[0].x), static_cast<int>(arrow_2d[0].y)),
                    cv::Point(static_cast<int>(arrow_2d[1].x), static_cast<int>(arrow_2d[1].y)),
                    cv::Scalar(255, 0, 255), 3, cv::LINE_AA, 0, 0.18);
}

namespace {

cv::Point2f ComputeEyeCenter(const std::vector<cv::Point2f>& landmarks,
                             const std::vector<int>& indices) {
    float sum_x = 0, sum_y = 0;
    for (int idx : indices) {
        sum_x += landmarks[idx].x;
        sum_y += landmarks[idx].y;
    }
    float n = static_cast<float>(indices.size());
    return {sum_x / n, sum_y / n};
}

}  // namespace

void GazeVisualizer::DrawGazeVectors(cv::Mat& image, const std::vector<cv::Point2f>& landmarks,
                                     float pitch_rad, float yaw_rad) {
    if (image.empty() || landmarks.size() < 468) return;
    cv::Point2f left_center = ComputeEyeCenter(landmarks, LeftEyeIndices());
    cv::Point2f right_center = ComputeEyeCenter(landmarks, RightEyeIndices());
    float arrow_len = cv::norm(left_center - right_center) * 0.8f;
    float dx = -arrow_len * std::sin(yaw_rad) * std::cos(pitch_rad);
    float dy = -arrow_len * std::sin(pitch_rad);
    const cv::Scalar kGazeColor(0, 0, 255);

    for (auto center : {left_center, right_center}) {
        cv::Point pt(static_cast<int>(center.x), static_cast<int>(center.y));
        cv::Point end(static_cast<int>(center.x + dx), static_cast<int>(center.y + dy));
        cv::circle(image, pt, 3, kGazeColor, -1, cv::LINE_AA);
        cv::arrowedLine(image, pt, end, kGazeColor, 2, cv::LINE_AA, 0, 0.2);
    }
}

void GazeVisualizer::DrawGazeArrow(cv::Mat& image, float x1, float y1, float x2, float y2,
                                   float pitch_rad, float yaw_rad) {
    if (image.empty()) return;

    float cx = (x1 + x2) * 0.5f;
    float cy = (y1 + y2) * 0.5f;

    float arrow_len = (x2 - x1) * 0.6f;

    float dx = -arrow_len * std::sin(yaw_rad) * std::cos(pitch_rad);
    float dy = -arrow_len * std::sin(pitch_rad);

    const cv::Scalar kGazeColor(0, 0, 255);
    cv::Point pt(static_cast<int>(cx), static_cast<int>(cy));
    cv::Point end(static_cast<int>(cx + dx), static_cast<int>(cy + dy));

    cv::circle(image, pt, 4, kGazeColor, -1, cv::LINE_AA);
    cv::arrowedLine(image, pt, end, kGazeColor, 2, cv::LINE_AA, 0, 0.25);
}

void AuVisualizer::DrawAuText(cv::Mat& image, const analysis::AuAnalysisResult& result,
                              const cv::Point& position, int& label_y) {
    if (image.empty() || !result.valid) return;
    std::string au_text = "AU: ";
    int active_count = 0;
    auto append_aus = [&](const auto& aus, const auto& names, int count) {
        for (int i = 0; i < count; ++i) {
            if (aus[i].active) {
                if (active_count > 0) au_text += ",";
                au_text += names[i];
                active_count++;
            }
        }
    };
    append_aus(result.main_aus, analysis::kMainAuNames, 27);
    append_aus(result.sub_aus, analysis::kSubAuNames, 14);
    if (active_count > 0) {
        cv::putText(image, au_text, position, cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(255, 0, 255), 1, cv::LINE_AA);
        label_y += 20;
    }
}

}  // namespace omniface::viz
