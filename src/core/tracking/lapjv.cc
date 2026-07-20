#include "omniface/core/tracking/lapjv.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace omniface::tracking {
namespace {

constexpr float kLarge = 1000000.0f;

int CcrrtDense(int n, const float* cost, std::vector<int>& free_rows, std::vector<int>& x,
               std::vector<int>& y, std::vector<float>& v) {
    for (int i = 0; i < n; i++) {
        x[i] = -1;
        v[i] = kLarge;
        y[i] = 0;
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float c = cost[i * n + j];
            if (c < v[j]) {
                v[j] = c;
                y[j] = i;
            }
        }
    }
    std::vector<bool> unique(n, true);
    int j = n;
    do {
        j--;
        int i = y[j];
        if (x[i] < 0) {
            x[i] = j;
        } else {
            unique[i] = false;
            y[j] = -1;
        }
    } while (j > 0);

    int n_free_rows = 0;
    for (int i = 0; i < n; i++) {
        if (x[i] < 0) {
            free_rows[n_free_rows++] = i;
        } else if (unique[i]) {
            int jj = x[i];
            float min = kLarge;
            for (int j2 = 0; j2 < n; j2++) {
                if (j2 == jj) continue;
                float c = cost[i * n + j2] - v[j2];
                if (c < min) min = c;
            }
            v[jj] -= min;
        }
    }
    return n_free_rows;
}

int CarrDense(int n, const float* cost, int n_free_rows, std::vector<int>& free_rows,
              std::vector<int>& x, std::vector<int>& y, std::vector<float>& v) {
    int current = 0;
    int new_free_rows = 0;
    int rr_cnt = 0;

    while (current < n_free_rows) {
        rr_cnt++;
        int free_i = free_rows[current++];
        int j1 = 0;
        float v1 = cost[free_i * n + 0] - v[0];
        int j2 = -1;
        float v2 = kLarge;
        for (int j = 1; j < n; j++) {
            float c = cost[free_i * n + j] - v[j];
            if (c < v2) {
                if (c >= v1) {
                    v2 = c;
                    j2 = j;
                } else {
                    v2 = v1;
                    v1 = c;
                    j2 = j1;
                    j1 = j;
                }
            }
        }
        int i0 = y[j1];
        float v1_new = v[j1] - (v2 - v1);
        bool v1_lowers = v1_new < v[j1];

        if (rr_cnt < current * n) {
            if (v1_lowers) {
                v[j1] = v1_new;
            } else if (i0 >= 0 && j2 >= 0) {
                j1 = j2;
                i0 = y[j2];
            }
            if (i0 >= 0) {
                if (v1_lowers) {
                    free_rows[--current] = i0;
                } else {
                    free_rows[new_free_rows++] = i0;
                }
            }
        } else {
            if (i0 >= 0) {
                free_rows[new_free_rows++] = i0;
            }
        }
        x[free_i] = j1;
        y[j1] = free_i;
    }
    return new_free_rows;
}

int FindDense(int n, int lo, const std::vector<float>& d, std::vector<int>& cols,
              const std::vector<int>&) {
    int hi = lo + 1;
    float mind = d[cols[lo]];
    for (int k = hi; k < n; k++) {
        int j = cols[k];
        if (d[j] <= mind) {
            if (d[j] < mind) {
                hi = lo;
                mind = d[j];
            }
            cols[k] = cols[hi];
            cols[hi++] = j;
        }
    }
    return hi;
}

int ScanDense(int n, const float* cost, int* plo, int* phi, std::vector<float>& d,
              std::vector<int>& cols, std::vector<int>& pred, const std::vector<int>& y,
              std::vector<float>& v) {
    int lo = *plo;
    int hi = *phi;
    while (lo != hi) {
        int jj = cols[lo++];
        int i = y[jj];
        float mind = d[jj];
        float h = cost[i * n + jj] - v[jj] - mind;
        for (int k = hi; k < n; k++) {
            jj = cols[k];
            float cred_ij = cost[i * n + jj] - v[jj] - h;
            if (cred_ij < d[jj]) {
                d[jj] = cred_ij;
                pred[jj] = i;
                if (cred_ij == mind) {
                    if (y[jj] < 0) return jj;
                    cols[k] = cols[hi];
                    cols[hi++] = jj;
                }
            }
        }
    }
    *plo = lo;
    *phi = hi;
    return -1;
}

int FindPathDense(int n, const float* cost, int start_i, const std::vector<int>& y,
                  std::vector<float>& v, std::vector<int>& pred) {
    int lo = 0, hi = 0;
    int final_j = -1;
    int n_ready = 0;
    std::vector<int> cols(n);
    std::vector<float> d(n);

    for (int i = 0; i < n; i++) {
        cols[i] = i;
        pred[i] = start_i;
        d[i] = cost[start_i * n + i] - v[i];
    }

    while (final_j == -1) {
        if (lo == hi) {
            n_ready = lo;
            hi = FindDense(n, lo, d, cols, y);
            for (int k = lo; k < hi; k++) {
                int jj = cols[k];
                if (y[jj] < 0) final_j = jj;
            }
        }
        if (final_j == -1) {
            final_j = ScanDense(n, cost, &lo, &hi, d, cols, pred, y, v);
        }
    }

    float mind = d[cols[lo]];
    for (int k = 0; k < n_ready; k++) {
        int jj = cols[k];
        v[jj] += d[jj] - mind;
    }
    return final_j;
}

void CaDense(int n, const float* cost, int n_free_rows, std::vector<int>& free_rows,
             std::vector<int>& x, std::vector<int>& y, std::vector<float>& v) {
    std::vector<int> pred(n);
    for (int p = 0; p < n_free_rows; p++) {
        int i = -1;
        int jj = FindPathDense(n, cost, free_rows[p], y, v, pred);
        int k = 0;
        while (i != free_rows[p]) {
            i = pred[jj];
            y[jj] = i;
            std::swap(jj, x[i]);
            k++;
            if (k >= n) break;
        }
    }
}

}  // namespace

int Lapjv(const float* cost, int rows, int cols, bool extend_cost, float cost_limit,
          std::vector<int>& x, std::vector<int>& y) {
    int n = rows;
    if (extend_cost || rows != cols) {
        n = std::max(rows, cols);
    }

    std::vector<float> square_cost;
    if (n != rows || n != cols) {
        square_cost.resize(n * n, cost_limit);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                square_cost[r * n + c] = cost[r * cols + c];
            }
        }
    }
    const float* used_cost = (n != rows || n != cols) ? square_cost.data() : cost;

    std::vector<int> xn(n, -1);
    std::vector<int> yn(n, -1);
    std::vector<int> free_rows(n);
    std::vector<float> v(n);

    int ret = CcrrtDense(n, used_cost, free_rows, xn, yn, v);
    int loop = 0;
    while (ret > 0 && loop < 2) {
        ret = CarrDense(n, used_cost, ret, free_rows, xn, yn, v);
        loop++;
    }
    if (ret > 0) {
        CaDense(n, used_cost, ret, free_rows, xn, yn, v);
    }

    x.assign(rows, -1);
    y.assign(cols, -1);

    for (int r = 0; r < rows; r++) {
        if (xn[r] >= 0 && xn[r] < cols) {
            float val = cost[r * cols + xn[r]];
            if (val <= cost_limit) {
                x[r] = xn[r];
            }
        }
    }
    for (int c = 0; c < cols; c++) {
        if (yn[c] >= 0 && yn[c] < rows) {
            float val = cost[yn[c] * cols + c];
            if (val <= cost_limit) {
                y[c] = yn[c];
            }
        }
    }

    return 0;
}

}  // namespace omniface::tracking
