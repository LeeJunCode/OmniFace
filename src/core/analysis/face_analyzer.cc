#include "omniface/core/analysis/face_analyzer.h"

#include <algorithm>
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
        : cfg_(cfg),
          visualizer_(true, false, false, true)  // track, no HOG, no align, AUs
    {
        visualizer_.visualisation_boundary = -2.0;
        namespace fs = std::filesystem;
        fs::path model_path = fs::absolute(cfg.model_dir).lexically_normal();

        if (!fs::is_directory(model_path)) {
            spdlog::error("OpenFace model directory not found: {}", model_path.string());
            return;
        }

        // Use CECLM model (CEN patch experts) — the official default, most accurate.
        fs::path model_file = model_path / "main_ceclm_general.txt";
        det_params_.model_location = model_file.string();

        // Path for MTCNN face detector (used internally by DetectLandmarksInVideo
        // for reinitialization when tracking fails).
        det_params_.mtcnn_face_detector_location =
            (model_path / "mtcnn_detector" / "MTCNN_detector.txt").string();

        // Official defaults: validate landmarks with CNN SVR validator.
        det_params_.validate_detections = true;
        det_params_.validation_boundary = 0.725f;

        // Standard reinitialization scheduling: attempt re-detection every 2
        // consecutive tracking failures (same as official FaceLandmarkVid).
        det_params_.reinit_video_every = 2;

        // Window sizes: official defaults.
        // During tracking, skip the coarsest and finest scales for speed.
        det_params_.window_sizes_small = {0, 9, 7, 0};
        // During (re)initialization, search all 4 scales with large windows.
        det_params_.window_sizes_init = {11, 9, 7, 5};

        // CLNF/CECLM fitting parameters — official defaults.
        // CECLM overrides: sigma *= 1.5, reg_factor *= 0.9 internally.
        det_params_.sigma = 1.5f;
        det_params_.reg_factor = 25.0f;
        det_params_.weight_factor = 0.0f;
        det_params_.num_optimisation_iteration = 5;

        // Enable template-based global motion correction between frames.
        det_params_.use_face_template = true;
        det_params_.face_template_scale = 0.3f;

        // Use hierarchical part models (eye models for gaze, inner face).
        det_params_.refine_hierarchical = true;
        det_params_.refine_parameters = true;

        spdlog::info("Loading CECLM model from: {}", det_params_.model_location);
        clnf_ = std::make_unique<LandmarkDetector::CLNF>(
            det_params_.model_location);
        if (!clnf_->loaded_successfully) {
            spdlog::error("CECLM model failed to load from: {}",
                          det_params_.model_location);
            clnf_.reset();
            return;
        }
        spdlog::info("CECLM model loaded successfully");

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

        // ---- Quality pre-check ----
        float aspect = bbox.width / bbox.height;
        if (aspect < cfg_.min_face_aspect ||
            static_cast<int>(bbox.width) < cfg_.min_face_width_px) {
            return result;
        }

        // ---- Camera intrinsics ----
        float fx, fy, cx, cy;
        ApproxIntrinsics(frame.cols, frame.rows, fx, fy, cx, cy);

        // ---- Set up visualizer ----
        cv::Mat canvas = frame.clone();
        visualizer_.SetImage(canvas, fx, fy, cx, cy);

        // ---- Grayscale for CLNF ----
        cv::Mat gray;
        if (frame.channels() == 3)
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else
            gray = frame;

        // ---- Correct bbox: full-face (SCRFD) → chin-to-eyebrows (PDM) ----
        // MTCNN applies this identical correction internally
        // (FaceDetectorMTCNN.cpp:872-877) to match PDM's 3D mean shape.
        // SCRFD outputs full-face bboxes, so we must replicate it.
        cv::Rect_<float> cb = bbox;
        cb.x += cb.width * -0.0075f;
        cb.y += cb.height * 0.2459f;
        cb.width *= 1.0323f;
        cb.height *= 0.7751f;

        // ---- Core: match official FaceLandmarkVid behavior ----
        // Official workflow:
        //   First frame: MTCNN detection → CalcParams → multi-view init
        //                (11 rotation hypotheses, best fit selected)
        //   Subsequent:  track from previous converged state (small windows)
        //   On failure:  MTCNN re-detect → multi-view reinit → save/restore
        //
        // We substitute SCRFD bbox for MTCNN detection on first frame only.
        // Subsequent frames never see the bbox — tracking is purely temporal.
        //
        // Guide internal MTCNN reinit to our target face: set preference_det
        // to the normalized center of the SCRFD bbox. If tracking fails and
        // MTCNN reinit triggers, DetectSingleFaceMTCNN selects the face
        // closest to this point instead of the most confident/largest one,
        // preventing reinit from latching onto a non-target face.
        clnf_->preference_det.x =
            (bbox.x + bbox.width * 0.5f) / frame.cols;
        clnf_->preference_det.y =
            (bbox.y + bbox.height * 0.5f) / frame.rows;

        cv::Rect_<double> bbox_d(cb.x, cb.y, cb.width, cb.height);
        bool detected;
        if (!clnf_->tracking_initialised) {
            clnf_->params_local.setTo(0);
            clnf_->pdm.CalcParams(clnf_->params_global, bbox_d,
                                  clnf_->params_local);
            clnf_->tracking_initialised = true;
            det_params_.window_sizes_current = det_params_.window_sizes_init;
            det_params_.multi_view = true;
            detected = LandmarkDetector::DetectLandmarksInImage(
                frame, bbox_d, *clnf_, det_params_, gray);
            det_params_.multi_view = false;
        } else {
            detected = LandmarkDetector::DetectLandmarksInVideo(
                frame, *clnf_, det_params_, gray);
        }

        if (!detected || !clnf_->detection_success) {
            return result;
        }

        result.valid = true;

        // ---- Landmark visibility ----
        {
            auto vis = clnf_->GetVisibilities();
            int vis_count = 0;
            if (!vis.empty()) {
                for (int r = 0; r < vis.rows; ++r)
                    if (vis.at<int>(r) != 0) ++vis_count;
            } else {
                vis_count = 68;
            }
            result.visible_landmarks = vis_count;

            // Map detection_certainty (-1=perfect..1=worst) to quality (0..1)
            float cert_norm = (clnf_->detection_certainty + 1.0f) / 2.0f;
            cert_norm = std::clamp(1.0f - cert_norm, 0.0f, 1.0f); // invert: -1→1.0, 1→0.0
            result.quality_score = cert_norm * 0.5f +
                (static_cast<float>(vis_count) / 68.0f) * 0.5f;
        }

        result.detection_certainty = clnf_->detection_certainty;
        result.detection_success = clnf_->detection_success;

        // ---- Feed OpenFace Visualizer ----
        visualizer_.SetObservationLandmarks(
            clnf_->detected_landmarks, clnf_->detection_certainty,
            clnf_->GetVisibilities());

        // ---- Head pose ----
        cv::Vec6f pose_vec = LandmarkDetector::GetPose(*clnf_, fx, fy, cx, cy);
        result.head_pose.pitch = pose_vec[3] * kDegPerRad;
        result.head_pose.yaw   = pose_vec[4] * kDegPerRad;
        result.head_pose.roll  = pose_vec[5] * kDegPerRad;
        result.pose_Tx = pose_vec[0];
        result.pose_Ty = pose_vec[1];
        result.pose_Tz = pose_vec[2];
        result.pdm_rigid[0] = clnf_->params_global[0];
        result.pdm_rigid[1] = clnf_->params_global[1];
        result.pdm_rigid[2] = clnf_->params_global[2];
        result.pdm_rigid[3] = clnf_->params_global[3];
        result.pdm_rigid[4] = clnf_->params_global[4];
        result.pdm_rigid[5] = clnf_->params_global[5];
        if (!clnf_->params_local.empty()) {
            result.pdm_nonrigid.assign(
                clnf_->params_local.begin(), clnf_->params_local.end());
        }
        visualizer_.SetObservationPose(pose_vec, clnf_->detection_certainty);

        // ---- Gaze ----
        if (clnf_->eye_model) {
            cv::Point3f gaze_left, gaze_right;
            GazeAnalysis::EstimateGaze(*clnf_, gaze_left, fx, fy, cx, cy, true);
            GazeAnalysis::EstimateGaze(*clnf_, gaze_right, fx, fy, cx, cy, false);
            cv::Vec2f gaze_angle = GazeAnalysis::GetGazeAngle(gaze_left, gaze_right);
            result.gaze_angle_x = gaze_angle[0];
            result.gaze_angle_y = gaze_angle[1];
            result.gaze_0_x = gaze_left.x;
            result.gaze_0_y = gaze_left.y;
            result.gaze_0_z = gaze_left.z;
            result.gaze_1_x = gaze_right.x;
            result.gaze_1_y = gaze_right.y;
            result.gaze_1_z = gaze_right.z;

            auto eye_lm2d = LandmarkDetector::CalculateAllEyeLandmarks(*clnf_);
            auto eye_lm3d = LandmarkDetector::Calculate3DEyeLandmarks(
                *clnf_, fx, fy, cx, cy);
            // Store as x0..xN, y0..yN (matching FeatureExtraction column order)
            if (!eye_lm2d.empty()) {
                result.eye_landmarks_2d.reserve(eye_lm2d.size() * 2);
                for (const auto& p : eye_lm2d)
                    result.eye_landmarks_2d.push_back(p.x);
                for (const auto& p : eye_lm2d)
                    result.eye_landmarks_2d.push_back(p.y);
            }
            if (!eye_lm3d.empty()) {
                result.eye_landmarks_3d.reserve(eye_lm3d.size() * 3);
                for (const auto& p : eye_lm3d)
                    result.eye_landmarks_3d.push_back(p.x);
                for (const auto& p : eye_lm3d)
                    result.eye_landmarks_3d.push_back(p.y);
                for (const auto& p : eye_lm3d)
                    result.eye_landmarks_3d.push_back(p.z);
            }
            visualizer_.SetObservationGaze(
                gaze_left, gaze_right, eye_lm2d, eye_lm3d,
                clnf_->detection_certainty);
        }

        // ---- Action units ----
        face_analyser_->AddNextFrame(frame, clnf_->detected_landmarks,
                                     clnf_->detection_success, 0.0, false);
        auto au_reg = face_analyser_->GetCurrentAUsReg();
        auto au_class = face_analyser_->GetCurrentAUsClass();
        result.action_units = au_reg;
        result.au_classifications = au_class;
        visualizer_.SetObservationActionUnits(au_reg, au_class);

        // ---- 68-point 2D landmarks ----
        if (clnf_->detected_landmarks.rows >= 136) {
            for (int i = 0; i < 136; ++i)
                result.landmarks_2d[i] = clnf_->detected_landmarks.at<float>(i);
        }

        // ---- 68-point 3D landmarks (world mm) ----
        cv::Mat_<float> shape_3d = clnf_->GetShape(fx, fy, cx, cy);
        if (!shape_3d.empty()) {
            result.landmarks_3d.assign(
                shape_3d.begin(), shape_3d.end());
        }

        frame = visualizer_.GetVisImage().clone();
        return result;
    }

    void Reset() {
        if (clnf_) clnf_->Reset();
        if (face_analyser_) face_analyser_->Reset();
    }

    cv::Mat GetAlignedFace() const {
        cv::Mat aligned;
        if (face_analyser_)
            face_analyser_->GetLatestAlignedFace(aligned);
        return aligned;
    }

    void GetHOG(cv::Mat_<double>& hog, int& rows, int& cols) const {
        if (face_analyser_)
            face_analyser_->GetLatestHOG(hog, rows, cols);
    }

private:
    const Config cfg_;
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

cv::Mat FaceAnalyzer::GetAlignedFace() const {
    return impl_->GetAlignedFace();
}

void FaceAnalyzer::GetHOG(cv::Mat_<double>& hog, int& num_rows,
                          int& num_cols) const {
    impl_->GetHOG(hog, num_rows, num_cols);
}

}  // namespace omniface::analysis
