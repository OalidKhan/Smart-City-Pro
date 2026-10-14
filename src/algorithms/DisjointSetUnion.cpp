#include "algorithms/DisjointSetUnion.h"

#include <numeric>
#include <stdexcept>
#include <utility>

namespace smartcity::algorithms {

DisjointSetUnion::DisjointSetUnion(std::size_t elementCount)
    : parent_(elementCount), rank_(elementCount, 0), componentCount_(elementCount)
{
    std::iota(parent_.begin(), parent_.end(), 0);
}

int DisjointSetUnion::find(int element)
{
    if (element < 0 || static_cast<std::size_t>(element) >= parent_.size()) {
        throw std::out_of_range("Disjoint-set element is out of range");
    }
    if (parent_[element] != element) parent_[element] = find(parent_[element]);
    return parent_[element];
}

bool DisjointSetUnion::unite(int first, int second)
{
    int firstRoot = find(first);
    int secondRoot = find(second);
    if (firstRoot == secondRoot) return false;
    if (rank_[firstRoot] < rank_[secondRoot]) std::swap(firstRoot, secondRoot);
    parent_[secondRoot] = firstRoot;
    if (rank_[firstRoot] == rank_[secondRoot]) ++rank_[firstRoot];
    --componentCount_;
    return true;
}

std::size_t DisjointSetUnion::componentCount() const noexcept { return componentCount_; }

} // namespace smartcity::algorithms
