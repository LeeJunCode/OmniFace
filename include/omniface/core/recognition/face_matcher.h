#pragma once

#include <array>

#include "omniface/types.h"

namespace omniface::recognition {

class FaceMatcher {
public:
    FaceMatcher() = default;

    void SetReference(FaceEmbedding ref);

    bool HasReference() const;

    MatchResult Match(const FaceEmbedding& probe, const BoundingBox& box,
                      float threshold = 0.5f) const;

private:
    std::array<float, 512> normed_reference_{};
    bool has_reference_ = false;
};

}  // namespace omniface::recognition
