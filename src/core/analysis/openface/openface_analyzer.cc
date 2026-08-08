#include "omniface/core/analysis/openface/openface_analyzer.h"
#include "omniface/pipeline/config.h"

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

namespace omniface::analysis::openface {

namespace {

constexpr float kDegPerRad = 180.0f / 3.14159265358979323846f;

void ApproxIntrinsics(int cols, int rows, float& fx, float& fy,
                      float& cx, float& cy) {
    fx = fy = static_cast<float>(cols);
    cx = static_cast<float>(cols) / 2.0f;
    cy = static_cast<float>(rows) / 2.0f;
}

std::string ResolveModelFile(const std::string& model_dir,
                             const std::string& explicit_file) {
    namespace fs = std::filesystem;
    fs::path base = fs::absolute(model_dir).lexically_normal();
    if (!explicit_file.empty())
        return (base / explicit_file).string();
    return (base / "model" / "main_ceclm_general.txt").string();
}

}  // namespace

OpenFaceParams MakeProfileParams(OpenFaceProfile profile,
                                 const std::string& model_dir) {
    OpenFaceParams p;
    switch (profile) {
        case OpenFaceProfile::kMaxQuality: {
            p.window_sizes_small        = {0, 11, 9, 0};
            p.window_sizes_init         = {15, 13, 11, 11};
            p.num_optimisation_iteration = 10;
            p.sigma                     = 1.25f;
            p.reg_factor                = 35.0f;
            p.weight_factor             = 2.5f;
            p.use_face_template         = true;
            p.face_template_scale       = 0.5f;
            p.validate_detections       = true;
            p.validation_boundary       = 0.65f;
            p.refine_hierarchical       = true;
            p.refine_parameters         = true;
            p.multi_view_first_frame    = true;
            p.reinit_video_every        = 1;
            p.limit_pose                = false;
            break;
        }
        case OpenFaceProfile::kStandard:
        default: {
            p.window_sizes_small        = {0, 9, 7, 0};
            p.window_sizes_init         = {11, 9, 7, 5};
            p.num_optimisation_iteration = 5;
            p.sigma                     = 1.5f;
            p.reg_factor                = 25.0f;
            p.weight_factor             = 0.0f;
            p.use_face_template         = true;
            p.face_template_scale       = 0.3f;
            p.validate_detections       = true;
            p.validation_boundary       = 0.725f;
            p.refine_hierarchical       = true;
            p.refine_parameters         = true;
            p.multi_view_first_frame    = true;
            p.reinit_video_every        = 2;
            p.limit_pose                = true;
            break;
        }
    }
    p.model_file = ResolveModelFile(model_dir, "");
    p.mtcnn_detector_path =
        (std::filesystem::absolute(model_dir).lexically_normal()
         / "mtcnn_detector" / "MTCNN_detector.txt").string();
    return p;
}

void ApplyOverrides(OpenFaceParams& p, const omniface::OpenFaceConfig& cfg,
                    const std::string& model_dir) {
    if (!cfg.model_file.empty())
        p.model_file = ResolveModelFile(model_dir, cfg.model_file);
    if (!cfg.window_sizes_small.empty()) {
        if (cfg.window_sizes_small.size() == 4)
            p.window_sizes_small = cfg.window_sizes_small;
        else
            spdlog::warn("window_sizes_small must have 4 elements, keeping profile default");
    }
    if (!cfg.window_sizes_init.empty()) {
        if (cfg.window_sizes_init.size() == 4)
            p.window_sizes_init = cfg.window_sizes_init;
        else
            spdlog::warn("window_sizes_init must have 4 elements, keeping profile default");
    }
    if (cfg.optimise_iterations >= 0)   p.num_optimisation_iteration = cfg.optimise_iterations;
    if (cfg.sigma >= 0.0f)              p.sigma          = cfg.sigma;
    if (cfg.reg_factor >= 0.0f)         p.reg_factor     = cfg.reg_factor;
    if (cfg.weight_factor >= 0.0f)      p.weight_factor  = cfg.weight_factor;
    if (cfg.use_face_template >= 0)     p.use_face_template = (cfg.use_face_template != 0);
    if (cfg.face_template_scale >= 0.0f) p.face_template_scale = cfg.face_template_scale;
    if (cfg.validate_detections >= 0)   p.validate_detections = (cfg.validate_detections != 0);
    if (cfg.validation_boundary >= 0.0f) p.validation_boundary = cfg.validation_boundary;
    if (cfg.refine_hierarchical >= 0)   p.refine_hierarchical = (cfg.refine_hierarchical != 0);
    if (cfg.refine_parameters >= 0)     p.refine_parameters   = (cfg.refine_parameters != 0);
    if (cfg.multi_view_first_frame >= 0) p.multi_view_first_frame = (cfg.multi_view_first_frame != 0);
    if (cfg.reinit_video_every >= 0)    p.reinit_video_every  = cfg.reinit_video_every;
    if (cfg.limit_pose >= 0)            p.limit_pose = (cfg.limit_pose != 0);
}

OpenFaceProfile ParseProfile(const std::string& name) {
    if (name == "max_quality") return OpenFaceProfile::kMaxQuality;
    if (name == "standard")    return OpenFaceProfile::kStandard;
    spdlog::warn("Unknown OpenFace profile '{}', falling back to standard", name);
    return OpenFaceProfile::kStandard;
}

class OpenFaceAnalyzer::Impl {
public:
    explicit Impl(const Config& cfg)
        : cfg_(cfg),
          visualizer_(true, false, false, true)
    {
        const auto& p = cfg_.openface;
        visualizer_.visualisation_boundary = -2.0;
        namespace fs = std::filesystem;
        fs::path model_dir = fs::absolute(cfg.model_dir).lexically_normal();

        if (!fs::is_directory(model_dir)) {
            spdlog::error("OpenFace model directory not found: {}", model_dir.string());
            return;
        }

        det_params_.model_location = p.model_file;
        det_params_.mtcnn_face_detector_location = p.mtcnn_detector_path;

        det_params_.validate_detections       = p.validate_detections;
        det_params_.validation_boundary       = p.validation_boundary;
        det_params_.reinit_video_every        = p.reinit_video_every;
        det_params_.window_sizes_small        = p.window_sizes_small;
        det_params_.window_sizes_init         = p.window_sizes_init;
        det_params_.weight_factor             = p.weight_factor;
        det_params_.num_optimisation_iteration = p.num_optimisation_iteration;
        det_params_.use_face_template         = p.use_face_template;
        det_params_.face_template_scale       = p.face_template_scale;
        det_params_.refine_hierarchical       = p.refine_hierarchical;
        det_params_.refine_parameters         = p.refine_parameters;
        det_params_.limit_pose                = p.limit_pose;

        det_params_.sigma                     = p.sigma;
        det_params_.reg_factor                = p.reg_factor;

        spdlog::info("Loading OpenFace model (CE-CLM): sigma={:.2f} reg={:.1f} iters={}",
                     det_params_.sigma, det_params_.reg_factor,
                     det_params_.num_optimisation_iteration);
        spdlog::info("  model: {}", det_params_.model_location);
        clnf_ = std::make_unique<LandmarkDetector::CLNF>(
            det_params_.model_location);
        if (!clnf_->loaded_successfully) {
            spdlog::error("OpenFace model failed to load: {}",
                          det_params_.model_location);
            clnf_.reset();
            return;
        }
        spdlog::info("OpenFace model loaded successfully (CE-CLM)");

        if (clnf_->eye_model) {
            spdlog::info("Gaze estimation: enabled");
        } else {
            spdlog::warn("Gaze estimation: eye model unavailable");
        }

        FaceAnalysis::FaceAnalyserParameters fa_params(
            model_dir.string());
        fa_params.OptimizeForVideos();
        face_analyser_ = std::make_unique<FaceAnalysis::FaceAnalyser>(fa_params);
        spdlog::info("FaceAnalyser (AU) initialized (dynamic)");
    }

    FaceAnalysisResult Analyze(cv::Mat& frame, const cv::Rect_<float>& bbox) {
        FaceAnalysisResult result;

        if (!clnf_) return result;
        if (frame.empty()) return result;

        float aspect = bbox.width / bbox.height;
        if (aspect < cfg_.min_face_aspect ||
            static_cast<int>(bbox.width) < cfg_.min_face_width_px) {
            return result;
        }

        float fx, fy, cx, cy;
        ApproxIntrinsics(frame.cols, frame.rows, fx, fy, cx, cy);

        cv::Mat canvas = frame.clone();
        visualizer_.SetImage(canvas, fx, fy, cx, cy);

        cv::Mat gray;
        if (frame.channels() == 3)
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else
            gray = frame;

        cv::Rect_<float> cb = bbox;
        cb.x += cb.width * -0.0075f;
        cb.y += cb.height * 0.2459f;
        cb.width *= 1.0323f;
        cb.height *= 0.7751f;

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
            det_params_.multi_view = cfg_.openface.multi_view_first_frame;
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

            float cert_norm = (clnf_->detection_certainty + 1.0f) / 2.0f;
            cert_norm = std::clamp(1.0f - cert_norm, 0.0f, 1.0f);
            result.quality_score = cert_norm * 0.5f +
                (static_cast<float>(vis_count) / 68.0f) * 0.5f;
        }

        result.detection_certainty = clnf_->detection_certainty;
        result.detection_success = clnf_->detection_success;

        visualizer_.SetObservationLandmarks(
            clnf_->detected_landmarks, clnf_->detection_certainty,
            clnf_->GetVisibilities());

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

        if (face_analyser_) {
            face_analyser_->AddNextFrame(frame, clnf_->detected_landmarks,
                                         clnf_->detection_success, 0.0, false);
            auto au_reg = face_analyser_->GetCurrentAUsReg();
            auto au_class = face_analyser_->GetCurrentAUsClass();
            result.action_units = au_reg;
            result.au_classifications = au_class;
            visualizer_.SetObservationActionUnits(au_reg, au_class);
        }

        if (clnf_->detected_landmarks.rows >= 136) {
            for (int i = 0; i < 136; ++i)
                result.landmarks_2d[i] = clnf_->detected_landmarks.at<float>(i);
        }

        cv::Mat_<float> shape_3d = clnf_->GetShape(fx, fy, cx, cy);
        if (!shape_3d.empty()) {
            result.landmarks_3d.assign(
                shape_3d.begin(), shape_3d.end());
        }

        frame = visualizer_.GetVisImage().clone();
        return result;
    }

    bool IsValid() const { return clnf_ != nullptr; }

    bool CanAlignFace() const { return face_analyser_ != nullptr; }

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
        if (face_analyser_) {
            face_analyser_->GetLatestHOG(hog, rows, cols);
        } else {
            hog = cv::Mat_<double>();
            rows = 0;
            cols = 0;
        }
    }

    void EnsureWriterInitialized(const std::string& input_path,
                                 const std::string& output_dir,
                                 double fps, bool output_aligned) {
        writer_.Initialize(input_path, output_dir, fps, output_aligned);
    }

    void WriteObservation(const FaceAnalysisResult& analysis,
                          int frame_index, double fps,
                          const cv::Mat& aligned_face) {
        writer_.WriteObservation(analysis, frame_index, fps, aligned_face);
    }

    void CloseWriter() { writer_.Close(); }

private:
    const Config cfg_;
    LandmarkDetector::FaceModelParameters det_params_;
    std::unique_ptr<LandmarkDetector::CLNF> clnf_;
    std::unique_ptr<FaceAnalysis::FaceAnalyser> face_analyser_;
    Utilities::Visualizer visualizer_;
    OpenFaceWriter writer_;
};

OpenFaceAnalyzer::OpenFaceAnalyzer(const Config& cfg)
    : impl_(std::make_unique<Impl>(cfg)) {}

OpenFaceAnalyzer::~OpenFaceAnalyzer() = default;

bool OpenFaceAnalyzer::IsValid() const {
    return impl_->IsValid();
}

bool OpenFaceAnalyzer::CanAlignFace() const {
    return impl_->CanAlignFace();
}

FaceAnalysisResult OpenFaceAnalyzer::Analyze(cv::Mat& frame,
                                         const cv::Rect_<float>& bbox) {
    return impl_->Analyze(frame, bbox);
}

FrameAnalysisResult OpenFaceAnalyzer::Analyze(const cv::Mat& frame,
                                               const BoundingBox& bbox,
                                               int track_id) {
    cv::Mat frame_copy = frame.clone();
    cv::Rect_<float> rect(bbox.x1, bbox.y1, bbox.x2 - bbox.x1, bbox.y2 - bbox.y1);
    auto native = impl_->Analyze(frame_copy, rect);

    FrameAnalysisResult result;
    result.backend = AnalyzerKind::kOpenFace;
    result.track_id = track_id;
    result.valid = native.valid;
    if (native.valid) {
        result.head_pose = native.head_pose;
        result.gaze_angle_x = native.gaze_angle_x;
        result.gaze_angle_y = native.gaze_angle_y;
        result.landmarks_2d = native.landmarks_2d;
        result.action_units = native.action_units;
        result.quality_score = native.quality_score;
        result.visible_landmarks = native.visible_landmarks;
        result.detection_certainty = native.detection_certainty;
        result.detection_success = native.detection_success;
        result.gaze_0_x = native.gaze_0_x;
        result.gaze_0_y = native.gaze_0_y;
        result.gaze_0_z = native.gaze_0_z;
        result.gaze_1_x = native.gaze_1_x;
        result.gaze_1_y = native.gaze_1_y;
        result.gaze_1_z = native.gaze_1_z;
        result.pose_Tx = native.pose_Tx;
        result.pose_Ty = native.pose_Ty;
        result.pose_Tz = native.pose_Tz;
        result.pdm_rigid = native.pdm_rigid;
        result.pdm_nonrigid = native.pdm_nonrigid;
        result.eye_landmarks_2d = native.eye_landmarks_2d;
        result.eye_landmarks_3d = native.eye_landmarks_3d;
        result.landmarks_3d = native.landmarks_3d;
    }
    return result;
}

void OpenFaceAnalyzer::Reset() { impl_->Reset(); }

cv::Mat OpenFaceAnalyzer::GetAlignedFace() const {
    return impl_->GetAlignedFace();
}

void OpenFaceAnalyzer::GetHOG(cv::Mat_<double>& hog, int& num_rows,
                          int& num_cols) const {
    impl_->GetHOG(hog, num_rows, num_cols);
}

void OpenFaceAnalyzer::EnsureWriterInitialized(const std::string& input_path,
                                                const std::string& output_dir,
                                                double fps,
                                                bool output_aligned) {
    impl_->EnsureWriterInitialized(input_path, output_dir, fps, output_aligned);
}

void OpenFaceAnalyzer::WriteObservation(const FaceAnalysisResult& analysis,
                                         int frame_index, double fps,
                                         const cv::Mat& aligned_face) {
    impl_->WriteObservation(analysis, frame_index, fps, aligned_face);
}

void OpenFaceAnalyzer::CloseWriter() {
    impl_->CloseWriter();
}

}  // namespace omniface::analysis::openface
