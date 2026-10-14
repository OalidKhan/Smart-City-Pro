#include "core/models/ConstructionCandidate.h"

#include <cmath>

namespace smartcity::core {

bool ConstructionCandidate::hasValidCost() const noexcept
{
    return std::isfinite(constructionCost) && constructionCost >= 0.0;
}

} // namespace smartcity::core
