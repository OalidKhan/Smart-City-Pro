#pragma once

#include <cstddef>
#include <vector>

namespace smartcity::algorithms {

class DisjointSetUnion {
public:
    explicit DisjointSetUnion(std::size_t elementCount);

    int find(int element);
    bool unite(int first, int second);
    [[nodiscard]] std::size_t componentCount() const noexcept;

private:
    std::vector<int> parent_;
    std::vector<unsigned int> rank_;
    std::size_t componentCount_;
};

} // namespace smartcity::algorithms
