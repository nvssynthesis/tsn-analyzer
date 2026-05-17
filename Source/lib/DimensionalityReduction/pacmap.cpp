//
// Created by Nicholas Solem on 3/6/26.
//

#include <numeric>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <random>
#include <thread>
#include <iomanip>
#include <stdexcept>
#include <iostream>

#include "pacmap.h"
#include "knn.h"

namespace nvs::dim {

thread_local std::mt19937 rng(
    std::random_device{}() ^
        static_cast<uint32_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()))
);

vecIdx sample_FP(
    const idx_t n_samples,
    const idx_t maximum,
    const vecIdx& reject_ind,
    const idx_t self_ind)
{
    assert(maximum > 0);
    assert(n_samples > 0);
    assert(n_samples <= maximum - 1 - static_cast<idx_t>(reject_ind.size()));

    std::uniform_int_distribution<idx_t> dist(0, maximum - 1);
    vecIdx result(n_samples);

    for (idx_t i = 0; i < n_samples; ++i) {
        idx_t j;
        while (true) {
            j = dist(rng);

            if (j == self_ind) continue;

            bool in_result = false;
            for (idx_t k = 0; k < i; ++k) {
                if (j == result[k]) { in_result = true; break; }
            }
            if (in_result) continue;

            bool in_reject = false;
            for (const idx_t r : reject_ind) {
                if (j == r) { in_reject = true; break; }
            }
            if (in_reject) continue;

            break;
        }
        result[i] = j;
    }
    return result;
}


/** sample_neighbors_pair:
 * Returns: a flat (n * n_neighbors, 2) array as a vecVecIdx
 */
std::vector<vecIdx> sample_neighbors_pair(
    const vecVecReal& X,
    const vecVecReal& scaled_dist,
    const std::vector<vecIdx>& neighbors,
    const idx_t n_neighbors)
{
    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(n_neighbors > 0);
    assert(static_cast<idx_t>(scaled_dist.size()) == n);
    assert(static_cast<idx_t>(neighbors.size()) == n);

    std::vector<vecIdx> pair_neighbors(n * n_neighbors, vecIdx(2));

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {
        assert(static_cast<idx_t>(scaled_dist[i].size()) >= static_cast<idx_t>(neighbors[i].size()));

        vecIdx scaled_sort(scaled_dist[i].size());
        std::iota(scaled_sort.begin(), scaled_sort.end(), 0);
        std::ranges::sort(scaled_sort,
                          [&](const idx_t a, const idx_t b) {
                              return scaled_dist[i][a] < scaled_dist[i][b];
                          });

        assert(n_neighbors <= static_cast<idx_t>(scaled_sort.size()));
        for (idx_t j = 0; j < n_neighbors; ++j) {
            assert(scaled_sort[j] < static_cast<idx_t>(neighbors[i].size()));
            pair_neighbors[i * n_neighbors + j][0] = i;
            pair_neighbors[i * n_neighbors + j][1] = neighbors[i][scaled_sort[j]];
        }
    }
    return pair_neighbors;
}


std::vector<vecIdx> sample_MN_pair(
    const vecVecReal& X,
    const idx_t n_MN,
    const Distance_e distance)
{
    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(n_MN > 0);

    std::vector<vecIdx> pair_MN(n * n_MN, vecIdx(2));

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {
        for (idx_t j = 0; j < n_MN; ++j) {

            // build reject_ind from already-filled entries pair_MN[i*n_MN .. i*n_MN+j-1][1]
            vecIdx reject_ind(j);
            for (idx_t k = 0; k < j; ++k)
                reject_ind[k] = pair_MN[i * n_MN + k][1];

            vecIdx sampled = sample_FP(6, n, reject_ind, i);
            assert(static_cast<idx_t>(sampled.size()) == 6);

            // compute distances for all 6 samples
            vecReal dist_list(6);
            for (idx_t t = 0; t < 6; ++t)
                dist_list[t] = calculate_dist(X[i], X[sampled[t]], distance);

            // find and remove the closest sample (keep the second-closest)
            const idx_t min_idx = static_cast<idx_t>(
                std::ranges::min_element(dist_list) - dist_list.begin());
            dist_list.erase(dist_list.begin() + min_idx);
            sampled.erase(sampled.begin() + min_idx);
            assert(static_cast<idx_t>(sampled.size()) == 5);

            //pick the closest of the remaining 5
            const idx_t picked_idx = static_cast<idx_t>(
                std::ranges::min_element(dist_list) - dist_list.begin());
            const idx_t picked = sampled[picked_idx];

            pair_MN[i * n_MN + j][0] = i;
            pair_MN[i * n_MN + j][1] = picked;
        }
    }
    return pair_MN;
}

std::vector<vecIdx> sample_MN_pair_deterministic(
    const vecVecReal& X,
    const idx_t n_MN,
    const Distance_e option = Distance_e::Euclidean)
{
    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(n_MN > 0);

    std::vector<vecIdx> pair_MN(n * n_MN, vecIdx(2));

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {
        for (idx_t j = 0; j < n_MN; ++j) {
            vecIdx reject_ind(j);
            for (idx_t k = 0; k < j; ++k)
                reject_ind[k] = pair_MN[i * n_MN + k][1];

            vecIdx sampled = sample_FP(6, n, reject_ind, i);
            assert(static_cast<idx_t>(sampled.size()) == 6);

            vecReal dist_list(6);
            for (idx_t t = 0; t < 6; ++t)
                dist_list[t] = calculate_dist(X[i], X[sampled[t]], option);

            const idx_t min_idx = static_cast<idx_t>(
                std::ranges::min_element(dist_list) - dist_list.begin());
            dist_list.erase(dist_list.begin() + min_idx);
            sampled.erase(sampled.begin() + min_idx);

            const idx_t picked_idx = static_cast<idx_t>(
                std::ranges::min_element(dist_list) - dist_list.begin());
            const idx_t picked = sampled[picked_idx];

            pair_MN[i * n_MN + j][0] = i;
            pair_MN[i * n_MN + j][1] = picked;
        }
    }
    return pair_MN;
}


std::vector<vecIdx> sample_FP_pair(
    const vecVecReal& X,
    const std::vector<vecIdx>& pair_neighbors,
    const idx_t n_neighbors,
    const idx_t n_FP)
{
    /*
        pair_neighbors and pair_FP are both stored as std::vector<vecIdx> with
        ner vectors of size 2, (important with lots of small heap allocations)
        If this is bottleneck, consider FLAT structure:
        std::vector<std::array<idx_t, 2>> would be more cache-friendly.
    */
    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(n_FP > 0);
    assert(n_neighbors > 0);
    assert(static_cast<idx_t>(pair_neighbors.size()) == n * n_neighbors);

    std::vector<vecIdx> pair_FP(n * n_FP, vecIdx(2));

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {

        // this extracts neighbor indices for point i as reject_ind
        vecIdx reject_ind(n_neighbors);
        for (idx_t k = 0; k < n_neighbors; ++k) {
            assert(static_cast<idx_t>(pair_neighbors[i * n_neighbors + k].size()) == 2);
            reject_ind[k] = pair_neighbors[i * n_neighbors + k][1];
        }

        vecIdx FP_index = sample_FP(n_FP, n, reject_ind, i);
        assert(static_cast<idx_t>(FP_index.size()) == n_FP);

        for (idx_t k = 0; k < n_FP; ++k) {
            pair_FP[i * n_FP + k][0] = i;
            pair_FP[i * n_FP + k][1] = FP_index[k];
        }
    }
    return pair_FP;
}

std::vector<vecIdx> sample_FP_pair_deterministic(
    const vecVecReal& X,
    const std::vector<vecIdx>& pair_neighbors,
    const idx_t n_neighbors,
    const idx_t n_FP)
{
    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(n_FP > 0);
    assert(n_neighbors > 0);
    assert(static_cast<idx_t>(pair_neighbors.size()) == n * n_neighbors);

    std::vector<vecIdx> pair_FP(n * n_FP, vecIdx(2));

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {
        vecIdx reject_ind(n_neighbors);
        for (idx_t k = 0; k < n_neighbors; ++k) {
            assert(static_cast<idx_t>(pair_neighbors[i * n_neighbors + k].size()) == 2);
            reject_ind[k] = pair_neighbors[i * n_neighbors + k][1];
        }

        vecIdx FP_index = sample_FP(n_FP, n, reject_ind, i);
        assert(static_cast<idx_t>(FP_index.size()) == n_FP);

        for (idx_t k = 0; k < n_FP; ++k) {
            pair_FP[i * n_FP + k][0] = i;
            pair_FP[i * n_FP + k][1] = FP_index[k];
        }
    }
    return pair_FP;
}

vecVecReal scale_dist(
    const vecVecReal& knn_distance,
    const vecReal& sig,
    const std::vector<vecIdx>& neighbors)
{
    const auto n = static_cast<idx_t>(knn_distance.size());
    assert(n > 0);
    assert(static_cast<idx_t>(sig.size()) == n);
    assert(static_cast<idx_t>(neighbors.size()) == n);

    const auto num_neighbors = static_cast<idx_t>(knn_distance[0].size());
    assert(num_neighbors > 0);

    vecVecReal scaled_dist(n, vecReal(num_neighbors, 0.0f));

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {
        assert(static_cast<idx_t>(knn_distance[i].size()) == num_neighbors);
        assert(static_cast<idx_t>(neighbors[i].size()) == num_neighbors);
        assert(sig[i] != 0.0f);

        for (idx_t j = 0; j < num_neighbors; ++j) {
            assert(neighbors[i][j] >= 0 && neighbors[i][j] < n);
            assert(sig[neighbors[i][j]] != 0.0f);
            scaled_dist[i][j] = knn_distance[i][j] * knn_distance[i][j]
                / sig[i] / sig[neighbors[i][j]];
        }
    }
    return scaled_dist;
}


void update_embedding_adam(
    vecVecReal& Y,
    const vecVecReal& grad,
    vecVecReal& m,
    vecVecReal& v,
    const Real beta1,
    const Real beta2,
    const Real lr,
    const idx_t itr)
{
    const auto n = static_cast<idx_t>(Y.size());
    assert(n > 0);
    assert(static_cast<idx_t>(grad.size()) == n);
    assert(static_cast<idx_t>(m.size()) == n);
    assert(static_cast<idx_t>(v.size()) == n);

    const auto dim = static_cast<idx_t>(Y[0].size());
    assert(dim > 0);
    assert(beta1 > 0.0f && beta1 < 1.0f);
    assert(beta2 > 0.0f && beta2 < 1.0f);

    const Real lr_t = lr * std::sqrt(1.0f - static_cast<float>(std::pow(beta2, itr + 1)))
                          / (1.0f - static_cast<float>(std::pow(beta1, itr + 1)));
    assert(std::isfinite(lr_t) && lr_t > 0.0f);

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {
        assert(static_cast<idx_t>(Y[i].size()) == dim);
        assert(static_cast<idx_t>(grad[i].size()) == dim);
        assert(static_cast<idx_t>(m[i].size()) == dim);
        assert(static_cast<idx_t>(v[i].size()) == dim);

        for (idx_t d = 0; d < dim; ++d) {
            m[i][d] += (1.0f - beta1) * (grad[i][d] - m[i][d]);
            v[i][d] += (1.0f - beta2) * (grad[i][d] * grad[i][d] - v[i][d]);
            Y[i][d] -= lr_t * m[i][d] / (std::sqrt(v[i][d]) + 1e-7f);
        }
    }
}


PacmapGradResult pacmap_grad(
    const vecVecReal& Y,
    const std::vector<vecIdx>& pair_neighbors,
    const std::vector<vecIdx>& pair_MN,
    const std::vector<vecIdx>& pair_FP,
    const Real w_neighbors,
    const Real w_MN,
    const Real w_FP)
{
/*
    parallelization opportunity: the three loops can't easily be parallelized
    as-is due to the grad[i] and grad[j] write conflicts.
    if we need parallel performance, standard approach would be per-thread
    gradient accumulators and reduce them at end
*/
    const auto n = static_cast<idx_t>(Y.size());
    assert(n > 0);
    const auto dim = static_cast<idx_t>(Y[0].size());
    assert(dim > 0);

    vecVecReal grad(n, vecReal(dim, 0.0f));
    Real loss0 = 0.0f, loss1 = 0.0f, loss2 = 0.0f;

    // NN
    for (const auto & pair_neighbor : pair_neighbors) {
        assert(static_cast<idx_t>(pair_neighbor.size()) == 2);
        const idx_t i = pair_neighbor[0];
        const idx_t j = pair_neighbor[1];
        assert(i >= 0 && i < n);
        assert(j >= 0 && j < n);

        vecReal y_ij(dim);
        Real d_ij = 1.0f;
        for (idx_t d = 0; d < dim; ++d) {
            y_ij[d] = Y[i][d] - Y[j][d];
            d_ij += y_ij[d] * y_ij[d];
        }
        loss0 += w_neighbors * (d_ij / (10.0f + d_ij));
        const Real w1 = w_neighbors * (20.0f / ((10.0f + d_ij) * (10.0f + d_ij)));
        for (idx_t d = 0; d < dim; ++d) {
            grad[i][d] += w1 * y_ij[d];
            grad[j][d] -= w1 * y_ij[d];
        }
    }

    // MN
    for (const auto & tt : pair_MN) {
        assert(static_cast<idx_t>(tt.size()) == 2);
        const idx_t i = tt[0];
        const idx_t j = tt[1];
        assert(i >= 0 && i < n);
        assert(j >= 0 && j < n);

        vecReal y_ij(dim);
        Real d_ij = 1.0f;
        for (idx_t d = 0; d < dim; ++d) {
            y_ij[d] = Y[i][d] - Y[j][d];
            d_ij += y_ij[d] * y_ij[d];
        }
        loss1 += w_MN * d_ij / (10000.0f + d_ij);
        const Real w = w_MN * 20000.0f / ((10000.0f + d_ij) * (10000.0f + d_ij));
        for (idx_t d = 0; d < dim; ++d) {
            grad[i][d] += w * y_ij[d];
            grad[j][d] -= w * y_ij[d];
        }
    }

    // FP
    for (const auto & ttt : pair_FP) {
        assert(static_cast<idx_t>(ttt.size()) == 2);
        const idx_t i = ttt[0];
        const idx_t j = ttt[1];
        assert(i >= 0 && i < n);
        assert(j >= 0 && j < n);

        vecReal y_ij(dim);
        Real d_ij = 1.0f;
        for (idx_t d = 0; d < dim; ++d) {
            y_ij[d] = Y[i][d] - Y[j][d];
            d_ij += y_ij[d] * y_ij[d];
        }
        loss2 += w_FP * 1.0f / (1.0f + d_ij);
        const Real w1 = w_FP * 2.0f / ((1.0f + d_ij) * (1.0f + d_ij));
        for (idx_t d = 0; d < dim; ++d) {
            grad[i][d] -= w1 * y_ij[d];
            grad[j][d] += w1 * y_ij[d];
        }
    }

    const Real total_loss = loss0 + loss1 + loss2;

    return PacmapGradResult{ grad, total_loss };
}


struct PacmapGradFitResult {
    vecVecReal grad;
    Real loss;
};


PacmapGradFitResult pacmap_grad_fit(
    const vecVecReal& Y,
    const std::vector<vecIdx>& pair_XP,
    const Real w_neighbors)
{
    const auto n = static_cast<idx_t>(Y.size());
    assert(n > 0);
    const auto dim = static_cast<idx_t>(Y[0].size());
    assert(dim > 0);

    vecVecReal grad(n, vecReal(dim, 0.0f));
    Real loss3 = 0.0f;

    for (const auto & tx : pair_XP) {
        assert(static_cast<idx_t>(tx.size()) == 2);
        const idx_t i = tx[0];
        const idx_t j = tx[1];
        assert(i >= 0 && i < n);
        assert(j >= 0 && j < n);

        vecReal y_ij(dim);
        Real d_ij = 1.0f;
        for (idx_t d = 0; d < dim; ++d) {
            y_ij[d] = Y[i][d] - Y[j][d];
            d_ij += y_ij[d] * y_ij[d];
        }
        loss3 += w_neighbors * (d_ij / (10.0f + d_ij));
        const Real w1 = w_neighbors * (20.0f / ((10.0f + d_ij) * (10.0f + d_ij)));

        // only update gradient for new point i, not existing point j
        for (idx_t d = 0; d < dim; ++d)
            grad[i][d] += w1 * y_ij[d];
    }

    return PacmapGradFitResult{ grad, loss3 };
}

struct Weights {
    Real w_MN;
    Real w_neighbors;
    Real w_FP;
};


Weights find_weight(const Real w_MN_init, const idx_t itr, const WeightPhases& num_iters)
{
    const idx_t phase_1_iters = num_iters.phase_1_iters;
    const idx_t phase_2_iters = num_iters.phase_2_iters;
    assert(phase_1_iters > 0);
    assert(phase_2_iters > 0);
    assert(itr >= 0);

    if (itr < phase_1_iters) {
        const Real t = static_cast<Real>(itr) / static_cast<Real>(phase_1_iters);
        return {
            (1.0f - t) * w_MN_init + t * 3.0f,
            2.0f,
            1.0f
        };
    }
    if (itr < phase_1_iters + phase_2_iters) {
        return { 3.0f, 3.0f, 1.0f };
    }
    return { 0.0f, 1.0f, 1.0f };
}


std::vector<vecIdx> sample_neighbors_pair_basis(
    const idx_t n_basis,
    const vecVecReal& X,
    const vecVecReal& scaled_dist,
    const std::vector<vecIdx>& neighbors,
    const idx_t n_neighbors)
{
    // note: scaled_dist here is raw L2 distance for new points, not sigma-scaled.
    // sig is not available for extra points, so raw distance ordering is used as approximation.

    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(n_neighbors > 0);
    assert(static_cast<idx_t>(scaled_dist.size()) == n);
    assert(static_cast<idx_t>(neighbors.size()) == n);

    std::vector<vecIdx> pair_neighbors(n * n_neighbors, vecIdx(2));

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {
        assert(static_cast<idx_t>(scaled_dist[i].size()) >= static_cast<idx_t>(neighbors[i].size()));

        vecIdx scaled_sort(scaled_dist[i].size());
        std::iota(scaled_sort.begin(), scaled_sort.end(), 0);
        std::ranges::sort(scaled_sort,
                          [&](const idx_t a, const idx_t b) {
                              return scaled_dist[i][a] < scaled_dist[i][b];
                          });

        assert(n_neighbors <= static_cast<idx_t>(scaled_sort.size()));
        for (idx_t j = 0; j < n_neighbors; ++j) {
            assert(scaled_sort[j] < static_cast<idx_t>(neighbors[i].size()));
            pair_neighbors[i * n_neighbors + j][0] = n_basis + i;
            pair_neighbors[i * n_neighbors + j][1] = neighbors[i][scaled_sort[j]];
        }
    }
    return pair_neighbors;
}


std::vector<vecIdx> generate_extra_pair_basis(
    const vecVecReal& basis,
    const vecVecReal& X,
    idx_t n_neighbors,
    const bool verbose)
{
    assert(!basis.empty());
    assert(!X.empty());

    const auto n    = static_cast<idx_t>(basis.size());
    const auto dim  = static_cast<idx_t>(basis[0].size());
    const auto npr   = static_cast<idx_t>(X.size());
    const auto dimp  = static_cast<idx_t>(X[0].size());
    assert(dim == dimp);

    const idx_t n_neighbors_extra = std::min(n_neighbors + 50, n - 1);
    n_neighbors = std::min(n_neighbors, n - 1);
    assert(n_neighbors > 0);

    if (n - 1 < n_neighbors) {
        std::cerr << "Warning: sample size is smaller than n_neighbors. n_neighbors will be reduced.\n";
    }

    const Eigen::MatrixXf B = to_eigen(basis);
    const Eigen::MatrixXf Q = to_eigen(X);

    const auto [indices, distances] = knn_search(B, Q, n_neighbors_extra);

    if (verbose) {
        std::cout << "Found nearest neighbors\n";
    }

    // convert to format expected by sample_neighbors_pair_basis
    std::vector<vecIdx> neighbors(npr, vecIdx(n_neighbors_extra));
    vecVecReal knn_distances(npr, vecReal(n_neighbors_extra));
    for (idx_t i = 0; i < npr; ++i) {
        for (idx_t j = 0; j < n_neighbors_extra; ++j) {
            neighbors[i][j] = indices[i][j];
            knn_distances[i][j] = distances[i][j];
        }
    }

    return sample_neighbors_pair_basis(n, X, knn_distances, neighbors, n_neighbors);
}


struct GeneratePairResult {
    std::vector<vecIdx> pair_neighbors;
    std::vector<vecIdx> pair_MN;
    std::vector<vecIdx> pair_FP;
};

GeneratePairResult generate_pair(
    const vecVecReal& X,
    idx_t n_neighbors,
    idx_t n_MN,
    idx_t n_FP,
    const std::optional<idx_t> random_state,
    const bool verbose = true)
{
/*
     tree is dropped from the return value since we have no stateful index to cache.
     if caller later needs to call generate_extra_pair_basis with the same X as basis,
     it will rerun knn_search from scratch.
     if this becomes bottleneck, try returning precomputed
     Eigen::MatrixXf from to_eigen(X) as cached basis matrix.
*/
    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);

    if (n - 1 < n_neighbors) {
        std::cerr << "Warning: sample size smaller than n_neighbors. n_neighbors will be reduced.\n";
    }
    if (n - 1 < n_FP) {
        std::cerr << "Warning: sample size smaller than n_FP. n_FP will be reduced.\n";
    }
    if (n - 1 < n_MN) {
        std::cerr << "Warning: sample size smaller than n_MN. n_MN will be reduced.\n";
    }

    const idx_t n_neighbors_extra = std::min(n_neighbors + 50, n - 1);
    n_neighbors = std::min(n_neighbors, n - 1);
    n_FP        = std::min(n_FP,        n - 1);
    n_MN        = std::min(n_MN,        n - 1);

    if (n_neighbors + n_MN + n_FP >= n) {
        std::cerr << "Warning: sample size smaller than total assigned points. Reorganizing n_neighbors, n_MN, n_FP.\n";
        if (n_neighbors > 0) {
            const Real mn_r = static_cast<Real>(n_MN) / static_cast<Real>(n_neighbors);
            const Real fp_r = static_cast<Real>(n_FP) / static_cast<Real>(n_neighbors);
            n_neighbors = static_cast<idx_t>(static_cast<Real>(n) / (1.0f + mn_r + fp_r));
            n_MN        = static_cast<idx_t>(static_cast<float>(n_neighbors) * mn_r);
            n_FP        = static_cast<idx_t>(static_cast<float>(n_neighbors) * fp_r);
        } else {
            n_neighbors = 0;
            n_MN        = 0;
            n_FP        = 0;
        }
    }

    assert(n_neighbors_extra >= 6); // required for sig computation (cols 3:6)

    auto [neighbors, knn_distances] = compute_nearest_neighbors(X, n_neighbors_extra);

    if (verbose) std::cout << "Found nearest neighbors\n";

    // sig = max(mean(knn_distances[:, 3:6], axis=1), 1e-10)
    const auto n_knn = static_cast<idx_t>(knn_distances[0].size());
    assert(n_knn >= 6);
    vecReal sig(n);
    for (idx_t i = 0; i < n; ++i) {
        const Real mean_356 = (knn_distances[i][3] +
                               knn_distances[i][4] +
                               knn_distances[i][5]) / 3.0f;
        sig[i] = std::max(mean_356, 1e-10f);
    }

    if (verbose) std::cout << "Calculated sigma\n";

    const vecVecReal scaled = scale_dist(knn_distances, sig, neighbors);

    if (verbose) std::cout << "Found scaled dist\n";

    const std::vector<vecIdx> pair_neighbors = sample_neighbors_pair(
        X, scaled, neighbors, n_neighbors);

    std::vector<vecIdx> pair_MN;
    std::vector<vecIdx> pair_FP;

    if (!random_state.has_value()) {
        pair_MN = sample_MN_pair(X, n_MN, Distance_e::Euclidean);
        pair_FP = sample_FP_pair(X, pair_neighbors, n_neighbors, n_FP);
    } else {
        pair_MN = sample_MN_pair_deterministic(X, n_MN, Distance_e::Euclidean);
        pair_FP = sample_FP_pair_deterministic(X, pair_neighbors, n_neighbors, n_FP);
    }

    return GeneratePairResult{ pair_neighbors, pair_MN, pair_FP };
}


GeneratePairResult generate_pair_no_neighbors(
    const vecVecReal& X,
    const idx_t n_neighbors,
    const idx_t n_MN,
    const idx_t n_FP,
    const std::vector<vecIdx>& pair_neighbors,
    const std::optional<idx_t> random_state)
{
    assert(!X.empty());
    assert(!pair_neighbors.empty());

    std::vector<vecIdx> pair_MN;
    std::vector<vecIdx> pair_FP;

    if (!random_state.has_value()) {
        pair_MN = sample_MN_pair(X, n_MN, Distance_e::Euclidean);
        pair_FP = sample_FP_pair(X, pair_neighbors, n_neighbors, n_FP);
    } else {
        pair_MN = sample_MN_pair_deterministic(X, n_MN, Distance_e::Euclidean);
        pair_FP = sample_FP_pair_deterministic(X, pair_neighbors, n_neighbors, n_FP);
    }

    return GeneratePairResult{ pair_neighbors, pair_MN, pair_FP };
}


struct PacmapResult {
    vecVecReal Y;
    std::vector<vecVecReal> intermediate_states; // empty if intermediate=false
    std::vector<vecIdx> pair_neighbors;
    std::vector<vecIdx> pair_MN;
    std::vector<vecIdx> pair_FP;
};

// inline StandardScaler: center and scale each column to unit variance
static vecVecReal standard_scale(const vecVecReal& Y)
{
    const auto n   = static_cast<idx_t>(Y.size());
    const auto dim = static_cast<idx_t>(Y[0].size());
    vecReal mean(dim, 0.0f), std_dev(dim, 0.0f);

    for (idx_t d = 0; d < dim; ++d) {
        for (idx_t i = 0; i < n; ++i) mean[d] += Y[i][d];
        mean[d] /= static_cast<Real>(n);
        for (idx_t i = 0; i < n; ++i) std_dev[d] += (Y[i][d] - mean[d]) * (Y[i][d] - mean[d]);
        std_dev[d] = std::sqrt(std_dev[d] / static_cast<Real>(n));
        if (std_dev[d] < 1e-10f) std_dev[d] = 1.0f; // prevent division by zero
    }

    vecVecReal result(n, vecReal(dim));
    for (idx_t i = 0; i < n; ++i)
        for (idx_t d = 0; d < dim; ++d)
            result[i][d] = (Y[i][d] - mean[d]) / std_dev[d];
    return result;
}

PacmapResult pacmap(
    const vecVecReal& X,
    const idx_t n_dims,
    const std::vector<vecIdx>& pair_neighbors,
    const std::vector<vecIdx>& pair_MN,
    const std::vector<vecIdx>& pair_FP,
    const Real lr,
    const WeightPhases& num_iters,
    const YinitParam& Yinit,
    const FittedSVD& tsvd,
    const std::optional<idx_t> random_state,
    const bool verbose,
    const bool intermediate,
    const vecIdx& inter_snapshots)
{
/*
     standard_scale uses population std (dividing by n) instead of sample std (dividing by n-1).
     sklearn's StandardScaler uses population std by default, so this fn matches that.
     update_embedding_adam takes grad as const vecVecReal&, but earlier implementation
     takes this as const vecVecReal& and Y as vecVecReal&.

     re: inter_snapshots bounds: if itr_ind reaches the end of inter_snapshots before the loop ends,
     the check itr_ind < inter_snapshots.size() guards against out-of-bounds.
     the og python code doesn't have this guard and would crash with an index error if inter_snapshots were malformed.
*/
    const auto n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(n_dims > 0);
    assert(!intermediate || !inter_snapshots.empty());
    assert(!intermediate || std::ranges::is_sorted(inter_snapshots));

    const auto t_start = std::chrono::steady_clock::now();

    vecVecReal Y(n, vecReal(n_dims));

    if (Yinit.user_supplied.has_value()) {
        // StandardScaler then scale by 0.0001
        const vecVecReal scaled = standard_scale(Yinit.user_supplied.value());
        for (idx_t i = 0; i < n; ++i)
            for (idx_t d = 0; d < n_dims; ++d)
                Y[i][d] = scaled[i][d] * 0.0001f;

    } else if (Yinit.mode == YinitMode::PCA) {
        assert(tsvd.is_fitted);
        const Eigen::MatrixXf Xm = to_eigen(X);
        const Eigen::MatrixXf Yt = tsvd.transform(Xm);
        assert(Yt.cols() >= n_dims);
        for (idx_t i = 0; i < n; ++i) {
            for (idx_t d = 0; d < n_dims; ++d) {
                Y[i][d] = 0.01f * Yt(i, d);
            }
        }
    } else { // YinitMode::Random
        std::mt19937 rng_init(random_state.has_value()
            ? static_cast<uint32_t>(random_state.value())
            : std::random_device{}());
        std::normal_distribution<float> normal(0.0f, 1.0f);
        for (idx_t i = 0; i < n; ++i) {
            for (idx_t d = 0; d < n_dims; ++d) {
                Y[i][d] = normal(rng_init) * 0.0001f;
            }
        }
    }

    constexpr Real w_MN_init = 1000.0f;
    constexpr Real beta1     = 0.9f;
    constexpr Real beta2     = 0.999f;
    vecVecReal m(n, vecReal(n_dims, 0.0f));
    vecVecReal v(n, vecReal(n_dims, 0.0f));

    std::vector<vecVecReal> intermediate_states;
    idx_t itr_ind = 0;
    if (intermediate) {
        intermediate_states.resize(inter_snapshots.size());
        if (inter_snapshots[0] == 0) {
            intermediate_states[0] = Y;
            itr_ind = 1;
        }
    }

    if (verbose) {
        std::cout << "Pairs: " << pair_neighbors.size()
                  << " " << pair_MN.size()
                  << " " << pair_FP.size() << "\n";
    }

    const idx_t num_iters_total = num_iters.phase_1_iters
                                + num_iters.phase_2_iters;
                                + num_iters.phase_3_iters;

    // MAIN OPTIMIZATION LOOP
    for (idx_t itr = 0; itr < num_iters_total; ++itr) {
        auto [w_MN, w_neighbors, w_FP] = find_weight(w_MN_init, itr, num_iters);

        auto [grad, loss] = pacmap_grad(
            Y, pair_neighbors, pair_MN, pair_FP,
            w_neighbors, w_MN, w_FP);

        const Real C = loss;
        if (verbose && itr == 0)
            std::cout << "Initial Loss: " << C << "\n";

        update_embedding_adam(Y, grad, m, v, beta1, beta2, lr, itr);

        if (intermediate && itr_ind < static_cast<idx_t>(inter_snapshots.size())) {
            if (itr + 1 == inter_snapshots[itr_ind]) {
                intermediate_states[itr_ind] = Y;
                ++itr_ind;
            }
        }

        if (verbose && (itr + 1) % 10 == 0) {
            std::cout << "Iteration: " << std::setw(4) << (itr + 1)
                      << ", Loss: " << C << "\n";
        }
    }

    const auto t_end = std::chrono::steady_clock::now();
    const Real elapsed = std::chrono::duration<Real>(t_end - t_start).count();
    if (verbose) {
        std::cout << "Elapsed time: " << std::fixed << std::setprecision(2)
                  << elapsed << "s\n";
    }

    return PacmapResult{ Y, intermediate_states, pair_neighbors, pair_MN, pair_FP };
}

struct PacmapFitResult {
    vecVecReal Y;
    std::vector<vecVecReal> intermediate_states; // empty if intermediate=false
};

PacmapFitResult pacmap_fit(
    const vecVecReal& X,
    const vecVecReal& embedding,
    const idx_t n_dims,
    const std::vector<vecIdx>& pair_XP,
    const Real lr,
    const WeightPhases& num_iters,
    const YinitParam& Yinit,
    const FittedSVD& tsvd,
    const std::optional<idx_t> random_state,
    const bool verbose,
    const bool intermediate,
    const vecIdx& inter_snapshots)
{
    const auto n       = static_cast<idx_t>(X.size());
    const auto n_basis = static_cast<idx_t>(embedding.size());
    assert(n > 0);
    assert(n_basis > 0);
    assert(n_dims > 0);
    assert(!intermediate || !inter_snapshots.empty());
    assert(!intermediate || std::ranges::is_sorted(inter_snapshots));
    assert(static_cast<idx_t>(embedding[0].size()) == n_dims);

    const auto t_start = std::chrono::steady_clock::now();

    // init new points' rows
    vecVecReal Y_new(n, vecReal(n_dims));

    if (Yinit.user_supplied.has_value()) {
        vecVecReal scaled = standard_scale(Yinit.user_supplied.value());
        for (idx_t i = 0; i < n; ++i) {
            for (idx_t d = 0; d < n_dims; ++d) {
                Y_new[i][d] = scaled[i][d] * 0.0001f;
            }
        }
    } else if (Yinit.mode == YinitMode::PCA) {
        assert(tsvd.is_fitted);
        Eigen::MatrixXf Xm = to_eigen(X);
        Eigen::MatrixXf Yt = tsvd.transform(Xm);
        assert(Yt.cols() >= n_dims);
        for (idx_t i = 0; i < n; ++i) {
            for (idx_t d = 0; d < n_dims; ++d) {
                Y_new[i][d] = 0.01f * Yt(i, d);
            }
        }
    } else { // YinitMode::Random
        std::mt19937 rng_init(random_state.has_value()
            ? static_cast<uint32_t>(random_state.value())
            : std::random_device{}());
        std::normal_distribution<float> normal(0.0f, 1.0f);
        for (idx_t i = 0; i < n; ++i) {
            for (idx_t d = 0; d < n_dims; ++d) {
                Y_new[i][d] = normal(rng_init) * 0.0001f;
            }
        }
    }

    // concatenate embedding (basis) + new points
    vecVecReal Y;
    Y.reserve(n_basis + n);
    Y.insert(Y.end(), embedding.begin(), embedding.end());
    Y.insert(Y.end(), Y_new.begin(), Y_new.end());
    assert(static_cast<idx_t>(Y.size()) == n_basis + n);
    // each row of Y must have right num dims
    assert(static_cast<idx_t>(Y[0].size()) == n_dims);
    assert(static_cast<idx_t>(Y[n_basis].size()) == n_dims); // first new point


    // optimizer state (full concatenated size)
    const Real beta1 = 0.9f;
    const Real beta2 = 0.999f;
    vecVecReal m(n_basis + n, vecReal(n_dims, 0.0f));
    vecVecReal v(n_basis + n, vecReal(n_dims, 0.0f));

    // intermediate snapshots (new points only, to avoid shape bug)
    std::vector<vecVecReal> intermediate_states;
    idx_t itr_ind = 0;
    if (intermediate) {
        intermediate_states.resize(inter_snapshots.size());
        if (inter_snapshots[0] == 0) {
            // store only new points' rows
            intermediate_states[0] = vecVecReal(Y.begin() + n_basis, Y.end());
            itr_ind = 1;
        }
    }
    for (const auto& pair : pair_XP) {
        assert(pair[0] >= n_basis && pair[0] < n_basis + n); // new point index
        assert(pair[1] >= 0 && pair[1] < n_basis);           // basis point index
    }
    if (verbose) {
        std::cout << "pair_XP size: " << pair_XP.size() << "\n";
    }

    const idx_t num_iters_total = num_iters.phase_1_iters
                                + num_iters.phase_2_iters
                                + num_iters.phase_3_iters;
    const idx_t max_itr_ind = static_cast<idx_t>(inter_snapshots.size()) - 1;


    for (idx_t itr = 0; itr < num_iters_total; ++itr) {
        // only w_neighbors used in pacmap_grad_fit
        Weights w = find_weight(0.0f, itr, num_iters);

        auto [grad, loss] = pacmap_grad_fit(Y, pair_XP, w.w_neighbors);

        const Real C = loss;
        if (verbose && itr == 0)
            std::cout << "Initial Loss: " << C << "\n";

        update_embedding_adam(Y, grad, m, v, beta1, beta2, lr, itr);

        if (intermediate && itr_ind <= max_itr_ind) {
            if (itr + 1 == inter_snapshots[itr_ind]) {
                intermediate_states[itr_ind] = vecVecReal(Y.begin() + n_basis, Y.end());
                itr_ind = std::min(itr_ind + 1, max_itr_ind);
            }
        }

        if (verbose && (itr + 1) % 10 == 0) {
            std::cout << "Iteration: " << std::setw(4) << (itr + 1)
                      << ", Loss: " << C << "\n";
        }
    }

    // Assert all snapshots were filled and have the correct shape (new points only)
    if (intermediate) {
        for (const auto& snap : intermediate_states) {
            assert(!snap.empty());
            assert(static_cast<idx_t>(snap.size()) == n);
            assert(static_cast<idx_t>(snap[0].size()) == n_dims);
        }
    }

    const auto t_end = std::chrono::steady_clock::now();
    const Real elapsed = std::chrono::duration<Real>(t_end - t_start).count();
    if (verbose) {
        std::cout << "Elapsed time: " << std::fixed << std::setprecision(2)
                  << elapsed << "s\n";
    }

    return PacmapFitResult{ Y, intermediate_states };
}


//======================================================================================

PaCMAP::PaCMAP(
    const idx_t n_components,
    const std::optional<idx_t> n_neighbors,
    const Real MN_ratio,
    const Real FP_ratio,
    const Real lr,
    const WeightPhases num_iters,
    const bool verbose,
    const PreprocessMode_e preprocess_mode,
    const bool intermediate,
    const bool save_tree,
    const vecIdx &intermediate_snapshots,
    const std::optional<idx_t> random_state)
:   n_components_(n_components)
,   n_neighbors_opt_(n_neighbors)
,   MN_ratio_(MN_ratio)
,   FP_ratio_(FP_ratio)
,   lr_(lr)
,   num_iters_(num_iters)
,   verbose_(verbose)
,   intermediate_(intermediate)
,   save_tree_(save_tree)
,   intermediate_snapshots_(intermediate_snapshots)
,   random_state_(random_state)
,   preprocess_mode_(preprocess_mode)
,   num_instances_(0), num_dimensions_(0)
{
    if (n_components < 1)
        throw std::invalid_argument("n_components must be at least 1.");
    if (lr <= 0.0f)
        throw std::invalid_argument("Learning rate must be > 0.");
    if (n_components != 2 && verbose)
        std::cerr << "Warning: n_components != 2 has not been thoroughly tested.\n";
}
void PaCMAP::setVerbose(const bool verbose) {
    verbose_ = verbose;
}

void PaCMAP::decide_num_pairs(const idx_t n)
{
    if (!n_neighbors_opt_.has_value()) {
        if (n <= 10000) {
            n_neighbors_ = 10;
        }
        else {
            n_neighbors_ = static_cast<idx_t>(std::round(
                    10.0 + 15.0 * (std::log10(static_cast<double>(n)) - 4.0)));
        }
    } else {
        n_neighbors_ = n_neighbors_opt_.value();
    }

    n_MN_ = static_cast<idx_t>(std::round(static_cast<float>(n_neighbors_) * MN_ratio_));
    n_FP_ = static_cast<idx_t>(std::round(static_cast<float>(n_neighbors_) * FP_ratio_));

    if (n - 1 < n_neighbors_) {
        std::cerr << "Warning: sample size smaller than n_neighbors. Reducing.\n";
    }
    n_neighbors_ = std::min(n_neighbors_, n - 1);

    if (n - 1 < n_FP_) {
        std::cerr << "Warning: sample size smaller than n_FP. Reducing.\n";
    }
    n_FP_ = std::min(n_FP_, n - 1 - n_neighbors_);

    if (n - 1 < n_MN_) {
        std::cerr << "Warning: sample size smaller than n_MN. Reducing.\n";
    }
    n_MN_ = std::min(n_MN_, n - 1);

    if (n_neighbors_ + n_MN_ + n_FP_ >= n) {
        std::cerr << "Warning: reorganizing n_neighbors, n_MN, n_FP.\n";
        const Real denom = 1.0f + MN_ratio_ + FP_ratio_;
        n_neighbors_ = static_cast<idx_t>(static_cast<float>(n) / denom);
        n_MN_        = static_cast<idx_t>(static_cast<float>(n) / denom * MN_ratio_);
        n_FP_        = static_cast<idx_t>(static_cast<float>(n) / denom * FP_ratio_);
    }

    if (n_neighbors_ < 1) {
        throw std::invalid_argument("n_neighbors < 1 after reduction.");
    }
    if (n_FP_ < 1) {
        throw std::invalid_argument("n_FP < 1 after reduction.");
    }
}

void PaCMAP::sample_pairs(const vecVecReal& X)
{
    if (verbose_) {
        std::cout << "Finding pairs\n";
    }

    if (pair_neighbors_.empty()) {
        auto [pair_neighbors, pair_MN, pair_FP] = generate_pair(
            X, n_neighbors_, n_MN_, n_FP_, random_state_, verbose_);
        pair_neighbors_ = pair_neighbors;
        pair_MN_        = pair_MN;
        pair_FP_        = pair_FP;
        if (verbose_) std::cout << "Pairs sampled successfully.\n";

    } else if (pair_MN_.empty() && pair_FP_.empty()) {
        if (verbose_) std::cout << "Using user provided nearest neighbor pairs.\n";
        assert(static_cast<idx_t>(pair_neighbors_.size()) ==
               static_cast<idx_t>(X.size()) * n_neighbors_);
        const auto [pair_neighbors, pair_MN, pair_FP] = generate_pair_no_neighbors(
            X, n_neighbors_, n_MN_, n_FP_, pair_neighbors_, random_state_);
        pair_neighbors_ = pair_neighbors;
        pair_MN_        = pair_MN;
        pair_FP_        = pair_FP;
        if (verbose_) std::cout << "Pairs sampled successfully.\n";

    } else {
        if (verbose_) std::cout << "Using stored pairs.\n";
    }
}

void PaCMAP::del_pairs()
{
    pair_neighbors_.clear();
    pair_MN_.clear();
    pair_FP_.clear();
}

void PaCMAP::fit(const vecVecReal& X_in, const YinitParam &init, const bool save_pairs)
{
    assert(!X_in.empty());
    const auto n   = static_cast<idx_t>(X_in.size());
    const auto dim = static_cast<idx_t>(X_in[0].size());

    if (n <= 1) {
        throw std::invalid_argument("Sample size must be larger than 1.");
    }

    preprocess_result_ = preprocess_X(X_in, Distance_e::Euclidean,
        preprocess_mode_,
        verbose_,
        random_state_.value_or(0),
        dim,
        n_components_);

    decide_num_pairs(n);

    if (verbose_)
        std::cout << "PaCMAP(n_neighbors=" << n_neighbors_
                  << ", n_MN=" << n_MN_
                  << ", n_FP=" << n_FP_
                  << ", lr=" << lr_
                  << ", preprocess_mode=" << ((preprocess_mode_ == Normalize) ? "normalize" : "standardize")
                  << ", intermediate=" << intermediate_ << ")\n";

    sample_pairs(preprocess_result_.X);

    num_instances_  = n;
    num_dimensions_ = static_cast<idx_t>(preprocess_result_.X[0].size());

    auto [Y, intermediate_states, pair_neighbors, pair_MN, pair_FP] =
        pacmap(
            preprocess_result_.X, n_components_,
            pair_neighbors_, pair_MN_, pair_FP_,
            lr_, num_iters_, init,
            preprocess_result_.tsvd,
            random_state_, verbose_,
            intermediate_,
            intermediate_snapshots_);

    embedding_           = Y;
    intermediate_states_ = intermediate_states;
    pair_neighbors_       = pair_neighbors;
    pair_MN_              = pair_MN;
    pair_FP_              = pair_FP;
    is_fitted_            = true;

    if (!save_pairs) {
        del_pairs();
    }
}

vecVecReal PaCMAP::fit_transform(
    const vecVecReal& X,
    const YinitParam &init,
    const bool save_pairs)
{
    fit(X, init, save_pairs);
    return embedding_;
}

std::vector<vecVecReal> PaCMAP::fit_transform_intermediate(
    const vecVecReal& X,
    const YinitParam &init,
    const bool save_pairs)
{
    fit(X, init, save_pairs);
    return intermediate_states_;
}

std::vector<vecVecReal> PaCMAP::transform(
    const vecVecReal& X_in,
    const vecVecReal& basis,
    const YinitParam &init,
    const bool save_pairs)
{
    if (!is_fitted_) {
        throw std::runtime_error("PaCMAP instance is not fitted. Call fit() first.");
    }

    const vecVecReal X = preprocess_X_new(
        X_in, preprocess_mode_, preprocess_result_, verbose_);

    const vecVecReal basis_proc = preprocess_X_new(
        basis, preprocess_mode_, preprocess_result_, verbose_);

    pair_XP_ = generate_extra_pair_basis(basis_proc, X, n_neighbors_, verbose_);

    auto [Y, intermediate_states] = pacmap_fit(
        X, embedding_, n_components_,
        pair_XP_, lr_, num_iters_, init,
        preprocess_result_.tsvd,
        random_state_, verbose_,
        intermediate_, intermediate_snapshots_);

    if (!save_pairs) {
        pair_XP_.clear();
    }

    const auto n_basis = static_cast<idx_t>(embedding_.size());

    if (intermediate_) {
        return intermediate_states;
    }

    // strip basis rows, return only new points
    return { vecVecReal(Y.begin() + n_basis, Y.end()) };
}

bool PaCMAP::is_fitted() const {
    return is_fitted_;
}
vecVecReal PaCMAP::getEmbedding() const {
    return embedding_;
}

#if INCLUDE_UNUSED

vecIdx sample_FP_nearby(
    const idx_t n_samples,
    const idx_t maximum,
    const vecIdx& reject_ind,
    const idx_t self_ind,
    const vecVecReal& Y,
    const Real low_dist_thres,
    std::mt19937& rng)
{
    assert(maximum > 0);
    assert(n_samples > 0);
    assert(self_ind >= 0 && self_ind < maximum);
    assert(static_cast<idx_t>(Y.size()) >= maximum);

    std::uniform_int_distribution<idx_t> dist(0, maximum - 1);
    vecIdx result(n_samples);

    for (idx_t i = 0; i < n_samples; ++i) {
        idx_t j = -1;
        idx_t count = 0;
        bool reject_sample = true;

        while (reject_sample) {
            j = dist(rng);
            ++count;

            if (j == self_ind) continue;

            bool in_result = false;
            for (idx_t k = 0; k < i; ++k) {
                if (j == result[k]) { in_result = true; break; }
            }
            if (in_result) continue;

            bool in_reject = false;
            for (const idx_t r : reject_ind) {
                if (j == r) { in_reject = true; break; }
            }
            if (in_reject) continue;

            if (euclidean_dist(Y[self_ind], Y[j]) > low_dist_thres) {
                // j is too far in low-dim space
                if (count > 100) {
                    // give up, store sentinel -1 for this slot
                    j = -1;
                    reject_sample = false;
                }
                continue;
            }

            reject_sample = false;
        }

        // j may be -1 if no valid nearby sample was found within 100 attempts
        if (j == -1) {
            assert(false);
        }
        result[i] = j;
    }
    return result;
}

std::vector<vecIdx> sample_FP_pair_nearby(
    const vecVecReal& X,
    const std::vector<vecIdx>& pair_neighbors,
    const std::vector<vecIdx>& old_pair_FP,
    const vecVecReal& Y,
    const Real low_dist_thres)
{
    /*
    -1 fallback uses old_pair_FP — when sample_FP_nearby returns -1 for a slot, the old pair is kept.
    this is the caller-side handling of the sentinel we flagged.

    n_neighbors and n_FP derived from shape, so both are back-calculated from the pair array sizes divided by n.
    worth asserting these divide evenly.

    inner prange on k — same as before, not worth parallelizing.

    old_pair_FP[i*n_FP + k][1] indexes into the old pairs to get the fallback neighbor index.
    must assert old_pair_FP has the same layout as the new pair_FP.

    rng here refers to the thread_local std::mt19937 since this is the non-deterministic path.
     */
    const idx_t n = static_cast<idx_t>(X.size());
    assert(n > 0);
    assert(static_cast<idx_t>(pair_neighbors.size()) % n == 0);
    assert(static_cast<idx_t>(old_pair_FP.size()) % n == 0);

    const idx_t n_neighbors = static_cast<idx_t>(pair_neighbors.size()) / n;
    const idx_t n_FP        = static_cast<idx_t>(old_pair_FP.size()) / n;
    assert(n_neighbors > 0);
    assert(n_FP > 0);
    assert(static_cast<idx_t>(Y.size()) == n);

    std::vector<vecIdx> pair_FP(n * n_FP, vecIdx(2));

#pragma omp parallel for
    for (idx_t i = 0; i < n; ++i) {
        vecIdx reject_ind(n_neighbors);
        for (idx_t k = 0; k < n_neighbors; ++k) {
            assert(static_cast<idx_t>(pair_neighbors[i * n_neighbors + k].size()) == 2);
            reject_ind[k] = pair_neighbors[i * n_neighbors + k][1];
        }

        vecIdx FP_index = sample_FP_nearby(
            n_FP, n, reject_ind, i, Y, low_dist_thres, rng);
        assert(static_cast<idx_t>(FP_index.size()) == n_FP);

        for (idx_t k = 0; k < n_FP; ++k) {
            pair_FP[i * n_FP + k][0] = i;
            if (FP_index[k] == -1) {
                // no valid nearby sample found within 100 attempts — keep old pair
                assert(static_cast<idx_t>(old_pair_FP[i * n_FP + k].size()) == 2);
                pair_FP[i * n_FP + k][1] = old_pair_FP[i * n_FP + k][1];
            } else {
                pair_FP[i * n_FP + k][1] = FP_index[k];
            }
        }
    }
    return pair_FP;
}

PacmapGradResult pacmap_grad_nearby_recip_sqrt(
    const vecVecReal& Y,
    const std::vector<vecIdx>& pair_neighbors,
    const std::vector<vecIdx>& pair_MN,
    const std::vector<vecIdx>& pair_FP,
    const Real w_neighbors,
    const Real w_MN,
    const Real w_FP,
    const Real NN_coef_recip)
{
/*
    only differs from pacmap_grad in the NN section; MN and FP sections are identical. The only difference
    is w1 gets an additional factor of NN_coef_recip / sqrt(d_ij) after the standard computation. Worth noting
    that d_ij >= 1.0 always (since it's initialized to 1.0 and only positive terms are added), so sqrt(d_ij) is
    never zero — no division by zero risk.

    NN_coef_recip / sqrt(d_ij) applied after standard w1 — this is a multiplicative correction, not a separate term.

    The gradient direction is unchanged, only the magnitude is scaled.

    Given how much this overlaps with pacmap_grad, this could share code via a helper, but since you said no
    micro-optimizations I'll keep it as a standalone function.

    Since this reuses PacmapGradResult, the MN and FP sections are identical to pacmap_grad, and the return type
    is the same, the caller can treat them interchangeably — which is presumably the intent since they'd be
    swapped in depending on which gradient variant is active.
     */
    const idx_t n = static_cast<idx_t>(Y.size());
    assert(n > 0);
    const idx_t dim = static_cast<idx_t>(Y[0].size());
    assert(dim > 0);

    vecVecReal grad(n, vecReal(dim, 0.0f));
    Real loss0 = 0.0f, loss1 = 0.0f, loss2 = 0.0f;

    // NN
    for (idx_t t = 0; t < static_cast<idx_t>(pair_neighbors.size()); ++t) {
        assert(static_cast<idx_t>(pair_neighbors[t].size()) == 2);
        const idx_t i = pair_neighbors[t][0];
        const idx_t j = pair_neighbors[t][1];
        assert(i >= 0 && i < n);
        assert(j >= 0 && j < n);

        vecReal y_ij(dim);
        Real d_ij = 1.0f;
        for (idx_t d = 0; d < dim; ++d) {
            y_ij[d] = Y[i][d] - Y[j][d];
            d_ij += y_ij[d] * y_ij[d];
        }
        loss0 += w_neighbors * (d_ij / (10.0f + d_ij));
        Real w1 = w_neighbors * (20.0f / ((10.0f + d_ij) * (10.0f + d_ij)));
        // Additional reciprocal sqrt factor for nearby variant
        w1 *= NN_coef_recip / std::sqrt(d_ij); // d_ij >= 1.0 so sqrt is safe
        for (idx_t d = 0; d < dim; ++d) {
            grad[i][d] += w1 * y_ij[d];
            grad[j][d] -= w1 * y_ij[d];
        }
    }

    // MN — identical to pacmap_grad
    for (idx_t tt = 0; tt < static_cast<idx_t>(pair_MN.size()); ++tt) {
        assert(static_cast<idx_t>(pair_MN[tt].size()) == 2);
        const idx_t i = pair_MN[tt][0];
        const idx_t j = pair_MN[tt][1];
        assert(i >= 0 && i < n);
        assert(j >= 0 && j < n);

        vecReal y_ij(dim);
        Real d_ij = 1.0f;
        for (idx_t d = 0; d < dim; ++d) {
            y_ij[d] = Y[i][d] - Y[j][d];
            d_ij += y_ij[d] * y_ij[d];
        }
        loss1 += w_MN * d_ij / (10000.0f + d_ij);
        const Real w = w_MN * 20000.0f / ((10000.0f + d_ij) * (10000.0f + d_ij));
        for (idx_t d = 0; d < dim; ++d) {
            grad[i][d] += w * y_ij[d];
            grad[j][d] -= w * y_ij[d];
        }
    }

    // FP — identical to pacmap_grad
    for (idx_t ttt = 0; ttt < static_cast<idx_t>(pair_FP.size()); ++ttt) {
        assert(static_cast<idx_t>(pair_FP[ttt].size()) == 2);
        const idx_t i = pair_FP[ttt][0];
        const idx_t j = pair_FP[ttt][1];
        assert(i >= 0 && i < n);
        assert(j >= 0 && j < n);

        vecReal y_ij(dim);
        Real d_ij = 1.0f;
        for (idx_t d = 0; d < dim; ++d) {
            y_ij[d] = Y[i][d] - Y[j][d];
            d_ij += y_ij[d] * y_ij[d];
        }
        loss2 += w_FP * 1.0f / (1.0f + d_ij);
        const Real w1 = w_FP * 2.0f / ((1.0f + d_ij) * (1.0f + d_ij));
        for (idx_t d = 0; d < dim; ++d) {
            grad[i][d] -= w1 * y_ij[d];
            grad[j][d] += w1 * y_ij[d];
        }
    }

    const Real total_loss = loss0 + loss1 + loss2;
    return PacmapGradResult{ grad, total_loss };
}
#endif
}   // namespace nvs::dim
