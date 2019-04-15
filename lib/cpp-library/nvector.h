/*
  Copyright (C) 2017 Sven Willner <sven.willner@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NVECTOR_H
#define NVECTOR_H

#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

template<typename T, unsigned char dim, class Storage = std::vector<T>>
class nvector {
  protected:
    std::array<std::size_t, dim> dims;
    Storage data;

    template<unsigned char c, typename... Args>
    inline T& i_(const std::size_t index, const std::size_t& i, Args... args) noexcept {
        return i_<c + 1>(index * dims[c] + i, args...);
    }

    template<unsigned char c>
    inline T& i_(const std::size_t index) noexcept {
        static_assert(c == dim, "wrong number of arguments");
        return data[index];
    }

    template<unsigned char c, typename... Args>
    inline T& at_(const std::size_t index, const std::size_t& i, Args... args) {
        if (i >= dims[c]) {
            throw std::out_of_range("index out of bounds");
        }
        return at_<c + 1>(index * dims[c] + i, args...);
    }

    template<unsigned char c>
    inline T& at_(const std::size_t index) {
        static_assert(c == dim, "wrong number of arguments");
        return data.at(index);
    }

    template<unsigned char c, typename... Args>
    inline void initialize_(const T& initial_value, const std::size_t size, const std::size_t& i, Args... args) {
        dims[c] = i;
        initialize_<c + 1>(initial_value, size * i, args...);
    }

    template<unsigned char c>
    inline void initialize_(const T& initial_value, const std::size_t size) {
        static_assert(c == dim, "wrong number of arguments");
        data.resize(size, initial_value);
    }

  public:
    template<typename... Args>
    nvector(const T& initial_value, Args... args) {
        initialize_<0>(initial_value, 1, args...);
    }

    template<typename... Args>
    inline T& operator()(Args... args) noexcept {
        return i_<0>(0, args...);
    }

    template<typename... Args>
    inline T& at(Args... args) {
        return at_<0>(0, args...);
    }

    template<unsigned char c>
    inline std::size_t size() {
        static_assert(c < dim, "dimension index out of bounds");
        return dims[c];
    }

    inline std::size_t size(std::size_t i) { return dims.at(i); }

    void reset(const T& initial_value) { std::fill(std::begin(data), std::end(data), initial_value); }

    inline typename Storage::iterator begin() { return std::begin(data); };
    inline typename Storage::iterator end() { return std::end(data); };
    inline const typename Storage::iterator begin() const { return std::begin(data); };
    inline const typename Storage::iterator end() const { return std::end(data); };
};

#endif
