//
// Created by Nicholas Solem on 3/5/26.
//

#pragma once


#include <Eigen/Dense>
#include <Eigen/SVD>
#include <unordered_set>
#include "dim_using.h"

namespace nvs::dim {

inline Eigen::VectorXf to_eigen(const vecReal& v)
{
    return Eigen::Map<const Eigen::VectorXf>(v.data(), static_cast<Eigen::Index>(v.size()));
}
/** to_eigen: (row-major)*/
inline Eigen::MatrixXf to_eigen(const vecVecReal& X)
{
    const idx_t n = static_cast<idx_t>(X.size());
    const idx_t d = static_cast<idx_t>(X[0].size());
    Eigen::MatrixXf M(n, d);
    for (idx_t i = 0; i < n; ++i)
        for (idx_t j = 0; j < d; ++j)
            M(i, j) = X[i][j];
    return M;
}
template <std::size_t N>
Eigen::MatrixXf to_eigen(const std::array<vecReal, N>& X)
{
    const idx_t d = static_cast<idx_t>(X[0].size());
    Eigen::MatrixXf M(static_cast<idx_t>(N), d);
    for (idx_t i = 0; i < static_cast<idx_t>(N); ++i)
        for (idx_t j = 0; j < d; ++j)
            M(i, j) = X[i][j];
    return M;
}

/** from_eigen: (row-major) Eigen matrix -> vecVecReal*/
inline vecVecReal from_eigen(const Eigen::MatrixXf& M)
{
    const idx_t n = static_cast<idx_t>(M.rows());
    const idx_t d = static_cast<idx_t>(M.cols());
    vecVecReal result(n, vecReal(d));
    for (idx_t i = 0; i < n; ++i)
        for (idx_t j = 0; j < d; ++j)
            result[i][j] = M(i, j);
    return result;
}

template <std::size_t N>
void from_eigen(const Eigen::MatrixXf& M, std::array<vecReal, N>& out)
{
    const idx_t d = static_cast<idx_t>(M.cols());
    for (idx_t i = 0; i < static_cast<idx_t>(N); ++i)
        for (idx_t j = 0; j < d; ++j)
            out[i][j] = M(i, j);
}

inline void decorrelateFromCovariates(Eigen::MatrixXf& features,
                                      const Eigen::VectorXf& pitch,
                                      const Eigen::VectorXf& loudness)
{
    const Eigen::Index N = pitch.size();

    // center pitch and loudness
    const Eigen::VectorXf p = pitch.array() - pitch.mean();
    const Eigen::VectorXf l = loudness.array() - loudness.mean();

    // regressor matrix
    Eigen::MatrixXf X(N, 2);
    X.col(0) = p;
    X.col(1) = l;

    // solver needs only one-time computation
    const auto solver = (X.transpose() * X).colPivHouseholderQr();

    for (Eigen::Index i = 0; i < features.cols(); ++i)
    {
        Eigen::VectorXf f = features.col(i);
        f.array() -= f.mean();
        Eigen::VectorXf beta = solver.solve(X.transpose() * f);
        features.col(i) = f - X * beta;
    }
}

inline Eigen::MatrixXf removeColumns(const Eigen::MatrixXf& M, const std::vector<int>& colsToRemove)
/* assumes that M has all features present, and the ints of colsToRemove represent corresponding features
 */
{
    const auto removeSet = std::unordered_set<int>(colsToRemove.begin(), colsToRemove.end());

    Eigen::MatrixXf result(M.rows(), M.cols() - static_cast<int>(colsToRemove.size()));
    int outCol = 0;
    for (int i = 0; i < M.cols(); ++i) {
        if (!removeSet.contains(i)) {
            result.col(outCol++) = M.col(i);
        }
    }
    return result;
}

/** FittedSVD:
 * Holds a fitted PCA/TruncatedSVD projection for later use with transform()
 * returns: Eigen::MatrixXf of shape (n_data, n_components)
 */
struct FittedSVD {
    Eigen::MatrixXf components; // shape: (n_components, n_features)
    bool is_fitted = false;

    Eigen::MatrixXf transform(const Eigen::MatrixXf& X) const
    {
        assert(is_fitted);
        assert(X.cols() == components.cols());
        return X * components.transpose(); // (n_data, n_features) * (n_features, n_components)
    }
};


struct PreprocessResult {
    vecVecReal       X;
    vecReal          col_shift;  // per-column: col_min (Normalize) or col_mu (Standardize)
    vecReal          col_scale;  // per-column: col_range (Normalize) or col_sigma (Standardize)
    vecReal          xmean;      // post-scaling column means, for centering new data
    FittedSVD        tsvd;       // PCA components for embedding init only — not used in preprocess_X_new
};

namespace {
void fit_and_apply_column_transform(
    Eigen::MatrixXf&   M,
    const idx_t        d,
    const PreprocessMode_e mode,
    vecReal&           col_shift_out,
    vecReal&           col_scale_out)
{
    col_shift_out.assign(d, 0.0f);
    col_scale_out.assign(d, 1.0f);  // default: identity (constant features stay as-is)

    for (idx_t j = 0; j < d; ++j) {
        float shift = 0.0f, scale = 1.0f;

        if (mode == PreprocessMode_e::Normalize) {
            const float cmin  = M.col(j).minCoeff();
            const float cmax  = M.col(j).maxCoeff();
            if (const float range = cmax - cmin; range > 1e-8f)
            { shift = cmin; scale = range; }
        } else if (mode == PreprocessMode_e::Standardize) {
            const float mu = M.col(j).mean();
            if (const float sigma = std::sqrt((M.col(j).array() - mu).square().mean()); sigma > 1e-8f)
            { shift = mu; scale = sigma; }
        }

        col_shift_out[j] = shift;
        col_scale_out[j] = scale;
        M.col(j) = (M.col(j).array() - shift) / scale;
        // constant features: scale==1, shift==0 → left as-is;
        // the subsequent mean subtraction will zero them out
    }
}

// Apply stored per-column transform + centering to M in-place
void apply_column_transform(
    Eigen::MatrixXf& M,
    const idx_t      d,
    const vecReal&   col_shift,
    const vecReal&   col_scale,
    const vecReal&   xmean)
{
    for (idx_t j = 0; j < d; ++j)
        M.col(j) = (M.col(j).array() - col_shift[j]) / col_scale[j] - xmean[j];
}
}


inline PreprocessResult preprocess_X(
    const vecVecReal&      X_in,
    const Distance_e       distance,
    const PreprocessMode_e preprocess_mode,
    const bool             verbose,
    const idx_t            seed_unused,   // SVD is deterministic; seed has no effect
    const idx_t            high_dim,
    const idx_t            low_dim)
{
#pragma message("at the moment, seed is unused, and this algorithm already has deterministic behavior")
    assert(!X_in.empty());
    const idx_t n = static_cast<idx_t>(X_in.size());
    const idx_t d = static_cast<idx_t>(X_in[0].size());
    assert(d == high_dim);

    Eigen::MatrixXf M = to_eigen(X_in);
    vecReal col_shift, col_scale;

    fit_and_apply_column_transform(M, d, preprocess_mode, col_shift, col_scale);

    const Eigen::VectorXf mean = M.colwise().mean();
    M.rowwise() -= mean.transpose();

    vecReal xmean(d);
    for (idx_t j = 0; j < d; ++j)
        xmean[j] = mean(j);

    assert(d >= low_dim);
    const Eigen::BDCSVD<Eigen::MatrixXf, Eigen::ComputeThinU | Eigen::ComputeThinV> svd(M);
    FittedSVD tsvd;
    tsvd.components = svd.matrixV().leftCols(low_dim).transpose();  // (low_dim, d)
    tsvd.is_fitted  = true;

    if (verbose) {
        std::cout << (preprocess_mode == PreprocessMode_e::Normalize
                      ? "X is normalized\n" : "X is standardized\n");
    }
    return PreprocessResult{ from_eigen(M), col_shift, col_scale, xmean, tsvd };
}

// Takes the full PreprocessResult from the original fit — no need to pass params individually
inline vecVecReal preprocess_X_new(
    const vecVecReal&      X_in,
    const PreprocessMode_e preprocess_mode,
    const PreprocessResult& fit,
    const bool             verbose)
{
    assert(!X_in.empty());
    const idx_t d = static_cast<idx_t>(X_in[0].size());
    assert(static_cast<idx_t>(fit.xmean.size())     == d);
    assert(static_cast<idx_t>(fit.col_shift.size()) == d);
    assert(static_cast<idx_t>(fit.col_scale.size()) == d);

    Eigen::MatrixXf M = to_eigen(X_in);
    apply_column_transform(M, d, fit.col_shift, fit.col_scale, fit.xmean);

    if (verbose) {
        std::cout << (preprocess_mode == PreprocessMode_e::Normalize
                      ? "X is normalized\n" : "X is standardized\n");
    }
    return from_eigen(M);
}


}   // namespace nvs::dim

