#pragma once

namespace smartcity::core {

struct ConstructionCandidate {
    int id{-1};
    int firstLocationId{-1};
    int secondLocationId{-1};
    double constructionCost{0.0};

    [[nodiscard]] bool hasValidCost() const noexcept;
};

} // namespace smartcity::core
