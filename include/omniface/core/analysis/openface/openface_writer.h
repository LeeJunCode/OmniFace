#pragma once

#include <fstream>
#include <memory>
#include <string>

#include <opencv2/core/mat.hpp>

#include "omniface/types.h"

namespace Utilities {
class RecorderOpenFace;
}

namespace omniface::analysis::openface {

class OpenFaceWriter {
public:
    OpenFaceWriter();
    ~OpenFaceWriter();

    OpenFaceWriter(const OpenFaceWriter&) = delete;
    OpenFaceWriter& operator=(const OpenFaceWriter&) = delete;

    void Initialize(const std::string& input_path, const std::string& output_dir,
                    double fps, bool output_aligned = true);
    void WriteObservation(const FaceAnalysisResult& analysis, int frame_index,
                          double fps, const cv::Mat& aligned_face);
    void Close();

private:
    std::unique_ptr<Utilities::RecorderOpenFace> recorder_;
    bool initialized_ = false;
    std::ofstream quality_csv_;
    bool eigenvalues_written_ = false;
};

}  // namespace omniface::analysis::openface
