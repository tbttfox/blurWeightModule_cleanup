#pragma once

#include <maya/MIntArray.h>

#include <array>
#include <span>
#include <unordered_map>
#include <vector>

// Compressed Sparse Row (CSR) storage templates used throughout the skin brush.
//
// FlatCounts<T>   — variable-length rows, accessed by index.  x[i] -> span of T
// FlatChunks<T,C> — fixed-length rows of size C.             x[i] -> span<T,C>
// DoubleChunks<T,C> — jagged 2-level structure.              x(i,j) -> span<T,C>

template <typename T = int> class FlatCounts {
  private:
    std::vector<size_t> offsets;
    std::vector<T> values;

  public:
    void set(const std::vector<T> &inCounts, const std::vector<T> &inVals)
    {
        offsets.clear();
        values.clear();
        values = inVals;
        offsets.resize(inCounts.size() + 1);
        offsets[0] = 0;
        size_t i = 1, v = 0;
        for (const auto &c : inCounts) {
            v += c;
            offsets[i++] = v;
        }
    }

    void set(const MIntArray &inCounts, const MIntArray &inVals)
    {
        offsets.clear();
        values.clear();
        values.reserve(inVals.length());
        for (unsigned i = 0; i < inVals.length(); ++i) {
            values.push_back(inVals[i]);
        }
        offsets.resize(inCounts.length() + 1);
        offsets[0] = 0;
        size_t i = 1, v = 0;
        for (const auto &c : inCounts) {
            v += c;
            offsets[i++] = v;
        }
    }

    void set(const std::vector<std::vector<T>> &inVals)
    {
        offsets.clear();
        values.clear();
        offsets.reserve(inVals.size() + 1);
        offsets.push_back(0);
        for (const auto &sub : inVals) {
            values.insert(values.end(), sub.begin(), sub.end());
            offsets.push_back(values.size());
        }
    }

    template <typename K> void set(const std::unordered_map<K, std::vector<T>> &inVals)
    {
        offsets.clear();
        values.clear();
        const auto maxit = std::max_element(inVals.begin(), inVals.end());
        K maxkey = maxit->first;
        offsets.reserve(maxkey + 1);
        offsets.push_back(0);
        size_t offset = 0;
        for (size_t i = 0; i < maxkey; ++i) {
            auto search = inVals.find(i);
            if (search != inVals.end()) {
                offset += search->second.size();
                values.insert(values.end(), search->second.begin(), search->second.end());
            }
            offsets.push_back(offset);
        }
    }

    size_t length() const { return offsets.size() - 1; }

    const std::span<const T> operator[](size_t i) const
    {
        return std::span<const T>(values.begin() + offsets[i], values.begin() + offsets[i + 1]);
    }
};

template <typename T = int, size_t C = 2> class FlatChunks {
  private:
    std::vector<T> values;

  public:
    void set(const std::vector<std::array<int, C>> &invals)
    {
        values.reserve(invals.size() * C);
        for (const auto &ev : invals) {
            for (const auto &v : ev) {
                values.push_back(v);
            }
        }
    }

    void set(const T *invals, size_t length)
    {
        values.clear();
        values.insert(values.end(), invals, invals + (length * C));
    }

    size_t length() const { return values.size() / C; }

    const std::span<const T> operator[](size_t i) const
    {
        return std::span<const T>(values.begin() + (C * i), values.begin() + (C * (i + 1)));
    }
};

template <typename T = int, size_t C = 3> class DoubleChunks {
  private:
    std::vector<size_t> offsets;
    std::vector<T> values;

  public:
    void set(const MIntArray &counts, const MIntArray &flattris)
    {
        values.clear();
        values.reserve(flattris.length());
        for (size_t i = 0; i < flattris.length(); ++i) {
            values.push_back(flattris[i]);
        }
        offsets.clear();
        offsets.reserve(counts.length() + 1);
        offsets.push_back(0);
        size_t v = 0;
        for (size_t i = 0; i < counts.length(); ++i) {
            v += counts[i];
            offsets.push_back(v);
        }
    }

    void set(const std::vector<std::vector<std::array<T, C>>> &perEdgeVertices)
    {
        values.clear();
        offsets.clear();
        offsets.push_back(0);
        size_t offset = 0;
        values.reserve(perEdgeVertices.size() * C);
        for (const auto &eev : perEdgeVertices) {
            offset += eev.size();
            offsets.push_back(offset);
            for (const auto &ev : eev) {
                for (const auto &v : ev) {
                    values.push_back(v);
                }
            }
        }
    }

    size_t length() const { return offsets.size() - 1; }
    size_t length2(size_t i) const { return offsets[i + 1] - offsets[i]; }

    const std::span<const T> operator()(size_t i, size_t j) const
    {
        return std::span<const T>(
            values.begin() + offsets[i] + (C * j), values.begin() + offsets[i] + (C * (j + 1))
        );
    }
};
