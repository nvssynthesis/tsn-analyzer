//
// Created by Nicholas Solem on 3/5/26.
//

#pragma once
#include "dim_using.h"
#include <random>

namespace nvs::dim {

inline Real l2_norm(const vecReal &x) {
    Real result {0.0};
    for (const auto &xi : x) {
        result += xi * xi;
    }
    return std::sqrt(result);
}

inline Real euclidean_dist(const vecReal &x1, const vecReal &x2) {
    Real result {0.0};
    assert(x2.size() >= x1.size());
    for (size_t i = 0; i < x1.size(); ++i) {
        const auto d = (x1[i] - x2[i]);
        result += d * d;
    }
    return std::sqrt(result);
}


inline Real calculate_dist(const vecReal &x1, const vecReal &x2, Distance_e d=Distance_e::Euclidean) {
    if (d == Distance_e::Euclidean){
        return euclidean_dist(x1, x2);
    }
    assert (false);
}

enum class YinitMode {
    PCA,
    Random
};
struct YinitParam {
    YinitMode mode;
    std::optional<vecVecReal> user_supplied; // set if user provides their own Y
};
struct WeightPhases {
    idx_t phase_1_iters;
    idx_t phase_2_iters;
    idx_t phase_3_iters;
};
struct PacmapGradResult {
    vecVecReal grad;
    Real loss;
};

inline vecVecReal generateX() {
    vecVecReal X;
    std::mt19937 rng(42);
    std::normal_distribution<float> noise(0.0f, 0.3f);

    for (int i = 0; i < 50; ++i)
        X.push_back({1.0f + noise(rng), 2.0f + noise(rng),
                     3.0f + noise(rng), 4.0f + noise(rng)});
    for (int i = 0; i < 50; ++i)
        X.push_back({5.0f + noise(rng), 6.0f + noise(rng),
                     7.0f + noise(rng), 8.0f + noise(rng)});
    return X;
}

inline void print(const std::string_view s) {
    std::cout << s << std::endl;
}

inline void print(const std::vector<float> &v) {
    for (const auto &vi : v) {
        std::cout << vi << " ";
    }
    std::cout << std::endl;
}

inline void print(const std::vector<std::vector<float>> &V) {
    print("====================================");
    for (const auto& row : V) {
        print(row);
    }
    print("====================================");
}

}   // namespace nvs::dim
