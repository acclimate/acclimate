/*
  Copyright (C) 2019 Sven Willner <sven.willner@gmail.com>

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

#ifndef CUDATOOLS_H
#define CUDATOOLS_H

#include <algorithm>
#include <cstring>
#include <fstream>
#include "nvector.h"

namespace cudatools {

class exception : public std::runtime_error {
  public:
    explicit exception(const char* msg) : std::runtime_error(msg) {}
    explicit exception(const std::string& msg) : std::runtime_error(msg) {}
};

template<typename T, bool only_device = false>
class vector {
  protected:
    T* data = nullptr;
    std::size_t size_m = 0;
    inline void allocate(std::size_t size_p) {
#if defined(USE_CUDA) && defined(__CUDACC__)
        cudaError_t res;
        if (only_device) {
            res = cudaMalloc(&data, size_p * sizeof(T));
        } else {
            res = cudaMallocManaged(&data, size_p * sizeof(T));
        }
        cudaDeviceSynchronize();
        if (res != cudaSuccess) {
            throw cudatools::exception(cudaGetErrorString(res));
        }
#else
        data = static_cast<T*>(std::malloc(size_p * sizeof(T)));
        if (data == nullptr) {
            throw std::bad_alloc();
        }
#endif
        size_m = size_p;
    }

  public:
    using iterator = T*;

    vector() = default;
    vector(std::size_t size_p) { allocate(size_p); }
    vector(const vector&) = delete;
    vector(vector&&) noexcept = default;
    ~vector() { reset(); }
    const T* begin() const { return data; }
    T* begin() { return data; }
    const T* end() const { return data + size_m; }
    T* end() { return data + size_m; }
    inline void resize(std::size_t size_p) {
        reset();
        allocate(size_p);
    }
    inline void resize(std::size_t size_p, const T& value) {
        resize(size_p);
        if (only_device) {
            (void)value;
        } else {
            std::fill(data, data + size_p, value);
        }
    }
    inline void reset() {
        if (size_m > 0) {
#if defined(USE_CUDA) && defined(__CUDACC__)
            cudaDeviceSynchronize();
            cudaFree(data);
#else
            std::free(data);
#endif
            size_m = 0;
        }
    }
    inline std::size_t size() const { return size_m; }
    inline void set(const T* src) {
#if defined(USE_CUDA) && defined(__CUDACC__)
        if (only_device) {
            cudaMemcpy(data, src, size_m * sizeof(T), cudaMemcpyHostToDevice);
        } else {
#else
        {
#endif
            std::memcpy(data, src, size_m * sizeof(T));
        }
    }
    inline void read(std::ifstream& file) {
#if defined(USE_CUDA) && defined(__CUDACC__)
        if (only_device) {
            auto tmp = static_cast<T*>(std::malloc(size_m * sizeof(T)));
            if (tmp == nullptr) {
                throw std::bad_alloc();
            }
            file.read(reinterpret_cast<char*>(tmp), size_m * sizeof(T));
            set(tmp);
            std::free(tmp);
        } else {
#else
        {
#endif
            file.read(reinterpret_cast<char*>(data), size_m * sizeof(T));
        }
    }
    inline T& operator[](std::size_t i) { return data[i]; }
    inline const T& operator[](std::size_t i) const { return data[i]; }
};

}  // namespace cudatools

#if defined(USE_CUDA) && defined(__CUDACC__)
#define CUDA_DEVICE __device__
#define CUDA_GLOBAL __global__
#else
#define CUDA_DEVICE
#define CUDA_GLOBAL
#endif

namespace nvector {
namespace detail_gpu {

#if defined(USE_CUDA) && defined(__CUDACC__)
template<typename Function, typename Arg, typename... Args>
__global__ void device_func(Function func, std::size_t n, Arg* it, Args*... its) {
    const auto i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        func(i, it[i], its[i]...);
    }
}
#endif

template<typename Function, typename Arg, typename... Args>
inline void loop_foreach_aligned_view_gpu(Function&& func, Arg&& view, Args&&... views) {
#if defined(USE_CUDA) && defined(__CUDACC__)
    if (!nvector::detail::all_values_equal(view.slices(), views.slices()...)) {
        throw std::runtime_error("views have different slices");
    }
    constexpr auto block_size = 256;
    device_func<<<(view.total_size() + block_size - 1) / block_size, block_size>>>(func, view.total_size(), &view[0], &views[0]...);
#else
    nvector::detail::loop_foreach_aligned_view_parallel(func, std::forward<Arg>(view), std::forward<Args>(views)...);
#endif
}

template<std::size_t i, std::size_t n, typename... Args>
struct foreach_helper {
    template<typename Function, std::size_t... Ns>
    static constexpr void foreach_aligned_gpu(Function&& func, const std::tuple<Args...>& views) {
        foreach_helper<i + 1, n, Args...>::template foreach_aligned_gpu<Function, Ns..., i>(std::forward<Function>(func), views);
    }
};

template<std::size_t n, typename... Args>
struct foreach_helper<n, n, Args...> {
    template<typename Function, std::size_t... Ns>
    static constexpr void foreach_aligned_gpu(Function&& func, const std::tuple<Args...>& views) {
        loop_foreach_aligned_view_gpu(std::forward<Function>(func), std::forward<Args>(std::get<Ns>(views))...);
    }
};
}  // namespace detail_gpu

template<typename... Args, typename Function>
constexpr void foreach_aligned_gpu(const std::tuple<Args...>& views, Function&& func) {
    detail_gpu::foreach_helper<0, std::tuple_size<std::tuple<Args...>>::value, Args...>::foreach_aligned_gpu(std::forward<Function>(func), views);
}

}  // namespace nvector

#endif
