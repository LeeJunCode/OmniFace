#pragma once

#include <Eigen/Cholesky>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <utility>

namespace omniface::tracking {

using KalMean = Eigen::Matrix<float, 1, 8, Eigen::RowMajor>;
using KalCova = Eigen::Matrix<float, 8, 8, Eigen::RowMajor>;
using KalHmean = Eigen::Matrix<float, 1, 4, Eigen::RowMajor>;
using KalHcova = Eigen::Matrix<float, 4, 4, Eigen::RowMajor>;
using DetectBox = Eigen::Matrix<float, 1, 4, Eigen::RowMajor>;
using KalData = std::pair<KalMean, KalCova>;
using KalHdata = std::pair<KalHmean, KalHcova>;

class KalmanFilter {
public:
    static constexpr double kChi2Inv95[10] = {0,      3.8415, 5.9915, 7.8147, 9.4877,
                                              11.070, 12.592, 14.067, 15.507, 16.919};

    KalmanFilter();

    KalData Initiate(const DetectBox& measurement) const;

    void Predict(KalMean& mean, KalCova& covariance) const;

    KalData Update(const KalMean& mean, const KalCova& covariance,
                   const DetectBox& measurement) const;

    KalHdata Project(const KalMean& mean, const KalCova& covariance) const;

    float GatingDistance(const KalMean& mean, const KalCova& covariance,
                         const DetectBox& measurement) const;

private:
    Eigen::Matrix<float, 8, 8, Eigen::RowMajor> motion_mat_;
    Eigen::Matrix<float, 4, 8, Eigen::RowMajor> update_mat_;
    float std_weight_position_;
    float std_weight_velocity_;
};

}  // namespace omniface::tracking
