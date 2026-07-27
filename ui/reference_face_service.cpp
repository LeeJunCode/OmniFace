#include "omniface/ui/reference_face_service.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/align.h"
#include "omniface/core/recognition/face_recognizer.h"

namespace omniface::ui {

FaceEmbedding ExtractReferenceFace(const std::string& image_path,
                                    const PipelineConfig& config,
                                    bool& out_success) {
    out_success = false;
    cv::Mat ref_img = cv::imread(image_path);
    if (ref_img.empty()) {
        spdlog::error("Failed to load reference image: {}", image_path);
        return {};
    }

    detection::FaceDetector::Config det_cfg;
    det_cfg.intra_threads = config.model.detection_threads;
    det_cfg.input_width = config.model.detection_input_width;
    det_cfg.input_height = config.model.detection_input_height;
    det_cfg.confidence_threshold = config.thresholds.detection_confidence;
    det_cfg.nms_threshold = config.thresholds.detection_nms;
    detection::FaceDetector detector(config.model.detector_model, det_cfg);

    auto boxes = detector.Detect(ref_img);
    if (boxes.empty()) {
        spdlog::warn("No face detected in reference image");
        return {};
    }

    const BoundingBox* best = &boxes[0];
    for (const auto& b : boxes) {
        if (b.confidence > best->confidence) best = &b;
    }

    cv::Mat face_crop = recognition::AlignFace(ref_img, *best);
    if (face_crop.empty()) {
        spdlog::error("Face alignment failed for reference image");
        return {};
    }

    recognition::FaceRecognizer::Config rec_cfg;
    rec_cfg.intra_threads = config.model.recognition_threads;
    recognition::FaceRecognizer recognizer(config.model.recognizer_model, rec_cfg);

    auto embedding = recognizer.Extract(face_crop);
    out_success = embedding.valid;
    if (embedding.valid) {
        spdlog::info("Reference face embedding cached (confidence={:.2f})", best->confidence);
    }
    return embedding;
}

cv::Mat DrawDetectionsOnImage(const cv::Mat& image, const std::string& model_path,
                              const PipelineConfig& config) {
    cv::Mat result = image.clone();
    try {
        detection::FaceDetector::Config det_cfg;
        det_cfg.intra_threads = config.model.detection_threads;
        det_cfg.input_width = config.model.detection_input_width;
        det_cfg.input_height = config.model.detection_input_height;
        det_cfg.confidence_threshold = 0.5f;
        det_cfg.nms_threshold = config.thresholds.detection_nms;
        detection::FaceDetector detector(model_path, det_cfg);

        auto boxes = detector.Detect(result);
        for (const auto& b : boxes) {
            cv::Rect r(static_cast<int>(b.x1), static_cast<int>(b.y1),
                       static_cast<int>(b.x2 - b.x1), static_cast<int>(b.y2 - b.y1));
            cv::rectangle(result, r, cv::Scalar(0, 220, 0), 4);
            char conf_text[32];
            std::snprintf(conf_text, sizeof(conf_text), "%.2f", b.confidence);
            cv::putText(result, conf_text, cv::Point(r.x, r.y - 8),
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 220, 0), 3, cv::LINE_AA);
            for (int k = 0; k < 5; ++k) {
                cv::Point kp(static_cast<int>(b.keypoints[k * 2]),
                             static_cast<int>(b.keypoints[k * 2 + 1]));
                cv::circle(result, kp, 3, cv::Scalar(0, 0, 255), -1);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Face detection failed on image: {}", e.what());
    }
    return result;
}

}  // namespace omniface::ui
