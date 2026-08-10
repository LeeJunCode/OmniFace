#include "omniface/core/analysis/mediapipe/mediapipe_pose_estimator.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>

#include <onnxruntime_cxx_api.h>
#include <opencv2/calib3d.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/inference/onnx_session.h"
#include "omniface/core/inference/preprocess.h"

namespace omniface::analysis::mediapipe {

namespace {

std::vector<cv::Point3f> ParseCanonicalModel(const std::string& path) {
    std::vector<cv::Point3f> vertices;
    vertices.reserve(468);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("MediaPipePoseEstimator: cannot open canonical model: " + path);
    }

    std::string line;
    while (std::getline(file, line) && vertices.size() < 468) {
        if (line.size() < 4 || line[0] != 'v' || line[1] != ' ') continue;
        float x, y, z;
        if (std::sscanf(line.c_str(), "v %f %f %f", &x, &y, &z) == 3) {
            vertices.emplace_back(x, y, z);
        }
    }

    if (vertices.size() != 468) {
        throw std::runtime_error(
            "MediaPipePoseEstimator: canonical model has " + std::to_string(vertices.size()) +
            " vertices, expected 468");
    }
    return vertices;
}

HeadPose RotationToEuler(const cv::Mat& R) {
    HeadPose pose;

    float cy = std::sqrt(R.at<double>(2, 1) * R.at<double>(2, 1) +
                         R.at<double>(2, 2) * R.at<double>(2, 2));

    if (cy > 1e-6) {
        pose.pitch = std::atan2(R.at<double>(2, 1), R.at<double>(2, 2));
        pose.yaw   = std::atan2(-R.at<double>(2, 0), cy);
        pose.roll  = std::atan2(R.at<double>(1, 0), R.at<double>(0, 0));
    } else {
        pose.pitch = 0.0f;
        pose.yaw   = std::atan2(-R.at<double>(2, 0), cy);
        pose.roll  = std::atan2(-R.at<double>(0, 1), R.at<double>(1, 1));
    }

    constexpr float kRadToDeg = 180.0f / static_cast<float>(CV_PI);
    pose.pitch *= kRadToDeg;
    pose.yaw   *= kRadToDeg;
    pose.roll  *= kRadToDeg;

    return pose;
}

cv::Mat EstimateCameraMatrix(int img_w, int img_h) {
    float f = static_cast<float>(std::max(img_w, img_h));
    cv::Mat K = (cv::Mat_<double>(3, 3) << f, 0.0, img_w * 0.5,
                                            0.0, f,   img_h * 0.5,
                                            0.0, 0.0, 1.0);
    return K;
}

}  // namespace

class MediaPipePoseEstimator::Impl {
public:
    detail::OnnxSession session_;
    detail::FaceCropPreprocessor preproc_;
    Config config_;
    std::vector<cv::Point3f> canonical_model_;
    float face_size_ = 0.0f;
    std::string landmark_model_path_;

    Impl(const std::string& landmark_model_path, Config cfg)
        : session_(landmark_model_path, "MediaPipePoseEstimator", cfg.intra_threads, cfg.device)
        , preproc_(detail::PreprocessConfig{
              .target_size = 192,
              .norm = detail::NormKind::kSimple,
              .margin = cfg.margin
          })
        , config_(std::move(cfg))
        , landmark_model_path_(landmark_model_path) {

        canonical_model_ = ParseCanonicalModel(config_.canonical_model_path);

        float min_y = INFINITY, max_y = -INFINITY;
        for (const auto& v : canonical_model_) {
            min_y = std::min(min_y, v.y);
            max_y = std::max(max_y, v.y);
        }
        face_size_ = max_y - min_y;
    }

    PoseEstimationResult PostProcessOutputs(const std::vector<Ort::Value>& outputs,
                                             int img_w, int img_h) {
        PoseEstimationResult result;

        float score = 0.0f;
        const int32_t* landmarks_data = nullptr;

        for (size_t i = 0; i < session_.OutputCount(); ++i) {
            const auto& name = session_.OutputNameStrs()[i];
            if (name == "score" || name.find("score") != std::string::npos) {
                score = outputs[i].GetTensorData<float>()[0];
            } else if (name == "final_landmarks" ||
                       name.find("landmarks") != std::string::npos) {
                landmarks_data = outputs[i].GetTensorData<int32_t>();
            }
        }

        if (!landmarks_data || score < config_.score_threshold) return result;

        std::vector<cv::Point2f> image_pts;
        std::vector<cv::Point3f> object_pts;
        image_pts.reserve(468);
        object_pts.reserve(468);

        for (int i = 0; i < 468; ++i) {
            const int32_t* lm = &landmarks_data[i * 3];
            image_pts.emplace_back(static_cast<float>(lm[0]),
                                   static_cast<float>(lm[1]));
            object_pts.push_back(canonical_model_[i]);
        }

        cv::Mat K = EstimateCameraMatrix(img_w, img_h);
        static const cv::Mat dist = cv::Mat::zeros(4, 1, CV_64F);
        cv::Mat rvec, tvec;

        if (!cv::solvePnP(object_pts, image_pts, K, dist, rvec, tvec,
                          false, cv::SOLVEPNP_EPNP))
            return result;

        cv::Mat R;
        cv::Rodrigues(rvec, R);

        cv::Mat canonical_to_headpose = (cv::Mat_<double>(3, 3) <<
            1.0,  0.0,  0.0,
            0.0, -1.0,  0.0,
            0.0,  0.0, -1.0);
        cv::Mat R_headpose = R * canonical_to_headpose;
        result.pose = RotationToEuler(R_headpose);

        cv::Point3f nose_3d = canonical_model_[1];
        std::vector<cv::Point3f> nose_pts{nose_3d};
        std::vector<cv::Point2f> nose_proj;
        cv::projectPoints(nose_pts, rvec, tvec, K, dist, nose_proj);
        result.nose_tip = nose_proj[0];
        result.nose_proj_err = static_cast<float>(cv::norm(nose_proj[0] - image_pts[1]));

        std::vector<cv::Point2f> all_proj;
        cv::projectPoints(object_pts, rvec, tvec, K, dist, all_proj);
        float rms = 0;
        for (int i = 0; i < 468; ++i) {
            float dx = all_proj[i].x - image_pts[i].x;
            float dy = all_proj[i].y - image_pts[i].y;
            rms += dx * dx + dy * dy;
        }
        result.reproj_rms = std::sqrt(rms / 468.0f);
        result.valid = true;

        result.score = score;
        result.landmarks = std::move(image_pts);
        result.rvec = rvec.clone();
        result.tvec = tvec.clone();
        result.camera_matrix = K.clone();

        return result;
    }
};

MediaPipePoseEstimator::MediaPipePoseEstimator(const std::string& landmark_model_path, Config config) {
    if (landmark_model_path.empty()) {
        throw std::invalid_argument("MediaPipePoseEstimator: landmark_model_path is empty");
    }
    if (config.canonical_model_path.empty()) {
        throw std::invalid_argument("MediaPipePoseEstimator: canonical_model_path is empty");
    }
    impl_ = std::make_unique<Impl>(landmark_model_path, std::move(config));
}

MediaPipePoseEstimator::~MediaPipePoseEstimator() = default;
MediaPipePoseEstimator::MediaPipePoseEstimator(MediaPipePoseEstimator&&) noexcept = default;
MediaPipePoseEstimator& MediaPipePoseEstimator::operator=(MediaPipePoseEstimator&&) noexcept = default;

MediaPipePoseEstimator MediaPipePoseEstimator::Clone() const {
    return MediaPipePoseEstimator(impl_->landmark_model_path_, impl_->config_);
}

PoseEstimationResult MediaPipePoseEstimator::EstimatePose(const cv::Mat& bgr_image,
                                                  const BoundingBox& face_bbox) {
    PoseEstimationResult result;

    if (bgr_image.empty()) return result;
    if (face_bbox.x2 <= face_bbox.x1 || face_bbox.y2 <= face_bbox.y1) return result;

    detail::PreprocessResult prep;
    impl_->preproc_.Process(bgr_image, face_bbox, prep);
    int img_w = bgr_image.cols;
    int img_h = bgr_image.rows;

    auto& memory_info = impl_->session_.CpuMemoryInfo();

    std::array<int64_t, 4> img_shape{1, 3, 192, 192};
    auto img_tensor = Ort::Value::CreateTensor<float>(
        memory_info, prep.blob.ptr<float>(), prep.blob.total(),
        img_shape.data(), img_shape.size());

    const auto& cr = prep.crop_rect;
    std::array<int64_t, 2> crop_shape{1, 1};
    std::array<int32_t, 1> crop_x1{cr.x};
    std::array<int32_t, 1> crop_y1{cr.y};
    std::array<int32_t, 1> crop_w{cr.width};
    std::array<int32_t, 1> crop_h{cr.height};

    auto tx1 = Ort::Value::CreateTensor<int32_t>(
        memory_info, crop_x1.data(), 1, crop_shape.data(), crop_shape.size());
    auto ty1 = Ort::Value::CreateTensor<int32_t>(
        memory_info, crop_y1.data(), 1, crop_shape.data(), crop_shape.size());
    auto tw  = Ort::Value::CreateTensor<int32_t>(
        memory_info, crop_w.data(), 1, crop_shape.data(), crop_shape.size());
    auto th  = Ort::Value::CreateTensor<int32_t>(
        memory_info, crop_h.data(), 1, crop_shape.data(), crop_shape.size());

    std::vector<Ort::Value> input_tensors;
    input_tensors.reserve(5);
    input_tensors.push_back(std::move(img_tensor));
    input_tensors.push_back(std::move(tx1));
    input_tensors.push_back(std::move(ty1));
    input_tensors.push_back(std::move(tw));
    input_tensors.push_back(std::move(th));

    std::vector<Ort::Value> outputs;
    try {
        outputs = impl_->session_.Session().Run(
            Ort::RunOptions{nullptr},
            impl_->session_.InputNames(), input_tensors.data(), input_tensors.size(),
            impl_->session_.OutputNames(), impl_->session_.OutputCount());
    } catch (const std::exception&) {
        return result;
    }

    return impl_->PostProcessOutputs(outputs, img_w, img_h);
}

PoseEstimationResult MediaPipePoseEstimator::EstimatePoseFromCrop(const cv::Mat& face_roi,
                                                         const cv::Rect& crop_rect,
                                                         int original_img_w,
                                                         int original_img_h) {
    PoseEstimationResult result;
    if (face_roi.empty()) return result;

    detail::PreprocessConfig pp_cfg{
        .target_size = 192,
        .norm = detail::NormKind::kSimple,
        .margin = impl_->config_.margin,
        .skip_crop = true
    };
    detail::FaceCropPreprocessor preproc(pp_cfg);
    detail::PreprocessResult prep;
    preproc.ProcessFromCrop(face_roi, crop_rect, prep);

    int img_w = (original_img_w > 0) ? original_img_w : crop_rect.width;
    int img_h = (original_img_h > 0) ? original_img_h : crop_rect.height;

    auto& memory_info = impl_->session_.CpuMemoryInfo();
    std::array<int64_t, 4> img_shape{1, 3, 192, 192};
    auto img_tensor = Ort::Value::CreateTensor<float>(
        memory_info, prep.blob.ptr<float>(), prep.blob.total(),
        img_shape.data(), img_shape.size());

    const auto& cr = prep.crop_rect;
    std::array<int64_t, 2> crop_shape{1, 1};
    std::array<int32_t, 1> crop_x1{cr.x};
    std::array<int32_t, 1> crop_y1{cr.y};
    std::array<int32_t, 1> crop_w{cr.width};
    std::array<int32_t, 1> crop_h{cr.height};

    auto tx1 = Ort::Value::CreateTensor<int32_t>(
        memory_info, crop_x1.data(), 1, crop_shape.data(), crop_shape.size());
    auto ty1 = Ort::Value::CreateTensor<int32_t>(
        memory_info, crop_y1.data(), 1, crop_shape.data(), crop_shape.size());
    auto tw  = Ort::Value::CreateTensor<int32_t>(
        memory_info, crop_w.data(), 1, crop_shape.data(), crop_shape.size());
    auto th  = Ort::Value::CreateTensor<int32_t>(
        memory_info, crop_h.data(), 1, crop_shape.data(), crop_shape.size());

    std::vector<Ort::Value> input_tensors;
    input_tensors.reserve(5);
    input_tensors.push_back(std::move(img_tensor));
    input_tensors.push_back(std::move(tx1));
    input_tensors.push_back(std::move(ty1));
    input_tensors.push_back(std::move(tw));
    input_tensors.push_back(std::move(th));

    std::vector<Ort::Value> outputs;
    try {
        outputs = impl_->session_.Session().Run(
            Ort::RunOptions{nullptr},
            impl_->session_.InputNames(), input_tensors.data(), input_tensors.size(),
            impl_->session_.OutputNames(), impl_->session_.OutputCount());
    } catch (const std::exception&) {
        return result;
    }

    return impl_->PostProcessOutputs(outputs, img_w, img_h);
}

const std::vector<cv::Point3f>& MediaPipePoseEstimator::canonical_model() const {
    return impl_->canonical_model_;
}

float MediaPipePoseEstimator::face_size() const { return impl_->face_size_; }

}  // namespace omniface::analysis::mediapipe
