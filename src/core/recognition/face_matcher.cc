#include "omniface/core/recognition/face_matcher.h"

#include <cmath>

namespace omniface::recognition {

namespace {

// 优化版余弦相似度: ref已预归一化(SetReference时L2归一化)，probe在线计算L2范数
// 等价于 dot(ref, probe) / ||ref|| / ||probe||，但 ||ref|| = 1，省去一次sqrt
inline float CosineSimilarity(const std::array<float, 512>& normed_ref,
                               const std::array<float, 512>& probe) {
    float dot = 0.0f, probe_norm_sq = 0.0f;
    for (int i = 0; i < 512; ++i) {
        dot += normed_ref[i] * probe[i];
        probe_norm_sq += probe[i] * probe[i];
    }
    // 1e-24: 防止浮点下溢导致的除零错误
    return (probe_norm_sq > 1e-24f) ? dot / std::sqrt(probe_norm_sq) : 0.0f;
}

}  // namespace

// 预归一化参考特征: L2归一化后将范数固定为1，后续匹配时无需重复计算参考特征的范数
void FaceMatcher::SetReference(FaceEmbedding ref) {
    float nrm_sq = 0.0f;
    for (auto f : ref.features) nrm_sq += f * f;
    // 1e-24: 防止全零向量的除零错误
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
