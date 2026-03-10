//
// Created by Nicholas Solem on 3/5/26.
//

#pragma once


#include "dim_using.h"

#include <Eigen/Dense>
#include <algorithm>
#include <numeric>

namespace nvs::dim {

struct KNNResult {
    std::vector<vecIdx> indices; // (n_query, k)
    vecVecReal distances;       // (n_query, k), L2 distances (not squared)
};


inline KNNResult knn_search(
    const Eigen::MatrixXf& basis,  // (n_basis, dim)
    const Eigen::MatrixXf& query,  // (n_query, dim)
    const idx_t k,
    const bool exclude_self = true)
{
    const auto n_basis = static_cast<idx_t>(basis.rows());
    const auto n_query = static_cast<idx_t>(query.rows());
    assert(basis.cols() == query.cols());
    assert(k > 0);
    const idx_t k_fetch = exclude_self? k + 1 : k;
    assert(k_fetch < n_basis);

    KNNResult result;
    result.indices.resize(n_query, vecIdx(k));
    result.distances.resize(n_query, vecReal(k));

    // precompute squared norms for efficient L2 distance calculation using
    // ||a - b||^2 = ||a||^2 + ||b||^2 - 2*a·b
    Eigen::VectorXf basis_sq = basis.rowwise().squaredNorm();
    Eigen::VectorXf query_sq = query.rowwise().squaredNorm();

    #pragma omp parallel for
    for (idx_t i = 0; i < n_query; ++i) {
        // squared L2 distances from query[i] to all basis points
        Eigen::VectorXf sq_dists =
            basis_sq.array()
                - 2.0f * (basis * query.row(i).transpose()).array()
                    + query_sq(i);

        sq_dists = sq_dists.cwiseMax(0.0f); // prevent sqrt(small negative) from floating point imprecision

        // find k nearest via partial sort
        vecIdx idx(n_basis);
        std::iota(idx.begin(), idx.end(), 0);
        std::ranges::partial_sort(idx, idx.begin() + k_fetch,
                                  [&](const idx_t a, const idx_t b) {
                                      return sq_dists(a) < sq_dists(b);
                                  });

        idx_t out = 0;
        for (idx_t j = 0; j < k_fetch && out < k; ++j) {
            if (exclude_self && idx[j] == j) { continue; }
            result.indices[i][out] = idx[j];
            result.distances[i][out] = std::sqrt(sq_dists(idx[j]));
            ++out;
        }
    }
    return result;
}

inline KNNResult compute_nearest_neighbors(
    const vecVecReal& X,
    const idx_t n_neighbors)
{
/*
     note: tree is not returned here; knn_search is stateless, so nothing to cache.
     if generate_extra_pair_basis is called later with the same basis, it will just
     rerun knn_search.
     if this is bottleneck, try wrapping precomputed Eigen matrix in a shared struct.
*/
    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(n_neighbors > 0);
    assert(n_neighbors < n); // need at least n_neighbors + self

    // search for n_neighbors + 1 to account for index of self being part of results
    const idx_t k = n_neighbors + 1;
    const Eigen::MatrixXf M = to_eigen(X);

    return knn_search(M, M, k);
}

}   // namespace nvs::dim

