//
// Created by Nicholas Solem on 1/28/26.
//

#pragma once
#include <type_traits>

namespace nvs::util {

template < typename C, C beginVal, C endVal>
class Iterator {
    typedef typename std::underlying_type<C>::type val_t;
    int val;
public:
    explicit Iterator(const C & f) : val(static_cast<val_t>(f)) {}
    Iterator() : val(static_cast<val_t>(beginVal)) {}
    Iterator operator++() {
        ++val;
        return *this;
    }
    C operator*() { return static_cast<C>(val); }
    Iterator begin() { return *this; } //default ctor is good
    Iterator end() {
        static const Iterator endIter=++Iterator(endVal); // cache it
        return endIter;
    }
    bool operator!=(const Iterator& i) { return val != i.val; }
};

} // namespace nvs::util