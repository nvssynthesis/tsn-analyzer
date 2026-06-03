//
// Created by Nicholas Solem on 3/5/26.
//

#ifndef DR_USING_H
#define DR_USING_H

#include <vector>


namespace nvs::dim {
using Real = float;
using vecReal = std::vector<Real>;
using vecVecReal = std::vector<vecReal>;


using idx_t = int32_t;
using vecIdx = std::vector<idx_t>;

enum class Distance_e {
    Euclidean = 0
    /*
    manhattan, angular, and hamming available in original
    */
};
enum class PreprocessMode_e {
    Normalize,
    Standardize
};

}   // namespace nvs::dim
#endif //DR_USING_H
