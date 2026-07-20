#include "omniface/core/tracking/kalman_filter.h"

namespace omniface::tracking {

namespace {
Eigen::Matrix<float, 8, 8, Eigen::RowMajor> MakeMotionMat() {
    Eigen::Matrix<float, 8, 8, Eigen::RowMajor> m;
    m.setIdentity();
    m(0, 4) = 1.0f;
    m(1, 5) = 1.0f;
    m(2, 6) = 1.0f;
    m(3, 7) = 1.0f;
    return m;
}
const Eigen::Matrix<float, 8, 8, Eigen::RowMajor> kMotionMat = MakeMotionMat();
const Eigen::Matrix<float, 4, 8, Eigen::RowMajor> kUpdateMat =
    Eigen::Matrix<float, 4, 8>::Identity();
}  // namespace

KalmanFilter::KalmanFilter()
    : motion_mat_(kMotionMat),
      update_mat_(kUpdateMat),
      std_weight_position_(1.0f / 20.0f),
      std_weight_velocity_(1.0f / 160.0f) {}

KalData KalmanFilter::Initiate(const DetectBox& measurement) const {
    KalMean mean;
    mean << measurement(0), measurement(1), measurement(2), measurement(3), 0.0f, 0.0f, 0.0f, 0.0f;

    KalMean std;
    std(0) = 2.0f * std_weight_position_ * measurement(3);
    std(1) = 2.0f * std_weight_position_ * measurement(3);
    std(2) = 1e-2f;
    std(3) = 2.0f * std_weight_position_ * measurement(3);
    std(4) = 10.0f * std_weight_velocity_ * measurement(3);
    std(5) = 10.0f * std_weight_velocity_ * measurement(3);
    std(6) = 1e-5f;
    std(7) = 10.0f * std_weight_velocity_ * measurement(3);

    KalCova covariance = std.array().square().matrix().asDiagonal();
    return {mean, covariance};
}

void KalmanFilter::Predict(KalMean& mean, KalCova& covariance) const {
    DetectBox std_pos;
    std_pos << std_weight_position_ * mean(3), std_weight_position_ * mean(3), 1e-2f,
        std_weight_position_ * mean(3);

    DetectBox std_vel;
    std_vel << std_weight_velocity_ * mean(3), std_weight_velocity_ * mean(3), 1e-5f,
        std_weight_velocity_ * mean(3);

    KalMean noise(8);
    noise << std_pos(0), std_pos(1), std_pos(2), std_pos(3), std_vel(0), std_vel(1), std_vel(2),
        std_vel(3);

    KalCova motion_cov = noise.array().square().matrix().asDiagonal();

    KalMean new_mean = motion_mat_ * mean.transpose();
    KalCova new_cov = motion_mat_ * covariance * motion_mat_.transpose() + motion_cov;

    mean = new_mean;
    covariance = new_cov;
}

KalHdata KalmanFilter::Project(const KalMean& mean, const KalCova& covariance) const {
    DetectBox std;
    std << std_weight_position_ * mean(3), std_weight_position_ * mean(3), 1e-1f,
        std_weight_position_ * mean(3);

    KalHmean projected_mean = update_mat_ * mean.transpose();
    KalHcova projected_cov = update_mat_ * covariance * update_mat_.transpose();
    projected_cov += std.array().square().matrix().asDiagonal();

    return {projected_mean, projected_cov};
}

KalData KalmanFilter::Update(const KalMean& mean, const KalCova& covariance,
                             const DetectBox& measurement) const {
    auto [projected_mean, projected_cov] = Project(mean, covariance);

    Eigen::Matrix<float, 4, 8> B = (covariance * update_mat_.transpose()).transpose();
    Eigen::Matrix<float, 8, 4> kalman_gain = projected_cov.llt().solve(B).transpose();

    Eigen::Matrix<float, 1, 4> innovation = measurement - projected_mean;

    KalMean new_mean = (mean.array() + (innovation * kalman_gain.transpose()).array()).matrix();

    KalCova new_cov = covariance - kalman_gain * projected_cov * kalman_gain.transpose();

    return {new_mean, new_cov};
}

float KalmanFilter::GatingDistance(const KalMean& mean, const KalCova& covariance,
                                   const DetectBox& measurement) const {
    auto [proj_mean, proj_cov] = Project(mean, covariance);
    Eigen::Matrix<float, 1, 4> diff = measurement - proj_mean;
    return diff * proj_cov.llt().solve(diff.transpose());
}

}  // namespace omniface::tracking
