#include "algorithms/SortingAlgorithms.h"

#include <cmath>
#include <utility>

namespace smartcity::algorithms {
namespace {

class SortEngine {
public:
    SortEngine(const std::vector<double>& input, SortOrder order)
        : values_(input), order_(order) {}

    SortResult run(SortAlgorithm algorithm)
    {
        SortResult result;
        for (double value : values_) if (!std::isfinite(value)) return result;
        emit({AlgorithmEventType::Started});
        if (!values_.empty()) {
            switch (algorithm) {
            case SortAlgorithm::Bubble: bubble(); break;
            case SortAlgorithm::Selection: selection(); break;
            case SortAlgorithm::Insertion: insertion(); break;
            case SortAlgorithm::Merge: mergeSort(0, static_cast<int>(values_.size()) - 1); break;
            case SortAlgorithm::Quick: quickSort(0, static_cast<int>(values_.size()) - 1); break;
            case SortAlgorithm::Heap: heap(); break;
            }
        }
        AlgorithmEvent finished{AlgorithmEventType::Finished};
        if (!values_.empty()) { finished.rangeStart = 0; finished.rangeEnd = static_cast<int>(values_.size()) - 1; }
        emit(finished);
        result.validInput = true;
        result.values = std::move(values_);
        result.counters = counters_;
        result.events = std::move(events_);
        result.eventLimitReached = eventLimitReached_;
        return result;
    }

private:
    bool before(double left, double right) const
    { return order_ == SortOrder::Ascending ? left < right : left > right; }

    bool compareBefore(int leftIndex, int rightIndex)
    {
        ++counters_.comparisons;
        AlgorithmEvent event{AlgorithmEventType::ArrayCompared};
        event.indexA = leftIndex; event.indexB = rightIndex; emit(event);
        return before(values_[static_cast<std::size_t>(leftIndex)], values_[static_cast<std::size_t>(rightIndex)]);
    }

    bool compareValues(double left, double right, int leftIndex, int rightIndex)
    {
        ++counters_.comparisons;
        AlgorithmEvent event{AlgorithmEventType::ArrayCompared};
        event.indexA = leftIndex; event.indexB = rightIndex; emit(event);
        return before(left, right);
    }

    void swapAt(int first, int second)
    {
        if (first == second) return;
        std::swap(values_[static_cast<std::size_t>(first)], values_[static_cast<std::size_t>(second)]);
        ++counters_.swaps; counters_.writes += 2;
        AlgorithmEvent event{AlgorithmEventType::ArraySwapped};
        event.indexA = first; event.indexB = second; emit(event);
    }

    void writeAt(int index, double value)
    {
        values_[static_cast<std::size_t>(index)] = value; ++counters_.writes;
        AlgorithmEvent event{AlgorithmEventType::ArrayWritten};
        event.indexA = index; event.value = value; emit(event);
    }

    void mark(int first, int last)
    {
        AlgorithmEvent event{AlgorithmEventType::ArrayRangeMarked};
        event.rangeStart = first; event.rangeEnd = last; emit(event);
    }

    void emit(const AlgorithmEvent& event)
    {
        if (events_.size() < SortingAlgorithms::MaximumStoredEvents) events_.push_back(event);
        else eventLimitReached_ = true;
    }

    void bubble()
    {
        const int size = static_cast<int>(values_.size());
        for (int end = size - 1; end > 0; --end) {
            bool changed = false;
            for (int index = 0; index < end; ++index) {
                if (compareBefore(index + 1, index)) { swapAt(index, index + 1); changed = true; }
            }
            mark(end, size - 1);
            if (!changed) { mark(0, end - 1); break; }
        }
    }

    void selection()
    {
        const int size = static_cast<int>(values_.size());
        for (int start = 0; start < size; ++start) {
            int best = start;
            for (int index = start + 1; index < size; ++index)
                if (compareBefore(index, best)) best = index;
            swapAt(start, best); mark(0, start);
        }
    }

    void insertion()
    {
        const int size = static_cast<int>(values_.size());
        if (size > 0) mark(0, 0);
        for (int index = 1; index < size; ++index) {
            const double key = values_[static_cast<std::size_t>(index)];
            int position = index;
            while (position > 0) {
                if (!compareValues(key, values_[static_cast<std::size_t>(position - 1)], index, position - 1)) break;
                writeAt(position, values_[static_cast<std::size_t>(position - 1)]); --position;
            }
            writeAt(position, key); mark(0, index);
        }
    }

    void mergeSort(int first, int last)
    {
        if (first >= last) return;
        const int middle = first + (last - first) / 2;
        mergeSort(first, middle); mergeSort(middle + 1, last);
        std::vector<double> merged; merged.reserve(static_cast<std::size_t>(last - first + 1));
        int left = first; int right = middle + 1;
        while (left <= middle && right <= last) {
            // Take from the left on equality, preserving stability.
            if (!compareValues(values_[static_cast<std::size_t>(right)], values_[static_cast<std::size_t>(left)], right, left))
                merged.push_back(values_[static_cast<std::size_t>(left++)]);
            else merged.push_back(values_[static_cast<std::size_t>(right++)]);
        }
        while (left <= middle) merged.push_back(values_[static_cast<std::size_t>(left++)]);
        while (right <= last) merged.push_back(values_[static_cast<std::size_t>(right++)]);
        for (int index = first; index <= last; ++index)
            writeAt(index, merged[static_cast<std::size_t>(index - first)]);
        mark(first, last);
    }

    int partition(int first, int last)
    {
        const double pivot = values_[static_cast<std::size_t>(last)];
        int boundary = first;
        for (int index = first; index < last; ++index) {
            if (compareValues(values_[static_cast<std::size_t>(index)], pivot, index, last)) {
                swapAt(boundary, index); ++boundary;
            }
        }
        swapAt(boundary, last); mark(boundary, boundary); return boundary;
    }

    void quickSort(int first, int last)
    {
        if (first >= last) return;
        const int pivot = partition(first, last);
        quickSort(first, pivot - 1); quickSort(pivot + 1, last);
    }

    void siftDown(int root, int count)
    {
        while (true) {
            const int left = root * 2 + 1;
            if (left >= count) return;
            int chosen = left;
            const int right = left + 1;
            // Build the opposite heap so its root belongs at the end.
            if (right < count && compareBefore(left, right)) chosen = right;
            if (!compareBefore(root, chosen)) return;
            swapAt(root, chosen); root = chosen;
        }
    }

    void heap()
    {
        const int size = static_cast<int>(values_.size());
        for (int root = size / 2 - 1; root >= 0; --root) siftDown(root, size);
        for (int end = size - 1; end > 0; --end) {
            swapAt(0, end); mark(end, size - 1); siftDown(0, end);
        }
        if (size > 0) mark(0, size - 1);
    }

    std::vector<double> values_;
    SortOrder order_;
    OperationCounters counters_;
    std::vector<AlgorithmEvent> events_;
    bool eventLimitReached_{false};
};

bool inputSorted(const std::vector<double>& values, SortOrder order)
{
    for (std::size_t index = 1; index < values.size(); ++index) {
        if (order == SortOrder::Ascending ? values[index] < values[index - 1]
                                          : values[index] > values[index - 1]) return false;
    }
    return true;
}
}

SortResult SortingAlgorithms::sort(const std::vector<double>& input,
                                   SortAlgorithm algorithm, SortOrder order)
{
    return SortEngine(input, order).run(algorithm);
}

SearchResult SortingAlgorithms::binarySearch(const std::vector<double>& sortedInput,
                                             double target, SortOrder order)
{
    SearchResult result;
    if (!std::isfinite(target) || !inputSorted(sortedInput, order)) return result;
    for (double value : sortedInput) if (!std::isfinite(value)) return result;
    int low = 0; int high = static_cast<int>(sortedInput.size()) - 1;
    while (low <= high) {
        const int middle = low + (high - low) / 2;
        ++result.comparisons;
        AlgorithmEvent probe{AlgorithmEventType::SearchProbe};
        probe.indexA = middle; probe.rangeStart = low; probe.rangeEnd = high;
        if (result.events.size() < MaximumStoredEvents) result.events.push_back(probe);
        else result.eventLimitReached = true;
        const double value = sortedInput[static_cast<std::size_t>(middle)];
        if (value == target) {
            result.found = true; result.index = middle;
            AlgorithmEvent found{AlgorithmEventType::SearchFound}; found.indexA = middle;
            if (result.events.size() < MaximumStoredEvents) result.events.push_back(found);
            else result.eventLimitReached = true;
            break;
        }
        const bool targetBefore = order == SortOrder::Ascending ? target < value : target > value;
        if (targetBefore) high = middle - 1; else low = middle + 1;
    }
    result.validInput = true;
    return result;
}

AlgorithmProfile SortingAlgorithms::profile(SortAlgorithm algorithm) noexcept
{
    switch (algorithm) {
    case SortAlgorithm::Bubble: return {"Bubble Sort", "O(n²), best O(n)", "O(1)", true, false};
    case SortAlgorithm::Selection: return {"Selection Sort", "O(n²)", "O(1)", false, false};
    case SortAlgorithm::Insertion: return {"Insertion Sort", "O(n²), best O(n)", "O(1)", true, false};
    case SortAlgorithm::Merge: return {"Merge Sort", "O(n log n)", "O(n)", true, true};
    case SortAlgorithm::Quick: return {"Quick Sort", "average O(n log n), worst O(n²)", "average O(log n), worst O(n)", false, true};
    case SortAlgorithm::Heap: return {"Heap Sort", "O(n log n)", "O(1)", false, false};
    }
    return {"Unknown", "", "", false, false};
}

} // namespace smartcity::algorithms
