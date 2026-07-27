#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

#include <QImage>

namespace omniface::ui {

inline QImage MatToQImage(const cv::Mat& bgr, QImage& cache) {
    int w = bgr.cols, h = bgr.rows;
    if (cache.width() != w || cache.height() != h ||
        cache.format() != QImage::Format_RGB888) {
        cache = QImage(w, h, QImage::Format_RGB888);
    }
    cv::Mat rgb_view(h, w, CV_8UC3, cache.bits(), cache.bytesPerLine());
    cv::cvtColor(bgr, rgb_view, cv::COLOR_BGR2RGB);
    return cache;
}

}  // namespace omniface::ui
