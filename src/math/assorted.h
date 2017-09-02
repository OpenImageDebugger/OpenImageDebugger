#pragma once

#include <algorithm>

template<typename T>
int clamp(T value, T lower, T upper) {
    return std::min(std::max(value, lower), upper);
}
