//
// Created by Nicholas Solem on 3/5/26.
//

#pragma once

#include <vector>
#include <optional>

#include "dim_using.h"
#include "pca.h"
#include "util.h"

namespace nvs::dim {

/**
    notes: fit_transform return type — in Python it returns either intermediate_states (a 3D array) or
    the embedding (2D). In C++ both are std::vector<vecVecReal> but with different semantics — one has
    multiple snapshots, the other has one. The caller needs to know which mode they're in. A std::variant
    would be cleaner here but adds complexity; worth revisiting.

    transform always preprocesses basis — the Python skips this if self.tree is not None (i.e. the tree was
    cached). Since we have no tree, we always reprocess basis. This is correct but potentially slow for large
    basis sets called repeatedly.

    random_state.value_or(0) in preprocess_X — the Python passes self.random_state which defaults to 0 when
    no random state is given, so this matches.

    User-supplied pairs — the Python allows pre-setting pair_neighbors, pair_MN, pair_FP in the constructor.
    In C++ the user can set these member variables directly before calling fit(), which achieves the same
    thing.
 */
class PaCMAP {
public:
    explicit PaCMAP(
        idx_t n_components = 2,
        std::optional<idx_t> n_neighbors = std::nullopt,
        Real MN_ratio = 0.5f,
        Real FP_ratio = 2.0f,
        Real lr = 1.0f,
        WeightPhases num_iters = {100, 100, 250},
        bool verbose = false,
        PreprocessMode_e preprocess_mode = PreprocessMode_e::Normalize,
        bool intermediate = false,
        bool save_tree = false,
        const vecIdx &intermediate_snapshots = {0,10,30,60,100,120,140,170,200,250,300,350,450},
        std::optional<idx_t> random_state = std::nullopt);

    void decide_num_pairs(idx_t n);

    void sample_pairs(const vecVecReal& X);

    void del_pairs();

    void setVerbose(bool verbose);
    /**
     *   Projects a high dimensional dataset into a low-dimensional embedding, without returning the output.
     *
     *   Parameters
     *   ---------
     *   X: numpy.ndarray
     *       The high-dimensional dataset that is being projected.
     *       An embedding will get created based on parameters of the PaCMAP instance.
     *
     *   init: str, optional
     *       One of ['pca', 'random']. Initialization of the embedding, default='pca'.
     *       If 'pca', then the low dimensional embedding is initialized to the PCA mapped dataset.
     *       If 'random', then the low dimensional embedding is initialized with a Gaussian distribution.
     *
     *   save_pairs: bool, optional
     *       Whether to save the pairs that are sampled from the dataset. Useful for reproducing results.
     */
    void fit(const vecVecReal& X_in, const YinitParam &init = {YinitMode::PCA, std::nullopt}, bool save_pairs = true);

/**
 * Projects a high dimensional dataset into a low-dimensional embedding and return the embedding.
 * @param X:
 * The high-dimensional dataset that is being projected.
 * An embedding will get created based on parameters of the PaCMAP instance.
* @param init: YinitMode::PCA or YinitMode::Random.
* If 'pca', then the low dimensional embedding is initialized to the PCA mapped dataset.
* If 'random', then the low dimensional embedding is initialized with a Gaussian distribution.
 * @param save_pairs:
 * Whether to save the pairs that are sampled from the dataset. Useful for reproducing results.
 * @return embedding.
 */
    vecVecReal fit_transform(
        const vecVecReal& X,
        const YinitParam &init = {YinitMode::PCA, std::nullopt},
        bool save_pairs = true);

    std::vector<vecVecReal> fit_transform_intermediate(
        const vecVecReal& X,
        const YinitParam &init = {YinitMode::PCA, std::nullopt},
        bool save_pairs = true);

    // Returns new points' embedding only (basis rows stripped)
    std::vector<vecVecReal> transform(
        const vecVecReal& X_in,
        const vecVecReal& basis,
        const YinitParam &init = {YinitMode::PCA, std::nullopt},
        bool save_pairs = true);

    [[nodiscard]] bool is_fitted() const;
    [[nodiscard]] vecVecReal getEmbedding() const;

private:
    idx_t n_components_;
    std::optional<idx_t> n_neighbors_opt_; // optional: auto-decided if not set
    Real MN_ratio_;
    Real FP_ratio_;
    Real lr_;
    WeightPhases num_iters_;
    bool verbose_;
    bool intermediate_;
    bool save_tree_;
    vecIdx intermediate_snapshots_;
    std::optional<idx_t> random_state_;

    // --- Decided at fit time ---
    idx_t n_neighbors_{};
    idx_t n_MN_{};
    idx_t n_FP_{};

    // --- State set during fit ---
    vecVecReal embedding_;
    std::vector<vecVecReal> intermediate_states_;
    std::vector<vecIdx> pair_neighbors_;
    std::vector<vecIdx> pair_MN_;
    std::vector<vecIdx> pair_FP_;
    std::vector<vecIdx> pair_XP_;
    bool is_fitted_ = false;

    PreprocessMode_e preprocess_mode_;
    PreprocessResult preprocess_result_;
    idx_t num_instances_;
    idx_t num_dimensions_;
};


/*
pacmap_grad_nearby_recip_sqrt: Only differs from pacmap_grad in the NN section – the MN and FP sections are
identical. The only difference is w1 gets an additional factor of NN_coef_recip / sqrt(d_ij) after
standard computation. Worth noting that d_ij >= 1.0 always (since it's initialized to 1.0 and only
positive terms are added), so sqrt(d_ij) is never zero — no division by zero risk.

NN_coef_recip / sqrt(d_ij) applied after standard w1 — this is a multiplicative correction, not a separate term.

The gradient direction is unchanged, only the magnitude is scaled.

Given how much this overlaps with pacmap_grad, this could share code via a helper, but since you said no
micro-optimizations I'll keep it as a standalone function.

Since this reuses PacmapGradResult, the MN and FP sections are identical to pacmap_grad, and the return type
is the same, the caller can treat them interchangeably — which is presumably the intent since they'd be
swapped in depending on which gradient variant is active.
 */
PacmapGradResult pacmap_grad_nearby_recip_sqrt(
    const vecVecReal& Y,
    const std::vector<vecIdx>& pair_neighbors,
    const std::vector<vecIdx>& pair_MN,
    const std::vector<vecIdx>& pair_FP,
    Real w_neighbors,
    Real w_MN,
    Real w_FP,
    Real NN_coef_recip);


}   // namespace nvs::dim

