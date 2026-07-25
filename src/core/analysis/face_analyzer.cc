#include "omniface/core/analysis/face_analyzer.h"

#include <filesystem>

#include <opencv2/imgproc.hpp>

#include <FaceAnalyser.h>
#include <FaceAnalyserParameters.h>
#include <GazeEstimation.h>
#include <LandmarkDetectorFunc.h>
#include <LandmarkDetectorModel.h>
#include <LandmarkDetectorParameters.h>
#include <LandmarkDetectorUtils.h>
#include <Visualizer.h>

#include <spdlog/spdlog.h>

namespace omniface::analysis {

namespace {

constexpr float kDegPerRad = 180.0f / 3.14159265358979323846f;

void ApproxIntrinsics(int cols, int rows, float& fx, float& fy,
                      float& cx, float& cy) {
    fx = fy = static_cast<float>(cols);
    cx = static_cast<float>(cols) / 2.0f;
    cy = static_cast<float>(rows) / 2.0f;
}

}  // namespace

class FaceAnalyzer::Impl {
public:
    explicit Impl(const Config& cfg)
        : visualizer_(true, false, false, true)  // track, no HOG, no align, AUs
    {
        // Force display all detections regardless of confidence value
        visualizer_.visualisation_boundary = -2.0;
        namespace fs = std::filesystem;
        fs::path model_path = fs::absolute(cfg.model_dir).lexically_normal();

        if (!fs::is_directory(model_path)) {
            spdlog::error("OpenFace model directory not found: {}", model_path.string());
            return;
        }

        // CLNF expects a full file path, not a directory
        fs::path model_file = model_path / "main_clnf_general.txt";
        det_params_.model_location = model_file.string();
        det_params_.reinit_video_every = -1;
        det_params_.validate_detections = false;
        det_params_.refine_hierarchical = true;

        // Enable template-based global motion correction between frames.
        // Without this, the tracker loses the face on even moderate motion
        // because window_sizes_small has no coarse-level search.
        det_params_.use_face_template = true;

        // Default window_sizes_small = {0, 9, 7, 0} skips the coarsest
        // scale entirely. Give scale 0 a reasonable search radius so the
        // tracker can handle larger inter-frame displacements.
        det_params_.window_sizes_small = {9, 9, 7, 5};

        spdlog::info("Loading CLNF model from: {}", det_params_.model_location);
        clnf_ = std::make_unique<LandmarkDetector::CLNF>(
            det_params_.model_location);
        if (!clnf_->loaded_successfully) {
            spdlog::error("CLNF model failed to load from: {}",
                          det_params_.model_location);
            clnf_.reset();
            return;
        }
        spdlog::info("CLNF model loaded successfully");

        FaceAnalysis::FaceAnalyserParameters fa_params(
            model_path.parent_path().string());
        fa_params.OptimizeForVideos();
        face_analyser_ = std::make_unique<FaceAnalysis::FaceAnalyser>(fa_params);
        spdlog::info("FaceAnalyser (AU) initialized");
    }

    FaceAnalysisResult Analyze(cv::Mat& frame, const cv::Rect_<float>& bbox) {
        FaceAnalysisResult result;

        if (!clnf_) return result;
        if (frame.empty()) return result;

        float fx, fy, cx, cy;
        ApproxIntrinsics(frame.cols, frame.rows, fx, fy, cx, cy);

        // Set up OpenFace's own visualizer on a copy of the frame
        cv::Mat canvas = frame.clone();
        visualizer_.SetImage(canvas, fx, fy, cx, cy);

        cv::Mat gray;
        if (frame.channels() == 3)
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else
            gray = frame;

        // Apply MTCNN-style correction: CLNF's PDM::CalcParams expects a
        // tight "chin to eyebrows" bounding box. Raw SCRFD bbox includes
        // forehead and below-chin, so we shift down and compress vertically.
        cv::Rect_<double> bbox_d(
            bbox.x + bbox.width * -0.0075,
            bbox.y + bbox.height * 0.2459,
            bbox.width * 1.0323,
            bbox.height * 0.7751);

        bool detected;
        bool needs_init = !clnf_->tracking_initialised
                       || !clnf_->detection_success;

        if (needs_init) {
            // Initialize CLNF from bbox: set zero local params (neutral
            // expression) and compute global params (scale, pose, translation)
            // that map the 3D mean face into the image bounding box.
            clnf_->params_local.setTo(0);
            clnf_->pdm.CalcParams(clnf_->params_global,
                cv::Rect_<float>((float)bbox_d.x, (float)bbox_d.y,
                                 (float)bbox_d.width, (float)bbox_d.height),
                clnf_->params_local);

            // Multi-view detection: try 11 rotation hypotheses and pick the
            // best fit. This is the same path OpenFace uses internally when
            // its own face detector finds a face. Much more accurate than
            // single-hypothesis tracking initialization.
            det_params_.multi_view = true;
            detected = LandmarkDetector::DetectLandmarksInImage(
                frame, bbox_d, *clnf_, det_params_, gray);
            det_params_.multi_view = false;

            if (detected) {
                clnf_->tracking_initialised = true;
                clnf_->failures_in_a_row = -1;
            }
        } else {
            // Frame-to-frame tracking: starts from previous frame's converged
            // state (params_global, params_local). Uses small search windows
            // for speed since inter-frame motion is small.
            detected = LandmarkDetector::DetectLandmarksInVideo(
                frame, *clnf_, det_params_, gray);
        }

        if (!detected) {
            clnf_->Reset();
            face_analyser_->Reset();
            return result;
        }

        // detection_certainty: -1=perfect, 1=worst. Set a good value since
        // we skipped CNN validation for speed.
        clnf_->detection_certainty = -0.9;

        result.valid = true;

        // --- Feed OpenFace Visualizer ---
        visualizer_.SetObservationLandmarks(
            clnf_->detected_landmarks, clnf_->detection_certainty,
            clnf_->GetVisibilities());

        // --- Head pose ---
        cv::Vec6f pose = LandmarkDetector::GetPose(*clnf_, fx, fy, cx, cy);
        result.head_pose.pitch = pose[3] * kDegPerRad;
        result.head_pose.yaw   = pose[4] * kDegPerRad;
        result.head_pose.roll  = pose[5] * kDegPerRad;
        visualizer_.SetObservationPose(pose, clnf_->detection_certainty);

        // --- Gaze ---
        if (clnf_->eye_model) {
            cv::Point3f gaze_left, gaze_right;
            GazeAnalysis::EstimateGaze(*clnf_, gaze_left, fx, fy, cx, cy, true);
            GazeAnalysis::EstimateGaze(*clnf_, gaze_right, fx, fy, cx, cy, false);
            cv::Vec2f gaze_angle = GazeAnalysis::GetGazeAngle(gaze_left, gaze_right);
            result.gaze_angle_x = gaze_angle[0];
            result.gaze_angle_y = gaze_angle[1];

            auto eye_lm2d = LandmarkDetector::CalculateAllEyeLandmarks(*clnf_);
            auto eye_lm3d = LandmarkDetector::Calculate3DEyeLandmarks(
                *clnf_, fx, fy, cx, cy);
            visualizer_.SetObservationGaze(
                gaze_left, gaze_right, eye_lm2d, eye_lm3d,
                clnf_->detection_certainty);
        }

        // --- Action units ---
        face_analyser_->AddNextFrame(frame, clnf_->detected_landmarks,
                                     clnf_->detection_success, 0.0, false);
        auto au_reg = face_analyser_->GetCurrentAUsReg();
        auto au_class = face_analyser_->GetCurrentAUsClass();
        result.action_units = au_reg;
        visualizer_.SetObservationActionUnits(au_reg, au_class);

        // --- 68-point landmarks ---
        if (clnf_->detected_landmarks.rows >= 136) {
            for (int i = 0; i < 136; ++i)
                result.landmarks_2d[i] = clnf_->detected_landmarks.at<float>(i);
        }

        // Composite OpenFace visualization back onto the frame
        frame = visualizer_.GetVisImage().clone();

        return result;
    }

    void Reset() {
        if (clnf_) clnf_->Reset();
        if (face_analyser_) face_analyser_->Reset();
    }

private:
    LandmarkDetector::FaceModelParameters det_params_;
    std::unique_ptr<LandmarkDetector::CLNF> clnf_;
    std::unique_ptr<FaceAnalysis::FaceAnalyser> face_analyser_;
    Utilities::Visualizer visualizer_;
};

FaceAnalyzer::FaceAnalyzer(const Config& cfg)
    : impl_(std::make_unique<Impl>(cfg)) {}

FaceAnalyzer::~FaceAnalyzer() = default;

FaceAnalysisResult FaceAnalyzer::Analyze(cv::Mat& frame,
                                         const cv::Rect_<float>& bbox) {
    return impl_->Analyze(frame, bbox);
}

void FaceAnalyzer::Reset() { impl_->Reset(); }

}  // namespace omniface::analysis
