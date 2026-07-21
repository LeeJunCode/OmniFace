#include "omniface/core/recognition/face_matcher.h"

#include <cmath>

namespace omniface::recognition {

namespace {

// 余弦相似度：ref 已预归一化，probe 运行时归一化
inline float CosineSimilarity(const std::array<float, 512>& normed_ref,
                              const std::array<float, 512>& probe) {
    float dot = 0.0f, probe_norm_sq = 0.0f;
    for (int i = 0; i < 512; ++i) {
        dot += normed_ref[i] * probe[i];
        probe_norm_sq += probe[i] * probe[i];
    }
    return (probe_norm_sq > 1e-24f) ? dot / std::sqrt(probe_norm_sq) : 0.0f;
}

}  // namespace

void FaceMatcher::SetReference(FaceEmbedding ref) {
    float nrm_sq = 0.0f;
    for (auto f : ref.features) nrm_sq += f * f;
    float inv_nrm = (nrm_sq > 1e-24f) ? 1.0f / std::sqrt(nrm_sq) : 1.0f;
    for (auto& f : ref.features) f *= inv_nrm;
    normed_reference_ = ref.features;
    has_reference_ = true;
}

bool FaceMatcher::HasReference() const {
    return has_reference_;
}

MatchResult FaceMatcher::Match(const FaceEmbedding& probe, const BoundingBox& box,
                               float threshold) const {
    MatchResult result;
    result.box = box;

    if (!has_reference_ || !probe.valid) {
        return result;
    }

    float similarity = CosineSimilarity(normed_reference_, probe.features);
    result.similarity = similarity;
    result.is_match = (similarity >= threshold);
    result.valid = true;
    return result;
}

}  // namespace omniface::recognition
