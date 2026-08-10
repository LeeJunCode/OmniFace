#pragma once

#include <vector>

namespace omniface::tracking {

// Jonker-Volgenant 线性分配算法（稠密矩阵版本）
// 求解最小化代价的二分图匹配，匈牙利算法的改进变体，时间复杂度 O(n³)
// cost: 代价矩阵（行=检测，列=轨道），rows×cols
// x[i]=j 表示检测 i 匹配轨道 j，y[j]=i 表示轨道 j 匹配检测 i，-1 表示未匹配
// cost_limit: 代价超过此阈值的匹配被拒绝
int Lapjv(const float* cost, int rows, int cols, bool extend_cost, float cost_limit,
          std::vector<int>& x, std::vector<int>& y);

}  // namespace omniface::tracking
