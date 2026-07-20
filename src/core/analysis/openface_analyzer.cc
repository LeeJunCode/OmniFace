#include "omniface/core/analysis/openface_analyzer.h"

#include <filesystem>
#include <fstream>

#include "FaceAnalyser.h"
#include "Face_utils.h"
#include "GazeEstimation.h"
#include "LandmarkCoreIncludes.h"
#include "LandmarkDetectorFunc.h"
#include "RecorderOpenFace.h"
#include "RecorderOpenFaceParameters.h"
#include "VisualizationUtils.h"
#include "Visualizer.h"

namespace omniface::analysis {

class OpenFaceAnalyzer::Impl {
public:
    explicit Impl(const std::string& model_dir);

    void StartVideoSequence(const std::string& output_name, const std::string& output_dir,
                            double fx, double fy, double cx, double cy, double fps);
    void ProcessVideoFrame(const cv::Mat& rgb, double timestamp);
    void ProcessVideoFrameWithBBox(const cv::Mat& rgb, double timestamp,
                                   const cv::Rect_<float>& bbox);
    void EndVideoSequence();

    cv::Mat GetVisImage() const;
    void SetVisFlags(bool track_pose_gaze, bool aus, bool hog, bool align);
    void SetFacePreference(double norm_x, double norm_y);
    bool quit_requested = false;
    OpenFaceAnalyzer::FrameResult last_result_;

private:
    void CreateRecorder(const std::string& output_name, const std::string& output_dir);
    void ProcessFrameCommon(const cv::Mat& rgb, double timestamp);
    void WriteFeatureRecord(LandmarkDetector::CLNF& model, FaceAnalysis::FaceAnalyser& analyser,
                            double timestamp, int frame_number);
    void SetVisualizerObservations(LandmarkDetector::CLNF& model,
                                   FaceAnalysis::FaceAnalyser* analyser);
    void EnsureRecorder(double fx, double fy, double cx, double cy);

    LandmarkDetector::FaceModelParameters det_params_;
    LandmarkDetector::CLNF face_model_;
    std::unique_ptr<FaceAnalysis::FaceAnalyser> face_analyser_;

    std::unique_ptr<Utilities::RecorderOpenFace> recorder_;
    std::unique_ptr<Utilities::RecorderOpenFaceParameters> rec_params_;
    std::unique_ptr<Utilities::Visualizer> visualizer_;
    std::unique_ptr<Utilities::FpsTracker> fps_tracker_;

    float fx_ = 0, fy_ = 0, cx_ = 0, cy_ = 0;
    double fps_ = 30;
    int frame_num_ = 0;
    std::string seq_output_name_, seq_output_dir_;
};

OpenFaceAnalyzer::Impl::Impl(const std::string& model_dir) {
    std::string exe_path = model_dir + "FaceLandmarkImg";
    std::vector<std::string> args = {exe_path, "-f", "dummy.jpg"};
    det_params_ = LandmarkDetector::FaceModelParameters(args);

    face_model_ = LandmarkDetector::CLNF(det_params_.model_location);
    if (!face_model_.loaded_successfully)
        throw std::runtime_error("Failed to load CLNF model: " + det_params_.model_location);
    if (!face_model_.eye_model) {}
    std::vector<std::string> au_args = {exe_path};
    FaceAnalysis::FaceAnalyserParameters au_params(au_args);
    face_analyser_ = std::make_unique<FaceAnalysis::FaceAnalyser>(au_params);

    if (face_analyser_->GetAUClassNames().empty() && face_analyser_->GetAURegNames().empty()) {}

    fps_tracker_ = std::make_unique<Utilities::FpsTracker>();
    fps_tracker_->AddFrame();
}

void OpenFaceAnalyzer::Impl::StartVideoSequence(const std::string& output_name,
                                                const std::string& output_dir, double fx, double fy,
                                                double cx, double cy, double fps) {
    fx_ = static_cast<float>(fx);
    fy_ = static_cast<float>(fy);
    cx_ = static_cast<float>(cx);
    cy_ = static_cast<float>(cy);
    fps_ = fps;
    frame_num_ = 0;
    quit_requested = false;
    seq_output_name_ = output_name;
    seq_output_dir_ = output_dir;

    recorder_.reset();
    rec_params_.reset();
    face_model_.Reset();
    face_analyser_->Reset();

    visualizer_ = std::make_unique<Utilities::Visualizer>(std::vector<std::string>{});
    visualizer_->vis_track = true;
    visualizer_->vis_aus = true;

    fps_tracker_ = std::make_unique<Utilities::FpsTracker>();
    fps_tracker_->AddFrame();
}

void OpenFaceAnalyzer::Impl::EnsureRecorder(double fx, double fy, double cx, double cy) {
    if (fx_ <= 0) fx_ = static_cast<float>(fx);
    if (fy_ <= 0) fy_ = static_cast<float>(fy);
    if (cx_ <= 0) cx_ = static_cast<float>(cx);
    if (cy_ <= 0) cy_ = static_cast<float>(cy);
    if (!recorder_) CreateRecorder(seq_output_name_, seq_output_dir_);
}

void OpenFaceAnalyzer::Impl::ProcessFrameCommon(const cv::Mat& rgb, double timestamp) {
    if (frame_num_ == 0) {
        double f = 500.0 * (std::max(rgb.cols, rgb.rows) / 640.0);
        EnsureRecorder(f, f, rgb.cols / 2.0, rgb.rows / 2.0);
    }
    visualizer_->SetImage(rgb, fx_, fy_, cx_, cy_);
    cv::Mat gray;
    cv::cvtColor(rgb, gray, cv::COLOR_BGR2GRAY);
    face_analyser_->AddNextFrame(rgb, face_model_.detected_landmarks, face_model_.detection_success,
                                 timestamp, false);
    fps_tracker_->AddFrame();
    SetVisualizerObservations(face_model_, face_analyser_.get());
    visualizer_->SetFps(fps_tracker_->GetFPS());
    if (visualizer_->ShowObservation() == 'q') quit_requested = true;
    WriteFeatureRecord(face_model_, *face_analyser_, timestamp, frame_num_);
    recorder_->SetObservationVisualization(visualizer_->GetVisImage());
    recorder_->WriteObservationTracked();
    frame_num_++;
}

void OpenFaceAnalyzer::Impl::ProcessVideoFrame(const cv::Mat& rgb, double timestamp) {
    cv::Mat gray;
    cv::cvtColor(rgb, gray, cv::COLOR_BGR2GRAY);
    LandmarkDetector::DetectLandmarksInVideo(rgb, face_model_, det_params_, gray);
    ProcessFrameCommon(rgb, timestamp);
}

void OpenFaceAnalyzer::Impl::ProcessVideoFrameWithBBox(const cv::Mat& rgb, double timestamp,
                                                       const cv::Rect_<float>& bbox) {
    cv::Mat gray;
    cv::cvtColor(rgb, gray, cv::COLOR_BGR2GRAY);
    LandmarkDetector::DetectLandmarksInVideo(rgb, bbox, face_model_, det_params_, gray);
    ProcessFrameCommon(rgb, timestamp);
}

void OpenFaceAnalyzer::Impl::EndVideoSequence() {
    std::string csv_path;
    if (recorder_ && face_analyser_) csv_path = recorder_->GetCSVFile();

    if (recorder_) {
        recorder_->Close();
        recorder_.reset();
    }
    if (!csv_path.empty()) face_analyser_->PostprocessOutputFile(csv_path);

    face_model_.Reset();
    face_analyser_->Reset();
    visualizer_.reset();
    frame_num_ = 0;
}

cv::Mat OpenFaceAnalyzer::Impl::GetVisImage() const {
    return visualizer_ ? visualizer_->GetVisImage() : cv::Mat{};
}

void OpenFaceAnalyzer::Impl::SetVisFlags(bool track_pose_gaze, bool aus, bool hog, bool align) {
    if (!visualizer_) return;
    visualizer_->vis_track = track_pose_gaze;
    visualizer_->vis_aus = aus;
    visualizer_->vis_hog = hog;
    visualizer_->vis_align = align;
}

void OpenFaceAnalyzer::Impl::SetFacePreference(double norm_x, double norm_y) {
    face_model_.preference_det = cv::Point_<double>(norm_x, norm_y);
}

void OpenFaceAnalyzer::Impl::CreateRecorder(const std::string& output_name,
                                            const std::string& output_dir) {
    std::filesystem::create_directories(output_dir);
    std::string path = output_dir + output_name;
    { std::ofstream tmp(path); }

    rec_params_ = std::make_unique<Utilities::RecorderOpenFaceParameters>(
        true, false, true, true, true, true, true, true, true, true, true, true, fx_, fy_, cx_, cy_,
        fps_);

    if (!face_model_.eye_model) rec_params_->setOutputGaze(false);
    recorder_ = std::make_unique<Utilities::RecorderOpenFace>(path, *rec_params_, output_dir);
    std::filesystem::remove(path);
}

void OpenFaceAnalyzer::Impl::WriteFeatureRecord(LandmarkDetector::CLNF& model,
                                                FaceAnalysis::FaceAnalyser& analyser,
                                                double timestamp, int frame_number) {
    bool success = model.detection_success;
    cv::Vec6d pose = LandmarkDetector::GetPose(model, fx_, fy_, cx_, cy_);

    cv::Point3f gaze0(0, 0, -1), gaze1(0, 0, -1);
    cv::Vec2f gaze_angle(0, 0);
    if (success && model.eye_model) {
        GazeAnalysis::EstimateGaze(model, gaze0, fx_, fy_, cx_, cy_, true);
        GazeAnalysis::EstimateGaze(model, gaze1, fx_, fy_, cx_, cy_, false);
        gaze_angle = GazeAnalysis::GetGazeAngle(gaze0, gaze1);
    }

    cv::Mat aligned;
    cv::Mat_<double> hog_desc;
    int hog_rows = 0, hog_cols = 0;
    analyser.GetLatestAlignedFace(aligned);
    analyser.GetLatestHOG(hog_desc, hog_rows, hog_cols);

    last_result_ = OpenFaceAnalyzer::FrameResult{};
    last_result_.valid = success;
    if (success) {
        constexpr float kRad2Deg = 57.29577951f;
        last_result_.pitch = static_cast<float>(pose[3]) * kRad2Deg;
        last_result_.yaw = static_cast<float>(pose[4]) * kRad2Deg;
        last_result_.roll = static_cast<float>(pose[5]) * kRad2Deg;
        last_result_.gaze_yaw = gaze_angle[0];
        last_result_.gaze_pitch = gaze_angle[1];
        for (const auto& [name, value] : analyser.GetCurrentAUsReg()) {
            last_result_.aus.emplace_back(name, static_cast<float>(value));
        }
    }

    recorder_->SetObservationHOG(success, hog_desc, hog_rows, hog_cols, 31);
    recorder_->SetObservationActionUnits(analyser.GetCurrentAUsReg(),
                                         analyser.GetCurrentAUsClass());
    recorder_->SetObservationLandmarks(model.detected_landmarks, model.GetShape(fx_, fy_, cx_, cy_),
                                       model.params_global, model.params_local,
                                       model.detection_certainty, success);
    recorder_->SetObservationPose(pose);
    recorder_->SetObservationGaze(
        gaze0, gaze1, gaze_angle, LandmarkDetector::CalculateAllEyeLandmarks(model),
        LandmarkDetector::Calculate3DEyeLandmarks(model, fx_, fy_, cx_, cy_));
    recorder_->SetObservationTimestamp(timestamp);
    recorder_->SetObservationFaceID(0);
    recorder_->SetObservationFrameNumber(frame_number);
    recorder_->SetObservationFaceAlign(aligned);
    recorder_->WriteObservation();
}

void OpenFaceAnalyzer::Impl::SetVisualizerObservations(LandmarkDetector::CLNF& model,
                                                       FaceAnalysis::FaceAnalyser* analyser) {
    cv::Mat aligned;
    cv::Mat_<double> hog_desc;
    int hog_rows = 0, hog_cols = 0;
    if (analyser) {
        analyser->GetLatestAlignedFace(aligned);
        analyser->GetLatestHOG(hog_desc, hog_rows, hog_cols);
    }

    cv::Vec6d pose = LandmarkDetector::GetPose(model, fx_, fy_, cx_, cy_);
    cv::Point3f gaze0(0, 0, -1), gaze1(0, 0, -1);
    if (model.detection_success && model.eye_model) {
        GazeAnalysis::EstimateGaze(model, gaze0, fx_, fy_, cx_, cy_, true);
        GazeAnalysis::EstimateGaze(model, gaze1, fx_, fy_, cx_, cy_, false);
    }

    visualizer_->SetObservationFaceAlign(aligned);
    visualizer_->SetObservationHOG(hog_desc, hog_rows, hog_cols);
    visualizer_->SetObservationLandmarks(model.detected_landmarks, model.detection_certainty,
                                         model.GetVisibilities());
    visualizer_->SetObservationPose(pose, model.detection_certainty);
    visualizer_->SetObservationGaze(
        gaze0, gaze1, LandmarkDetector::CalculateAllEyeLandmarks(model),
        LandmarkDetector::Calculate3DEyeLandmarks(model, fx_, fy_, cx_, cy_),
        model.detection_certainty);
    if (analyser)
        visualizer_->SetObservationActionUnits(analyser->GetCurrentAUsReg(),
                                               analyser->GetCurrentAUsClass());
}

OpenFaceAnalyzer::OpenFaceAnalyzer(const std::string& model_dir)
    : impl_(std::make_unique<Impl>(model_dir)) {}

OpenFaceAnalyzer::~OpenFaceAnalyzer() = default;

void OpenFaceAnalyzer::StartSequence(const std::string& output_name, const std::string& output_dir,
                                     double fx, double fy, double cx, double cy, double fps) {
    impl_->StartVideoSequence(output_name, output_dir, fx, fy, cx, cy, fps);
}

void OpenFaceAnalyzer::AnalyzeVideoFrame(const cv::Mat& bgr, double timestamp) {
    impl_->ProcessVideoFrame(bgr, timestamp);
}

void OpenFaceAnalyzer::AnalyzeVideoFrame(const cv::Mat& bgr, double timestamp,
                                         const cv::Rect_<float>& target_bbox) {
    impl_->ProcessVideoFrameWithBBox(bgr, timestamp, target_bbox);
}

void OpenFaceAnalyzer::EndSequence() {
    impl_->EndVideoSequence();
}
bool OpenFaceAnalyzer::IsQuitRequested() const {
    return impl_->quit_requested;
}
cv::Mat OpenFaceAnalyzer::GetVisualization() const {
    return impl_->GetVisImage();
}

const OpenFaceAnalyzer::FrameResult& OpenFaceAnalyzer::LastResult() const {
    return impl_->last_result_;
}

void OpenFaceAnalyzer::SetVisualization(bool track_pose_gaze, bool aus, bool hog, bool align) {
    impl_->SetVisFlags(track_pose_gaze, aus, hog, align);
}

void OpenFaceAnalyzer::SetFacePreference(double norm_x, double norm_y) {
    impl_->SetFacePreference(norm_x, norm_y);
}

}  // namespace omniface::analysis
