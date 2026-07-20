#include "omniface/core/analysis/pose_estimator.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <opencv2/calib3d.hpp>
#include <stdexcept>
#include <unordered_set>

#include "omniface/core/detail/onnx_session.h"
#include "omniface/core/detail/preprocess.h"

namespace omniface::analysis {

namespace {

inline const std::unordered_set<int>& StableLandmarkIndices() {
    static const std::unordered_set<int> idx = [] {
        std::unordered_set<int> s;

        for (int i : {10,  338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288, 397, 365, 379,
                      378, 400, 377, 152, 148, 176, 149, 150, 136, 172, 58,  132, 93,  234, 127,
                      162, 21,  54,  103, 67,  109, 8,   9,   151, 108, 69,  104, 68,  71})
            s.insert(i);

        for (int i : {33, 7, 163, 144, 145, 153, 154, 155, 133, 173, 157, 158, 159, 160, 161, 246})
            s.insert(i);

        for (int i :
             {362, 382, 381, 380, 374, 373, 390, 249, 263, 466, 388, 387, 386, 385, 384, 398})
            s.insert(i);

        for (int i : {46,  53,  52,  65,  55,  107, 66,  105, 63,  70,
                      276, 283, 282, 295, 285, 336, 296, 334, 293, 300})
            s.insert(i);

        for (int i :
             {168, 6, 197, 195, 5, 4, 1, 2, 3, 19, 94, 97, 98, 115, 122, 126, 64, 294, 79, 309})
            s.insert(i);

        for (int i : {0, 11, 12, 13, 14, 15, 16, 17, 37, 39, 40, 185, 61, 291, 267, 269, 270, 409})
            s.insert(i);
        return s;
    }();
    return idx;
}

std::vector<cv::Point3f> ParseCanonicalModel(const std::string& path) {
    std::vector<cv::Point3f> vertices;
    vertices.reserve(468);

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("PoseEstimator: cannot open canonical model: " + path);
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
        throw std::runtime_error("PoseEstimator: canonical model has " +
                                 std::to_string(vertices.size()) + " vertices, expected 468");
    }
    return vertices;
}

HeadPose RotationToEuler(const cv::Mat& R) {
    HeadPose pose;

    float cy = std::sqrt(R.at<double>(2, 1) * R.at<double>(2, 1) +
                         R.at<double>(2, 2) * R.at<double>(2, 2));

    if (cy > 1e-6) {
        pose.pitch = std::atan2(R.at<double>(2, 1), R.at<double>(2, 2));
        pose.yaw = std::atan2(-R.at<double>(2, 0), cy);
        pose.roll = std::atan2(R.at<double>(1, 0), R.at<double>(0, 0));
    } else {
        pose.pitch = 0.0f;
        pose.yaw = std::atan2(-R.at<double>(2, 0), cy);
        pose.roll = std::atan2(-R.at<double>(0, 1), R.at<double>(1, 1));
    }

    constexpr float kRadToDeg = 180.0f / static_cast<float>(CV_PI);
    pose.pitch *= kRadToDeg;
    pose.yaw *= kRadToDeg;
    pose.roll *= kRadToDeg;

    return pose;
}

cv::Mat EstimateCameraMatrix(int img_w, int img_h) {
    float f = static_cast<float>(std::max(img_w, img_h));
    cv::Mat K = (cv::Mat_<double>(3, 3) << f, 0.0, img_w * 0.5, 0.0, f, img_h * 0.5, 0.0, 0.0, 1.0);
    return K;
}

}  // namespace

class PoseEstimator::Impl {
public:
    detail::OnnxSession session_;
    detail::FaceCropPreprocessor preproc_;
    Config config_;

    std::vector<cv::Point3f> canonical_model_;

    float face_size_ = 0.0f;

    Impl(const std::string& landmark_model_path, Config cfg)
        : session_(landmark_model_path, "PoseEstimator", cfg.intra_threads),
          preproc_(detail::PreprocessConfig{
              .target_size = 192, .norm = detail::NormKind::kSimple, .margin = cfg.margin}),
          config_(std::move(cfg)) {
        canonical_model_ = ParseCanonicalModel(config_.canonical_model_path);

        {
            float min_y = INFINITY, max_y = -INFINITY;
            for (const auto& v : canonical_model_) {
                min_y = std::min(min_y, v.y);
                max_y = std::max(max_y, v.y);
            }
            face_size_ = max_y - min_y;
        }
    }
};

PoseEstimator::PoseEstimator(const std::string& landmark_model_path, Config config) {
    if (landmark_model_path.empty()) {
        throw std::invalid_argument("PoseEstimator: landmark_model_path is empty");
    }
    if (config.canonical_model_path.empty()) {
        throw std::invalid_argument("PoseEstimator: canonical_model_path is empty");
    }
    impl_ = std::make_unique<Impl>(landmark_model_path, std::move(config));
}

PoseEstimator::~PoseEstimator() = default;

PoseEstimator::PoseEstimator(PoseEstimator&&) noexcept = default;
PoseEstimator& PoseEstimator::operator=(PoseEstimator&&) noexcept = default;

PoseEstimationResult PoseEstimator::EstimatePose(const cv::Mat& bgr_image,
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
        memory_info, prep.blob.ptr<float>(), prep.blob.total(), img_shape.data(), img_shape.size());

    const auto& cr = prep.crop_rect;
    std::array<int64_t, 2> crop_shape{1, 1};
    std::array<int32_t, 1> crop_x1{cr.x};
    std::array<int32_t, 1> crop_y1{cr.y};
    std::array<int32_t, 1> crop_w{cr.width};
    std::array<int32_t, 1> crop_h{cr.height};

    auto tx1 = Ort::Value::CreateTensor<int32_t>(memory_info, crop_x1.data(), 1, crop_shape.data(),
                                                 crop_shape.size());
    auto ty1 = Ort::Value::CreateTensor<int32_t>(memory_info, crop_y1.data(), 1, crop_shape.data(),
                                                 crop_shape.size());
    auto tw = Ort::Value::CreateTensor<int32_t>(memory_info, crop_w.data(), 1, crop_shape.data(),
                                                crop_shape.size());
    auto th = Ort::Value::CreateTensor<int32_t>(memory_info, crop_h.data(), 1, crop_shape.data(),
                                                crop_shape.size());

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
            Ort::RunOptions{nullptr}, impl_->session_.InputNames(), input_tensors.data(),
            input_tensors.size(), impl_->session_.OutputNames(), impl_->session_.OutputCount());
    } catch (const std::exception& e) {
        return result;
    }

    float score = 0.0f;
    const int32_t* landmarks_data = nullptr;

    for (size_t i = 0; i < impl_->session_.OutputCount(); ++i) {
        const auto& name = impl_->session_.OutputNameStrs()[i];
        if (name == "score" || name.find("score") != std::string::npos) {
            score = outputs[i].GetTensorData<float>()[0];
        } else if (name == "final_landmarks" || name.find("landmarks") != std::string::npos) {
            landmarks_data = outputs[i].GetTensorData<int32_t>();
        }
    }

    if (!landmarks_data) {
        return result;
    }

    if (score < impl_->config_.score_threshold) {
        return result;
    }

    std::vector<cv::Point2f> all_landmarks;
    all_landmarks.reserve(468);
    for (int i = 0; i < 468; ++i) {
        const int32_t* lm = &landmarks_data[i * 3];
        all_landmarks.emplace_back(static_cast<float>(lm[0]), static_cast<float>(lm[1]));
    }

    const auto& stable_set = StableLandmarkIndices();
    std::vector<cv::Point2f> image_pts;
    std::vector<cv::Point3f> object_pts;
    image_pts.reserve(stable_set.size());
    object_pts.reserve(stable_set.size());

    for (int i = 0; i < 468; ++i) {
        if (stable_set.find(i) == stable_set.end()) continue;
        const int32_t* lm = &landmarks_data[i * 3];
        image_pts.emplace_back(static_cast<float>(lm[0]), static_cast<float>(lm[1]));
        object_pts.push_back(impl_->canonical_model_[i]);
    }

    cv::Mat K = EstimateCameraMatrix(img_w, img_h);
    static const cv::Mat dist = cv::Mat::zeros(4, 1, CV_64F);
    cv::Mat rvec, tvec;

    bool pnp_ok =
        cv::solvePnP(object_pts, image_pts, K, dist, rvec, tvec, false, cv::SOLVEPNP_EPNP);

    if (!pnp_ok) {
        return result;
    }

    cv::solvePnPRefineLM(object_pts, image_pts, K, dist, rvec, tvec);

    cv::Mat R;
    cv::Rodrigues(rvec, R);

    cv::Mat canonical_to_headpose =
        (cv::Mat_<double>(3, 3) << 1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, -1.0);
    cv::Mat R_headpose = R * canonical_to_headpose;
    result.pose = RotationToEuler(R_headpose);

    cv::Point3f nose_3d = impl_->canonical_model_[1];
    std::vector<cv::Point3f> nose_pts{nose_3d};
    std::vector<cv::Point2f> nose_proj;
    cv::projectPoints(nose_pts, rvec, tvec, K, dist, nose_proj);
    result.nose_tip = nose_proj[0];
    result.valid = true;

    result.landmarks = std::move(all_landmarks);
    result.rvec = rvec.clone();
    result.tvec = tvec.clone();
    result.camera_matrix = K.clone();

    return result;
}

const std::vector<cv::Point3f>& PoseEstimator::canonical_model() const {
    return impl_->canonical_model_;
}
float PoseEstimator::face_size() const {
    return impl_->face_size_;
}

}  // namespace omniface::analysis
