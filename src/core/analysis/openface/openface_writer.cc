#include "omniface/core/analysis/openface/openface_writer.h"

#include <filesystem>
#include <iomanip>
#include <string>
#include <vector>

#include <opencv2/core/mat.hpp>

#include <RecorderOpenFace.h>
#include <RecorderOpenFaceParameters.h>

namespace omniface::analysis::openface {

OpenFaceWriter::OpenFaceWriter() = default;

OpenFaceWriter::~OpenFaceWriter() { Close(); }

void OpenFaceWriter::Initialize(const std::string& input_path,
                                 const std::string& output_dir, double fps,
                                 bool output_aligned) {
    if (initialized_) return;

    std::vector<std::string> rec_args = {
        "",
        "-oc", "MJPG",
        "-2Dfp", "-3Dfp",
        "-pdmparams", "-pose",
        "-aus", "-gaze",
    };
    if (output_aligned)
        rec_args.push_back("-simalign");
    Utilities::RecorderOpenFaceParameters rec_params(
        rec_args, true, false,
        -1, -1, -1, -1, fps);

    recorder_ = std::make_unique<Utilities::RecorderOpenFace>(
        input_path, rec_params, output_dir);

    // Open quality CSV alongside the main CSV
    namespace fs = std::filesystem;
    std::string stem = fs::path(input_path).filename().replace_extension("").string();
    std::string quality_path = (fs::path(output_dir) / (stem + "_quality.csv")).string();
    quality_csv_.open(quality_path, std::ios_base::out);
    if (quality_csv_.is_open()) {
        quality_csv_ << std::fixed << std::setprecision(4);
        quality_csv_ << "frame,quality_score,visible_landmarks\n";
    }

    eigenvalues_written_ = false;
    initialized_ = true;
}

void OpenFaceWriter::WriteObservation(const FaceAnalysisResult& analysis,
                                       int frame_index, double fps,
                                       const cv::Mat& aligned_face) {
    if (!initialized_ || !recorder_) return;

    double ts = (fps > 0.0) ? (frame_index - 1) / fps : 0.0;
    recorder_->SetObservationTimestamp(ts);
    recorder_->SetObservationFrameNumber(frame_index);
    recorder_->SetObservationFaceID(0);

    cv::Mat_<float> lm2d(136, 1,
        const_cast<float*>(analysis.landmarks_2d.data()));
    int n3d = static_cast<int>(analysis.landmarks_3d.size());
    cv::Mat_<float> lm3d(n3d, 1,
        const_cast<float*>(analysis.landmarks_3d.data()));
    cv::Vec6f pdm_global(analysis.pdm_rigid[0], analysis.pdm_rigid[1],
        analysis.pdm_rigid[2], analysis.pdm_rigid[3],
        analysis.pdm_rigid[4], analysis.pdm_rigid[5]);
    int np = static_cast<int>(analysis.pdm_nonrigid.size());
    cv::Mat_<float> pdm_local(np, 1,
        const_cast<float*>(analysis.pdm_nonrigid.data()));
    recorder_->SetObservationLandmarks(
        lm2d, lm3d, pdm_global, pdm_local,
        static_cast<double>(analysis.detection_certainty),
        analysis.detection_success);

    cv::Vec6f pose_vec(analysis.pose_Tx, analysis.pose_Ty, analysis.pose_Tz,
        analysis.head_pose.pitch, analysis.head_pose.yaw, analysis.head_pose.roll);
    recorder_->SetObservationPose(pose_vec);

    recorder_->SetObservationActionUnits(
        analysis.action_units, analysis.au_classifications);

    cv::Point3f g0(analysis.gaze_0_x, analysis.gaze_0_y, analysis.gaze_0_z);
    cv::Point3f g1(analysis.gaze_1_x, analysis.gaze_1_y, analysis.gaze_1_z);
    cv::Vec2f ga(analysis.gaze_angle_x, analysis.gaze_angle_y);
    int ne2 = static_cast<int>(analysis.eye_landmarks_2d.size() / 2);
    std::vector<cv::Point2f> eye_lm2d(ne2);
    for (int i = 0; i < ne2; ++i)
        eye_lm2d[i] = cv::Point2f(
            analysis.eye_landmarks_2d[i],
            analysis.eye_landmarks_2d[i + ne2]);
    int ne3 = static_cast<int>(analysis.eye_landmarks_3d.size() / 3);
    std::vector<cv::Point3f> eye_lm3d(ne3);
    for (int i = 0; i < ne3; ++i)
        eye_lm3d[i] = cv::Point3f(
            analysis.eye_landmarks_3d[i],
            analysis.eye_landmarks_3d[i + ne3],
            analysis.eye_landmarks_3d[i + ne3 * 2]);
    recorder_->SetObservationGaze(g0, g1, ga, eye_lm2d, eye_lm3d);

    if (!aligned_face.empty())
        recorder_->SetObservationFaceAlign(aligned_face);

    recorder_->WriteObservation();

    // Write quality CSV row
    if (quality_csv_.is_open()) {
        if (!eigenvalues_written_ && !analysis.pdm_eigenvalues.empty()) {
            quality_csv_ << "#eigen_values";
            for (float v : analysis.pdm_eigenvalues)
                quality_csv_ << "," << v;
            quality_csv_ << std::endl;
            eigenvalues_written_ = true;
        }
        quality_csv_ << frame_index << ","
                     << analysis.quality_score << ","
                     << analysis.visible_landmarks << std::endl;
    }
}

void OpenFaceWriter::Close() {
    if (recorder_) recorder_->Close();
    if (quality_csv_.is_open()) quality_csv_.close();
    initialized_ = false;
}

}  // namespace omniface::analysis::openface
