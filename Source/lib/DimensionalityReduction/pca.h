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
inline Eigen::MatrixXf to_eigen(const std::array<vecReal, N>& X)
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
inline void from_eigen(const Eigen::MatrixXf& M, std::array<vecReal, N>& out)
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
    vecVecReal X;
    bool pca_solution;
    FittedSVD tsvd;
    Real xmin;
    Real xmax;
    vecReal xmean;
};

inline PreprocessResult preprocess_X(
    const vecVecReal& X_in,
    const Distance_e distance,
    const bool apply_pca,
    const bool verbose,
    const idx_t seed_unused,           // noted but unused: Eigen SVD is deterministic, seed has no effect
    const idx_t high_dim,
    const idx_t low_dim)
{
#pragma message("at the moment, seed is unused, and this algorithm already has deterministic behavior")
    assert(!X_in.empty());
    const idx_t n = static_cast<idx_t>(X_in.size());
    const idx_t d = static_cast<idx_t>(X_in[0].size());
    assert(d == high_dim);

    Eigen::MatrixXf M = to_eigen(X_in);
    FittedSVD tsvd;
    bool pca_solution = false;
    Real xmin = 0.0f, xmax = 0.0f;
    vecReal xmean(d, 0.0f);

    if (distance == Distance_e::Euclidean && high_dim > 100 && apply_pca) {
        // --- PCA branch: center then reduce to 100 dims via TruncatedSVD ---
        Eigen::VectorXf mean = M.colwise().mean();
        M.rowwise() -= mean.transpose();

        // Store xmean for return
        xmean.resize(d);
        for (idx_t j = 0; j < d; ++j)
            xmean[j] = mean(j);

        // TruncatedSVD: full SVD, take top 100 right singular vectors
        constexpr idx_t n_components = 100;
        assert(d >= n_components);
        const Eigen::BDCSVD<Eigen::MatrixXf> svd(M, Eigen::ComputeThinU | Eigen::ComputeThinV);

        // components = top n_components rows of V^T, i.e. first n_components cols of V
        tsvd.components = svd.matrixV().leftCols(n_components).transpose(); // (100, d)
        tsvd.is_fitted = true;

        M = tsvd.transform(M); // (n, 100)
        pca_solution = true;

        if (verbose)
            std::cout << "Applied PCA, the dimensionality becomes 100\n";

    } else {
        // --- Normalization branch: min/max scale then center ---
        xmin = M.minCoeff();
        M.array() -= xmin;
        xmax = M.maxCoeff();
        assert(xmax != 0.0f); // would produce NaN/Inf on division
        M.array() /= xmax;

        Eigen::VectorXf mean = M.colwise().mean();
        M.rowwise() -= mean.transpose();

        xmean.resize(d);
        for (idx_t j = 0; j < d; ++j) {
            xmean[j] = mean(j);
        }

        // Fit PCA(n_components=low_dim) for init only — X is NOT transformed
        assert(d >= low_dim);
        const Eigen::BDCSVD<Eigen::MatrixXf> svd(M, Eigen::ComputeThinU | Eigen::ComputeThinV);
        tsvd.components = svd.matrixV().leftCols(low_dim).transpose(); // (low_dim, d)
        tsvd.is_fitted = true;

        if (verbose)
            std::cout << "X is normalized\n";
    }

    return PreprocessResult{
        from_eigen(M),
        pca_solution,
        tsvd,
        xmin,
        xmax,
        xmean
    };
}

inline vecVecReal preprocess_X_new(
    const vecVecReal& X_in,
    const Distance_e distance,
    const Real xmin,
    const Real xmax,
    const vecReal& xmean,
    const FittedSVD& tsvd,
    const bool apply_pca,
    const bool verbose)
{
    assert(!X_in.empty());
    const idx_t n = static_cast<idx_t>(X_in.size());
    const idx_t high_dim = static_cast<idx_t>(X_in[0].size());
    assert(static_cast<idx_t>(xmean.size()) == high_dim);
    assert(tsvd.is_fitted);

    Eigen::MatrixXf M = to_eigen(X_in);

    if (distance == Distance_e::Euclidean && high_dim > 100 && apply_pca) {
        // Subtract original xmean (pre-reduction, full dim)
        for (idx_t j = 0; j < high_dim; ++j)
            M.col(j).array() -= xmean[j];

        M = tsvd.transform(M);

        if (verbose)
            std::cout << "Applied PCA, the dimensionality becomes 100 for new dataset.\n";
    } else {
        assert(xmax != 0.0f);
        M.array() -= xmin;
        M.array() /= xmax;

        for (idx_t j = 0; j < high_dim; ++j)
            M.col(j).array() -= xmean[j];

        if (verbose)
            std::cout << "X is normalized.\n";
    }

    return from_eigen(M);
}


}   // namespace nvs::dim

