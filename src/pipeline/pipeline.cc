#include "omniface/pipeline/pipeline.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <unordered_map>

#include "omniface/core/analysis/au_analyzer.h"
#include "omniface/core/recognition/align.h"
#include "omniface/viz/visualizer.h"

namespace omniface::pipeline {

Pipeline::Pipeline(const AppConfig& cfg) : cfg_(cfg) {}

bool Pipeline::Initialize() {
    try {
        detection::FaceDetector::Config dc;
        dc.confidence_threshold = cfg_.detection_confidence;
        dc.nms_threshold = cfg_.detection_nms;
        dc.intra_threads = cfg_.detection_threads;
        dc.input_width = cfg_.detection_input_width;
        dc.input_height = cfg_.detection_input_height;
        detector_ = std::make_unique<detection::FaceDetector>(cfg_.detector_model, dc);

        recognition::FaceRecognizer::Config rc;
        rc.intra_threads = cfg_.recognition_threads;
        recognizer_ = std::make_unique<recognition::FaceRecognizer>(cfg_.recognizer_model, rc);
    } catch (const std::exception& e) {
        init_error_ = std::string("MODEL_LOAD_FAIL|") + e.what();
        return false;
    }
    matcher_ = std::make_unique<recognition::FaceMatcher>();

    {
        cv::Mat ref_img = cv::imread(cfg_.reference_image, cv::IMREAD_COLOR);
        if (ref_img.empty()) {
            init_error_ = "REF_LOAD_FAIL|无法加载参考图片: " + cfg_.reference_image;
            return false;
        }

        auto ref_dets = detector_->Detect(ref_img);
        if (ref_dets.empty()) {
            init_error_ = "REF_NO_FACE|参考图片中未检测到人脸";
            return false;
        }

        std::sort(ref_dets.begin(), ref_dets.end(), [](const BoundingBox& a, const BoundingBox& b) {
            return a.confidence > b.confidence;
        });
        int idx = cfg_.reference_face_index;
        if (idx < 0 || idx >= static_cast<int>(ref_dets.size())) idx = 0;
        const auto& best_det = ref_dets[static_cast<size_t>(idx)];

        cv::Mat aligned = recognition::AlignFace(ref_img, best_det);
        if (aligned.empty()) {
            init_error_ = "REF_ALIGN_FAIL|目标人脸对齐失败";
            return false;
        }

        FaceEmbedding ref_emb = recognizer_->Extract(aligned);
        if (!ref_emb.valid) {
            init_error_ = "REF_EMBED_FAIL|目标人脸特征提取失败";
            return false;
        }

        matcher_->SetReference(std::move(ref_emb));
    }

    {
        auto& tc = cfg_.tracker;
        tracking::ByteTrackEngine::Config ec{tc.track_buffer, tc.track_thresh, tc.high_thresh,
                                             tc.match_thresh, tc.match_thresh_second};
        tracker_ = std::make_unique<tracking::ByteTrackEngine>(ec);
    }

    if (cfg_.backend == AnalysisBackend::kOpenFace) {
        try {
            openface_ = std::make_unique<analysis::OpenFaceAnalyzer>(cfg_.openface.model_dir);
        } catch (const std::exception& e) {
        }
    } else {
        auto& oc = cfg_.onnx;
        if (oc.enable_pose && std::filesystem::is_regular_file(oc.pose_model)) {
            try {
                analysis::PoseEstimator::Config pc;
                pc.canonical_model_path = oc.canonical_model;
                pc.margin = oc.pose_margin;
                pc.score_threshold = oc.pose_score_threshold;
                pc.intra_threads = oc.analysis_threads;
                pose_estimator_ = std::make_unique<analysis::PoseEstimator>(oc.pose_model, pc);
                nose_canonical_ = pose_estimator_->canonical_model()[1];
            } catch (const std::exception& e) {
            }
        }
        if (oc.enable_gaze && std::filesystem::is_regular_file(oc.gaze_model)) {
            try {
                analysis::GazeEstimator::Config gc;
                gc.margin = oc.gaze_margin;
                gc.intra_threads = oc.analysis_threads;
                gaze_estimator_ = std::make_unique<analysis::GazeEstimator>(oc.gaze_model, gc);
            } catch (const std::exception& e) {
            }
        }
        if (oc.enable_au && std::filesystem::is_regular_file(oc.au_model)) {
            try {
                analysis::AuAnalyzer::Config ac;
                ac.margin = oc.au_margin;
                ac.threshold = oc.au_threshold;
                ac.intra_threads = oc.analysis_threads;
                ac.per_au_thresholds = oc.au_thresholds;
                au_analyzer_ = std::make_unique<analysis::AuAnalyzer>(oc.au_model, ac);
            } catch (const std::exception& e) {
            }
        }
        if (!pose_estimator_ && !gaze_estimator_ && !au_analyzer_) {
        }
    }

    capture_.open(cfg_.input_video);
    if (!capture_.isOpened()) {
        init_error_ = "VIDEO_OPEN_FAIL|无法打开视频: " + cfg_.input_video;
        return false;
    }

    video_fps_ = capture_.get(cv::CAP_PROP_FPS);
    if (video_fps_ <= 0) video_fps_ = 30.0;
    total_frames_ = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_COUNT));
    frame_width_ = capture_.get(cv::CAP_PROP_FRAME_WIDTH);

    if (cfg_.resume_from_frame > 0) {
        capture_.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(cfg_.resume_from_frame));
        frame_idx_ = cfg_.resume_from_frame;
    }
    frame_height_ = capture_.get(cv::CAP_PROP_FRAME_HEIGHT);

    if (!cfg_.output_video.empty()) {
        std::filesystem::path out_path(cfg_.output_video);
        if (out_path.has_parent_path()) std::filesystem::create_directories(out_path.parent_path());

        const std::string& codec = cfg_.output_codec;
        int fourcc = (codec.size() == 4)
                         ? cv::VideoWriter::fourcc(codec[0], codec[1], codec[2], codec[3])
                         : cv::VideoWriter::fourcc('a', 'v', 'c', '1');
        writer_.open(cfg_.output_video, fourcc, video_fps_,
                     cv::Size(static_cast<int>(frame_width_), static_cast<int>(frame_height_)));
        if (!writer_.isOpened()) {
            fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
            writer_.open(cfg_.output_video, fourcc, video_fps_,
                         cv::Size(static_cast<int>(frame_width_), static_cast<int>(frame_height_)));
        }
    }
    if (!cfg_.tracking_csv.empty()) {
        std::filesystem::path csv_path(cfg_.tracking_csv);
        if (csv_path.has_parent_path()) std::filesystem::create_directories(csv_path.parent_path());
        tracking_csv_.open(cfg_.tracking_csv);
        tracking_csv_ << "frame,time_sec,faces,target,track_id,similarity,openface_row\n";
    }
    if (!cfg_.onnx_analysis_csv.empty() && cfg_.backend == AnalysisBackend::kOnnx) {
        std::filesystem::path csv_path(cfg_.onnx_analysis_csv);
        if (csv_path.has_parent_path()) std::filesystem::create_directories(csv_path.parent_path());
        analysis_csv_.open(cfg_.onnx_analysis_csv);

        analysis_csv_ << "frame,time_sec,pitch,yaw,roll,gaze_yaw,gaze_pitch";
        for (const auto& name : analysis::kMainAuNames) analysis_csv_ << "," << name;
        analysis_csv_ << "\n";
    }

    if (openface_) {
        double fx = cfg_.openface.focal_scale * (std::max(frame_width_, frame_height_) / 640.0);
        double cx = frame_width_ / 2.0;
        double cy = frame_height_ / 2.0;

        std::filesystem::create_directories(cfg_.openface.output_dir);
        openface_->StartSequence(cfg_.openface.output_name, cfg_.openface.output_dir, fx, fx, cx,
                                 cy, video_fps_);

        openface_->SetVisualization(cfg_.openface.vis_track, cfg_.openface.vis_aus,
                                    cfg_.openface.vis_hog, cfg_.openface.vis_align);
    }

    if (cfg_.show_window) {
        cv::namedWindow(cfg_.window_name, cv::WINDOW_NORMAL);
    }

    return true;
}

void Pipeline::Run() {
    while (true) {
        cv::Mat result = ProcessNextFrame();
        if (result.empty()) break;
    }
}

cv::Mat Pipeline::ProcessNextFrame() {
    cv::Mat frame;
    if (!capture_.read(frame) || frame.empty()) return {};

    last_raw_frame_ = frame.clone();

    double timestamp = static_cast<double>(frame_idx_) / video_fps_;

    auto detections = detector_->Detect(frame);

    std::vector<tracking::ByteTrackEngine::Detection> engine_dets;
    engine_dets.reserve(detections.size());
    for (const auto& d : detections)
        engine_dets.push_back({{d.x1, d.y1, d.x2 - d.x1, d.y2 - d.y1}, d.confidence});
    auto tracks = tracker_->Update(engine_dets);

    std::vector<TrackedFace> tracked_faces;
    tracked_faces.reserve(tracks.size());
    for (const auto& t : tracks)
        tracked_faces.push_back(
            {t.id,
             {t.tlwh[0], t.tlwh[1], t.tlwh[0] + t.tlwh[2], t.tlwh[1] + t.tlwh[3], t.score, {}},
             t.hit_streak,
             t.miss_streak});

    std::vector<MatchResult> match_results;
    int best_match_idx = -1;
    float best_match_sim = cfg_.match_threshold;
    ProcessFrameCore(frame, detections, tracked_faces, match_results, best_match_idx,
                     best_match_sim);

    bool target_found =
        UpdateTargetState(tracked_faces, match_results, best_match_idx, best_match_sim);

    last_metrics_ = FrameMetrics{};
    last_metrics_.frame = frame_idx_;
    last_metrics_.target_found = target_found;
    last_metrics_.target_track_id = target_track_id_;
    for (const auto& m : match_results) {
        last_metrics_.tracks.push_back({m.tracked_id, m.similarity,
                                        m.tracked_id == target_track_id_, m.box.x1, m.box.y1,
                                        m.box.x2, m.box.y2});
        if (m.tracked_id == target_track_id_) last_metrics_.similarity = m.similarity;
    }

    viz::Visualizer::DrawAnnotations(frame, tracked_faces, match_results, target_track_id_);

    cv::Mat display_frame = frame;
    int openface_frame_for_row = -1;

    if (target_found) {
        if (openface_) {
            cv::Mat vis = AnalyzeWithOpenFace(frame, timestamp);
            openface_frame_for_row = openface_frame_count_ - 1;
            if (!vis.empty()) display_frame = vis;

            const auto& r = openface_->LastResult();
            if (r.valid) {
                last_metrics_.pose_valid = true;
                last_metrics_.pitch = r.pitch;
                last_metrics_.yaw = r.yaw;
                last_metrics_.roll = r.roll;
                last_metrics_.gaze_valid = true;
                last_metrics_.gaze_yaw = r.gaze_yaw;
                last_metrics_.gaze_pitch = r.gaze_pitch;
                last_metrics_.aus = r.aus;
            }
        } else {
            AnalyzeWithOnnx(frame);
        }
    }

    if (cfg_.show_window) {
        cv::imshow(cfg_.window_name, display_frame);
        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) return {};
    }
    if (openface_ && openface_->IsQuitRequested()) return {};

    if (writer_.isOpened()) writer_.write(display_frame);
    if (tracking_csv_.is_open()) {
        tracking_csv_ << frame_idx_ << "," << timestamp << "," << detections.size() << ","
                      << (target_found ? 1 : 0) << "," << target_track_id_ << "," << best_match_sim
                      << "," << openface_frame_for_row << "\n";
    }
    if (analysis_csv_.is_open() && target_found) {
        analysis_csv_ << frame_idx_ << "," << timestamp;

        if (last_metrics_.pose_valid)
            analysis_csv_ << "," << last_metrics_.pitch << "," << last_metrics_.yaw << ","
                          << last_metrics_.roll;
        else
            analysis_csv_ << ",,,";

        if (last_metrics_.gaze_valid)
            analysis_csv_ << "," << last_metrics_.gaze_yaw << "," << last_metrics_.gaze_pitch;
        else
            analysis_csv_ << ",,";

        std::unordered_map<std::string, float> au_map;
        for (const auto& [name, val] : last_metrics_.aus) au_map[name] = val;
        for (size_t i = 0; i < analysis::kMainAuNames.size(); ++i) {
            auto it = au_map.find(std::string(analysis::kMainAuNames[i]));
            analysis_csv_ << "," << (it != au_map.end() ? it->second : 0.0f);
        }
        analysis_csv_ << "\n";
    }

    frame_idx_++;

    return display_frame;
}

void Pipeline::Shutdown() {
    if (openface_) openface_->EndSequence();
    capture_.release();
    if (writer_.isOpened()) writer_.release();
    if (tracking_csv_.is_open()) tracking_csv_.close();
    if (analysis_csv_.is_open()) analysis_csv_.close();
    cv::destroyAllWindows();
}

bool Pipeline::IsOpen() const {
    return capture_.isOpened();
}

}  // namespace omniface::pipeline
