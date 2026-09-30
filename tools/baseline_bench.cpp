// Headless benchmark for the mid-term comparison experiment.
//
// Two modes, same video / reference / execution provider:
//
//   baseline — per-frame independent recognition. Every face SCRFD detects in
//              every frame gets its own forward pass, so the number of
//              recognition forwards is sum_t |D_t|, i.e. it grows with the
//              product of video length and face count.
//
//   tlaiv    — the production decision layer (TargetTracker::ProcessMatches,
//              the home of TLAIV). Recognition forwards are M + T/I: bounded by
//              the track count instead of the frame count.
//
// Both modes report the same three per-stage timings and the recognition
// forward count, so the two rows of the comparison table are directly
// comparable. No accuracy metric is produced — the codebase has no ground
// truth to measure against.

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include <spdlog/spdlog.h>

#include "omniface/core/detection/face_detector.h"
#include "omniface/core/recognition/align.h"
#include "omniface/core/recognition/face_matcher.h"
#include "omniface/core/recognition/face_recognizer.h"
#include "omniface/core/tracking/face_tracker.h"
#include "omniface/pipeline/framework/config.h"
#include "omniface/pipeline/workers/target_tracker.h"
#include "omniface/types.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace omniface {
namespace {

struct Options {
    std::string video;
    std::string reference;
    std::string config = "config.yaml";
    std::string mode = "baseline";
    std::string out;
    int recheck_interval = -1;  // -1 keeps the value from the config file
    int max_frames = 0;         // 0 runs the whole video
    int start_frame = 0;        // 0-based offset; seek here before decoding
    // 1-based frame numbers to save as JPEG and then exit. Lets a run's
    // judgment be checked against the pixels it was made on.
    std::vector<int> dump_frames;
    std::string dump_dir;
};

struct FrameStats {
    int frame = 0;
    int faces = 0;        // detected faces this frame
    int tracks = 0;       // active tracks this frame
    int recog_calls = 0;  // recognition forward passes this frame
    // 1 if the reference person was judged present this frame. baseline: at
    // least one detection matched on its own. tlaiv: a target track was
    // confirmed, which includes frames served from the track cache.
    int matched = 0;
    float best_sim = 0;   // best similarity among this frame's target matches
    double det_ms = 0;
    double track_ms = 0;
    double recog_ms = 0;
    double read_ms = 0;   // VideoCapture::read, i.e. container demux + decode
};

double Ms(Clock::duration d) {
    return std::chrono::duration<double, std::milli>(d).count();
}

void PrintUsage() {
    std::cout <<
        "usage: omniface_baseline --video <file> --reference <face image>\n"
        "                         [--config <yaml>] [--mode baseline|tlaiv]\n"
        "                         [--out <csv>] [--recheck-interval N]\n"
        "                         [--max-frames N] [--start-frame N]\n"
        "                         [--dump-frame N]... [--dump-dir DIR]\n"
        "\n"
        "  --mode baseline   per-frame independent recognition of every detection\n"
        "  --mode tlaiv      the production TLAIV decision layer (default: baseline)\n"
        "  --recheck-interval N  override behavior.target_recheck_interval after\n"
        "                        loading the config; 0 disables rechecks entirely\n"
        "                        (the I = infinity / Theta(M) endpoint). Needed\n"
        "                        because the loader clamps the config value to\n"
        "                        >= 1, so a 0 in the yaml file cannot survive.\n"
        "  --start-frame N   seek to 0-based frame N before decoding. Combined with\n"
        "                    --max-frames this benchmarks one slice of a long video\n"
        "                    without splitting the file. Frame numbers in the CSV\n"
        "                    stay absolute, so slices do not overlap. The tracker\n"
        "                    and decision layer start cold at the slice boundary.\n"
        "  --dump-frame N    save 1-based frame N as a JPEG into --dump-dir and exit\n"
        "                    without benchmarking. Repeatable. Frame numbers match\n"
        "                    the 'frame' column of the CSV.\n";
}

bool ParseArgs(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        auto next = [&](std::string& dst) {
            if (i + 1 >= argc) return false;
            dst = argv[++i];
            return true;
        };
        if (key == "--help" || key == "-h") { PrintUsage(); return false; }
        else if (key == "--video")     { if (!next(opt.video)) return false; }
        else if (key == "--reference") { if (!next(opt.reference)) return false; }
        else if (key == "--config")    { if (!next(opt.config)) return false; }
        else if (key == "--mode")      { if (!next(opt.mode)) return false; }
        else if (key == "--out")       { if (!next(opt.out)) return false; }
        else if (key == "--recheck-interval") {
            std::string v; if (!next(v)) return false;
            opt.recheck_interval = std::stoi(v);
        } else if (key == "--max-frames") {
            std::string v; if (!next(v)) return false;
            opt.max_frames = std::stoi(v);
        } else if (key == "--start-frame") {
            std::string v; if (!next(v)) return false;
            opt.start_frame = std::stoi(v);
        } else if (key == "--dump-frame") {
            std::string v; if (!next(v)) return false;
            opt.dump_frames.push_back(std::stoi(v));
        } else if (key == "--dump-dir") {
            if (!next(opt.dump_dir)) return false;
        } else {
            std::cerr << "unknown argument: " << key << "\n";
            PrintUsage();
            return false;
        }
    }
    if (opt.video.empty() || (opt.reference.empty() && opt.dump_frames.empty())) {
        PrintUsage();
        return false;
    }
    if (opt.mode != "baseline" && opt.mode != "tlaiv") {
        std::cerr << "--mode must be baseline or tlaiv\n";
        return false;
    }
    return true;
}

// The UI resolves relative model paths against the executable directory
// (processing_page.cpp ResolveModelPath). This is the Qt-free equivalent,
// extended with the config file's own directory.
std::string ResolveModelPath(const std::string& relative, const fs::path& exe_dir,
                             const fs::path& config_dir) {
    if (relative.empty()) return relative;
    fs::path rel(relative);
    if (rel.is_absolute()) return rel.string();

    const fs::path candidates[] = {
        exe_dir / rel,
        exe_dir / ".." / rel,
        exe_dir / ".." / ".." / rel,
        config_dir / rel,
        fs::current_path() / rel,
    };
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec)) return fs::weakly_canonical(c, ec).string();
    }
    return relative;
}

void ResolveModelPaths(PipelineConfig& cfg, const fs::path& exe_dir,
                       const fs::path& config_dir) {
    cfg.model.detector_model =
        ResolveModelPath(cfg.model.detector_model, exe_dir, config_dir);
    cfg.model.recognizer_model =
        ResolveModelPath(cfg.model.recognizer_model, exe_dir, config_dir);
}

// Mirrors MakeDetectorConfig (module_factory.h) without pulling in that
// header's analysis-backend includes.
detection::FaceDetector::Config MakeDetectorConfig(const PipelineConfig& cfg) {
    detection::FaceDetector::Config det;
    det.intra_threads = cfg.model.detection_threads;
    det.input_width = cfg.model.detection_input_width;
    det.input_height = cfg.model.detection_input_height;
    det.confidence_threshold = cfg.thresholds.detection_confidence;
    det.nms_threshold = cfg.thresholds.detection_nms;
    det.device = cfg.model.device;
    return det;
}

// Mirrors base_pipeline.cc UpdateThresholds; the engine's header defaults are
// always overridden from ThresholdConfig at runtime.
tracking::FaceTracker::Config MakeTrackConfig(const PipelineConfig& cfg) {
    tracking::FaceTracker::Config track;
    track.high_threshold = cfg.thresholds.tracking_high;
    track.match_threshold = cfg.thresholds.tracking_match;
    track.match_threshold_second = cfg.thresholds.tracking_match_second;
    track.new_track_threshold = cfg.thresholds.tracking_new;
    return track;
}

// Mirrors ExtractReferenceEmbedding (pipeline_factory.cc).
FaceEmbedding LoadReferenceEmbedding(const std::string& path,
                                     detection::FaceDetector& detector,
                                     recognition::FaceRecognizer& recognizer) {
    cv::Mat image = cv::imread(path);
    if (image.empty()) {
        spdlog::error("Failed to load reference image: {}", path);
        return {};
    }
    auto boxes = detector.Detect(image);
    if (boxes.empty()) {
        spdlog::error("No face detected in reference image: {}", path);
        return {};
    }
    const BoundingBox* best = &boxes[0];
    for (const auto& box : boxes) {
        if (box.confidence > best->confidence) best = &box;
    }
    spdlog::info("Reference face: confidence={:.2f}", best->confidence);
    return recognizer.Extract(recognition::AlignFace(image, *best));
}

// Opens the video, seeks to opt.start_frame, and reports the container's
// metadata so the slice a run covers is recorded alongside its numbers.
cv::VideoCapture OpenVideo(const Options& opt) {
    cv::VideoCapture capture(opt.video);
    if (!capture.isOpened()) {
        spdlog::error("Failed to open video: {}", opt.video);
        return capture;
    }
    const double frames = capture.get(cv::CAP_PROP_FRAME_COUNT);
    const double fps = capture.get(cv::CAP_PROP_FPS);
    std::cout.setf(std::ios::fixed);
    std::cout.precision(3);
    std::cout << "video            : " << opt.video << '\n'
              << "  dimensions     : " << capture.get(cv::CAP_PROP_FRAME_WIDTH) << 'x'
              << capture.get(cv::CAP_PROP_FRAME_HEIGHT) << '\n'
              << "  fps            : " << fps << '\n'
              << "  frame count    : " << frames << "   (duration "
              << (fps > 0.0 ? frames / fps : 0.0) << " s)\n";
    if (opt.start_frame > 0) {
        const bool sought = capture.set(cv::CAP_PROP_POS_FRAMES,
                                        static_cast<double>(opt.start_frame));
        std::cout << "  start frame    : " << opt.start_frame
                  << (sought ? "" : "  (seek reported failure)") << '\n';
    }
    if (opt.max_frames > 0)
        std::cout << "  max frames     : " << opt.max_frames << '\n';
    std::cout << std::flush;
    return capture;
}

// Saves the requested 1-based frames as JPEGs and returns true if it handled
// the run. Lets a judgment be checked against the pixels behind it: the CSV
// frame column is 1-based, so frame N is at 0-based offset N-1.
bool DumpFrames(const Options& opt) {
    if (opt.dump_frames.empty()) return false;

    const std::string dir = opt.dump_dir.empty() ? "." : opt.dump_dir;
    cv::VideoCapture capture(opt.video);
    if (!capture.isOpened()) {
        spdlog::error("Failed to open video: {}", opt.video);
        return true;
    }
    for (int n : opt.dump_frames) {
        if (n < 1) {
            spdlog::warn("skipping non-positive frame number {}", n);
            continue;
        }
        capture.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(n - 1));
        cv::Mat frame;
        if (!capture.read(frame) || frame.empty()) {
            spdlog::error("could not read frame {}", n);
            continue;
        }
        char name[64];
        std::snprintf(name, sizeof(name), "frame_%d.jpg", n);
        const std::string path = dir + "/" + name;
        if (cv::imwrite(path, frame))
            std::cout << "wrote " << path << '\n';
        else
            spdlog::error("failed to write {}", path);
    }
    return true;
}

struct Runner {
    const Options& opt;
    const PipelineConfig& cfg;
    detection::FaceDetector& detector;
    recognition::FaceRecognizer& recognizer;
    recognition::FaceMatcher& matcher;
};

// Per-frame independent recognition. The tracker still runs so that track_fps
// is comparable across modes, but its output never feeds a decision.
std::vector<FrameStats> RunBaseline(const Runner& r, std::set<int>& track_ids) {
    tracking::FaceTracker tracker(MakeTrackConfig(r.cfg));
    tracker.Reset();

    cv::VideoCapture capture = OpenVideo(r.opt);
    if (!capture.isOpened()) return {};

    std::vector<FrameStats> stats;
    cv::Mat frame;
    int frame_no = r.opt.start_frame;  // absolute, so slices never overlap
    int processed = 0;
    while (true) {
        auto t_read0 = Clock::now();
        bool ok = capture.read(frame) && !frame.empty();
        auto t_read1 = Clock::now();
        if (!ok) break;
        if (r.opt.max_frames > 0 && processed >= r.opt.max_frames) break;
        ++frame_no;
        ++processed;

        FrameStats s;
        s.frame = frame_no;
        s.read_ms = Ms(t_read1 - t_read0);

        auto t0 = Clock::now();
        auto detections = r.detector.Detect(frame);
        auto t1 = Clock::now();

        auto tracked = tracker.Update(detections);
        auto t2 = Clock::now();

        for (const auto& tf : tracked) track_ids.insert(tf.id);

        if (!detections.empty()) {
            std::vector<cv::Mat> batch;
            batch.reserve(detections.size());
            for (const auto& box : detections)
                batch.push_back(recognition::AlignFace(frame, box));

            auto embeddings = r.recognizer.ExtractBatch(batch);
            for (size_t j = 0; j < embeddings.size(); ++j) {
                auto match = r.matcher.Match(embeddings[j], detections[j],
                                             r.cfg.thresholds.match_threshold);
                if (match.is_match) {
                    s.matched = 1;
                    if (match.similarity > s.best_sim) s.best_sim = match.similarity;
                }
            }
            s.recog_calls = static_cast<int>(batch.size());
        }
        auto t3 = Clock::now();

        s.faces = static_cast<int>(detections.size());
        s.tracks = static_cast<int>(tracked.size());
        s.det_ms = Ms(t1 - t0);
        s.track_ms = Ms(t2 - t1);
        s.recog_ms = Ms(t3 - t2);
        stats.push_back(s);
    }
    return stats;
}

// Drives the production decision layer. Only the pipeline's call order is
// reproduced here (base_pipeline.cc ProcessFrame); the TLAIV logic itself is
// entirely inside TargetTracker, so nothing about the algorithm is reimplemented.
std::vector<FrameStats> RunTlaiv(const Runner& r, std::set<int>& track_ids) {
    tracking::FaceTracker tracker(MakeTrackConfig(r.cfg));
    tracker.Reset();
    TargetTracker decision(r.cfg);

    cv::VideoCapture capture = OpenVideo(r.opt);
    if (!capture.isOpened()) return {};

    std::vector<FrameStats> stats;
    cv::Mat frame;
    int frame_no = r.opt.start_frame;  // absolute, so slices never overlap
    int processed = 0;
    while (true) {
        auto t_read0 = Clock::now();
        bool ok = capture.read(frame) && !frame.empty();
        auto t_read1 = Clock::now();
        if (!ok) break;
        if (r.opt.max_frames > 0 && processed >= r.opt.max_frames) break;
        ++frame_no;
        ++processed;
        decision.SetFrameIndex(frame_no);

        FrameStats s;
        s.frame = frame_no;
        s.read_ms = Ms(t_read1 - t_read0);

        auto t0 = Clock::now();
        auto detections = r.detector.Detect(frame);
        auto t1 = Clock::now();

        auto tracked = tracker.Update(detections);
        auto t2 = Clock::now();

        for (const auto& tf : tracked) track_ids.insert(tf.id);

        std::vector<MatchResult> matches;
        int best_idx = -1;
        float best_sim = r.cfg.thresholds.match_threshold;
        int calls = 0;
        if (!tracked.empty()) {
            decision.ProcessMatches(frame, detections, tracked, &r.recognizer,
                                    &r.matcher, matches, best_idx, best_sim, calls);
        }
        decision.UpdateTargetState(tracked, matches, best_idx, best_sim);
        auto t3 = Clock::now();

        s.faces = static_cast<int>(detections.size());
        s.tracks = static_cast<int>(tracked.size());
        s.recog_calls = calls;
        s.matched = (best_idx >= 0) ? 1 : 0;
        s.best_sim = best_sim;
        s.det_ms = Ms(t1 - t0);
        s.track_ms = Ms(t2 - t1);
        s.recog_ms = Ms(t3 - t2);
        stats.push_back(s);
    }
    return stats;
}

template <typename T>
double Mean(const std::vector<FrameStats>& stats, T FrameStats::*field) {
    if (stats.empty()) return 0.0;
    double sum = 0.0;
    for (const auto& s : stats) sum += static_cast<double>(s.*field);
    return sum / static_cast<double>(stats.size());
}

double Fps(double mean_ms) {
    return mean_ms > 0.0 ? 1000.0 / mean_ms : 0.0;
}

// Everything derived from the per-frame stats. recog_fps is the per-frame
// stage throughput (1000 / mean recog_ms over ALL frames) and is misleading
// whenever the stage idles on most frames: TLAIV skips recognition entirely on
// frames with no pending track, so those near-zero samples drag the mean down
// and inflate the rate. recog_fps_active restricts the mean to frames that
// actually recognized something, and ms_per_call is the quantity that is
// comparable across modes because it does not depend on how often the stage
// runs at all.
struct Summary {
    long long total_calls = 0;
    long long matched_frames = 0;
    long long active_frames = 0;
    float peak_sim = 0.0f;
    double mean_faces = 0;
    double mean_tracks = 0;
    double det_ms = 0;
    double track_ms = 0;
    double recog_ms = 0;
    double recog_ms_active = 0;
    double read_ms = 0;
    double pipeline_fps = 0;
    double recog_fps = 0;
    double recog_fps_active = 0;
    double ms_per_call = 0;
    double calls_per_frame = 0;
    double forwards_per_sec = 0;
};

Summary Summarize(const std::vector<FrameStats>& stats, double wall_seconds) {
    Summary s;
    if (stats.empty()) return s;

    double recog_ms_active_sum = 0.0;
    for (const auto& f : stats) {
        s.total_calls += f.recog_calls;
        s.matched_frames += f.matched;
        if (f.recog_calls > 0) {
            ++s.active_frames;
            recog_ms_active_sum += f.recog_ms;
        }
        if (f.best_sim > s.peak_sim) s.peak_sim = f.best_sim;
    }

    const double frames = static_cast<double>(stats.size());
    s.mean_faces = Mean(stats, &FrameStats::faces);
    s.mean_tracks = Mean(stats, &FrameStats::tracks);
    s.det_ms = Mean(stats, &FrameStats::det_ms);
    s.track_ms = Mean(stats, &FrameStats::track_ms);
    s.recog_ms = Mean(stats, &FrameStats::recog_ms);
    s.read_ms = Mean(stats, &FrameStats::read_ms);
    s.recog_ms_active = s.active_frames > 0
                            ? recog_ms_active_sum / static_cast<double>(s.active_frames)
                            : 0.0;
    s.pipeline_fps = wall_seconds > 0.0 ? frames / wall_seconds : 0.0;
    s.recog_fps = Fps(s.recog_ms);
    s.recog_fps_active = Fps(s.recog_ms_active);
    s.calls_per_frame = frames > 0.0 ? static_cast<double>(s.total_calls) / frames : 0.0;
    // Summed over all frames, so this is the true cost per forward regardless
    // of how the work is distributed across frames.
    s.ms_per_call = s.total_calls > 0
                        ? (s.recog_ms * frames) / static_cast<double>(s.total_calls)
                        : 0.0;
    s.forwards_per_sec =
        wall_seconds > 0.0 ? static_cast<double>(s.total_calls) / wall_seconds : 0.0;
    return s;
}

void WriteCsv(const std::string& path, const Options& opt,
              const std::vector<FrameStats>& stats, int effective_interval,
              double wall_seconds, int track_ids) {
    std::ofstream out(path);
    if (!out) {
        spdlog::error("Failed to open output csv: {}", path);
        return;
    }
    out.setf(std::ios::fixed);

    out << "frame,faces,tracks,recog_calls,matched,best_sim,read_ms,det_ms,track_ms,recog_ms\n";
    for (const auto& s : stats) {
        out << s.frame << ',' << s.faces << ',' << s.tracks << ',' << s.recog_calls
            << ',' << s.matched << ',' << s.best_sim
            << ',' << s.read_ms << ',' << s.det_ms << ',' << s.track_ms << ',' << s.recog_ms
            << '\n';
    }

    Summary sum = Summarize(stats, wall_seconds);

    // '#'-prefixed so pandas and friends can skip it with comment='#'
    out << "# mode,recheck_interval,frames,mean_faces,mean_tracks,total_track_ids,"
           "matched_frames,active_frames,total_calls,calls_per_frame,ms_per_call,"
           "forwards_per_sec,pipeline_fps,read_ms,det_ms,track_ms,recog_ms,stage_sum_ms,"
           "det_fps,track_fps,recog_fps,recog_fps_active,peak_sim\n"
        << "# " << opt.mode << ',' << effective_interval << ',' << stats.size() << ','
        << sum.mean_faces << ',' << sum.mean_tracks << ','
        << track_ids << ',' << sum.matched_frames << ',' << sum.active_frames << ','
        << sum.total_calls << ',' << sum.calls_per_frame << ',' << sum.ms_per_call << ','
        << sum.forwards_per_sec << ','
        << sum.pipeline_fps << ',' << sum.read_ms << ',' << sum.det_ms << ','
        << sum.track_ms << ',' << sum.recog_ms << ','
        << (sum.det_ms + sum.track_ms + sum.recog_ms) << ','
        << Fps(sum.det_ms) << ',' << Fps(sum.track_ms) << ','
        << sum.recog_fps << ',' << sum.recog_fps_active << ',' << sum.peak_sim << '\n';
}

void PrintSummary(const Options& opt, const std::vector<FrameStats>& stats,
                  int effective_interval, double wall_seconds, int track_ids) {
    if (stats.empty()) {
        std::cout << "no frames processed\n";
        return;
    }
    Summary s = Summarize(stats, wall_seconds);

    std::cout.setf(std::ios::fixed);
    std::cout.precision(2);
    std::cout << "\nmode             : " << opt.mode << '\n'
              << "recheck interval : " << effective_interval
              << (effective_interval == 0 ? "  (rechecks off)" : "") << '\n'
              << "frames           : " << stats.size() << '\n'
              << "mean faces/frame : " << s.mean_faces << '\n'
              << "mean tracks/frame: " << s.mean_tracks << '\n'
              << "total track ids  : " << track_ids << "   (M)\n"
              << "matched frames   : " << s.matched_frames
              << "   (target person judged present)\n"
              << "peak similarity  : " << s.peak_sim << '\n'
              << "\npipeline FPS     : " << s.pipeline_fps << '\n'
              << "video read       : " << s.read_ms << " ms/frame  (demux + decode)\n"
              << "detect   FPS     : " << Fps(s.det_ms) << "   (" << s.det_ms << " ms)\n"
              << "track    FPS     : " << Fps(s.track_ms) << "   (" << s.track_ms << " ms)\n"
              << "stage sum        : " << (s.read_ms + s.det_ms + s.track_ms + s.recog_ms)
              << " ms/frame  (vs pipeline "
              << (s.pipeline_fps > 0.0 ? 1000.0 / s.pipeline_fps : 0.0) << " ms)\n"
              << "\nrecognize stage  : active on " << s.active_frames << " / "
              << stats.size() << " frames\n"
              << "  mean recog_ms  : " << s.recog_ms << " ms/frame over all frames  -> "
              << s.recog_fps << " FPS  (idle frames included)\n"
              << "  mean recog_ms  : " << s.recog_ms_active
              << " ms/frame over active frames -> " << s.recog_fps_active << " FPS\n"
              << "  ms per forward : " << s.ms_per_call
              << "   (mode-independent; the comparable recognition cost)\n"
              << "\nrecognition calls: " << s.total_calls
              << "   (" << s.calls_per_frame << " per frame, "
              << s.forwards_per_sec << " forwards/s)\n";
}

}  // namespace
}  // namespace omniface

using namespace omniface;

int main(int argc, char** argv) {
    Options opt;
    if (!ParseArgs(argc, argv, opt)) return 1;

    if (DumpFrames(opt)) return 0;

    spdlog::set_level(spdlog::level::warn);

    const fs::path exe_dir = fs::absolute(argv[0]).parent_path();
    const fs::path config_path = fs::absolute(opt.config);

    PipelineConfig cfg;
    try {
        cfg = LoadPipelineConfig(config_path.string());
    } catch (const std::exception& e) {
        std::cerr << "failed to load config " << config_path << ": " << e.what() << "\n";
        return 1;
    }

    // After loading: LoadPipelineConfig clamps target_recheck_interval to
    // [1, 300], so a 0 written in the yaml cannot survive the load. The
    // override is what makes the I = infinity / Theta(M) endpoint reachable.
    if (opt.recheck_interval >= 0)
        cfg.behavior.target_recheck_interval = opt.recheck_interval;

    ResolveModelPaths(cfg, exe_dir, config_path.parent_path());

    if (!fs::exists(cfg.model.detector_model)) {
        std::cerr << "detector model not found: " << cfg.model.detector_model << "\n";
        return 1;
    }
    if (!fs::exists(cfg.model.recognizer_model)) {
        std::cerr << "recognizer model not found: " << cfg.model.recognizer_model << "\n";
        return 1;
    }

    detection::FaceDetector detector(cfg.model.detector_model, MakeDetectorConfig(cfg));

    recognition::FaceRecognizer::Config rec_cfg;
    rec_cfg.intra_threads = cfg.model.recognition_threads;
    rec_cfg.device = cfg.model.device;
    recognition::FaceRecognizer recognizer(cfg.model.recognizer_model, rec_cfg);

    FaceEmbedding reference = LoadReferenceEmbedding(opt.reference, detector, recognizer);
    if (!reference.valid) {
        std::cerr << "could not enroll a reference face from " << opt.reference << "\n";
        return 1;
    }

    recognition::FaceMatcher matcher;
    matcher.SetReference(reference);

    Runner runner{opt, cfg, detector, recognizer, matcher};

    auto wall_start = Clock::now();
    // Distinct track ids seen over the run = M, the quantity Theta(M) bounds.
    std::set<int> track_ids;
    std::vector<FrameStats> stats =
        (opt.mode == "tlaiv") ? RunTlaiv(runner, track_ids)
                              : RunBaseline(runner, track_ids);
    double wall_seconds = std::chrono::duration<double>(Clock::now() - wall_start).count();

    if (stats.empty()) {
        std::cerr << "no frames processed\n";
        return 1;
    }

    const int total_track_ids = static_cast<int>(track_ids.size());

    PrintSummary(opt, stats, cfg.behavior.target_recheck_interval, wall_seconds,
                 total_track_ids);
    if (!opt.out.empty()) {
        WriteCsv(opt.out, opt, stats, cfg.behavior.target_recheck_interval,
                 wall_seconds, total_track_ids);
        std::cout << "\nwrote " << opt.out << '\n';
    }
    return 0;
}
