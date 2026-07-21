#pragma once

#include <array>

#include "omniface/types.h"

namespace omniface::recognition {

// 目标人物匹配器：预存归一化参考向量，余弦相似度匹配
class FaceMatcher {
public:
    FaceMatcher() = default;

    // 设置目标参考 embedding 并 L2 归一化
    void SetReference(FaceEmbedding ref);

    bool HasReference() const;

    // 计算 probe 与参考向量的余弦相似度
    MatchResult Match(const FaceEmbedding& probe, const BoundingBox& box,
                      float threshold = 0.5f) const;

private:
    std::array<float, 512> normed_reference_{};  // 已 L2 归一化
    bool has_reference_ = false;
};

}  // namespace omniface::recognition
