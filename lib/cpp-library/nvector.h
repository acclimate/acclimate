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

#ifndef NVECTOR_H
#define NVECTOR_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace nvector {

struct Slice;

template<typename... Args>
constexpr std::tuple<const Args&...> collect(const Args&... args) {
    return std::tuple<const Args&...>(args...);
}

template<typename... Args>
constexpr std::tuple<Args&...> collect(Args&... args) {
    return std::tuple<Args&...>(args...);
}

namespace detail {

template<typename T>
constexpr T multiply_all() {
    return 1;
}
template<typename T, typename... Args>
constexpr T multiply_all(T arg, Args... args) {
    return arg * multiply_all<T>(args...);
}

template<typename Arg>
constexpr bool all_values_equal(Arg&& arg) {
    (void)arg;
    return true;
}
template<typename Arg1, typename Arg2, typename... Args>
constexpr bool all_values_equal(Arg1&& arg1, Arg2&& arg2, Args&&... args) {
    return arg1 == arg2 && all_values_equal(std::forward<Arg2>(arg2), std::forward<Args>(args)...);
}

template<bool...>
struct bool_pack;
template<bool... v>
using all_true = std::is_same<bool_pack<true, v...>, bool_pack<v..., true>>;

template<typename... Args>
using only_for_sizes = typename std::enable_if<all_true<std::is_integral<typename std::remove_reference<Args>::type>::value...>::value>::type;

template<typename... Args>
using only_for_slices = typename std::enable_if<all_true<std::is_same<Slice, typename std::remove_reference<Args>::type>::value...>::value>::type;

template<typename A, typename B>
constexpr B&& map(A&& /* unused */, B&& b) {
    return std::move(b);
}

constexpr void pass() {}

template<typename Arg>
constexpr void pass(Arg&& /* unused */) {}

template<typename Arg, typename... Args>
constexpr void pass(Arg&& /* unused */, Args&&... args) {
    pass(std::forward<Args>(args)...);
}

template<typename Arg>
constexpr bool none_ended(Arg&& it) {
    return !it.ended();
}

template<typename Arg, typename... Args>
constexpr bool none_ended(Arg&& it, Args&&... its) {
    return !it.ended() && none_ended(std::forward<Args>(its)...);
}

template<std::size_t c, std::size_t dim, typename... Args>
struct foreach_dim {
    template<typename... Slices>
    static constexpr bool all_sizes_equal(Slices&&... args) {
        return all_values_equal(std::get<c>(args).size...) && foreach_dim<c + 1, dim>::all_sizes_equal(std::forward<Slices>(args)...);
    }
    static constexpr std::size_t total_size(const std::array<Slice, dim>& dims) { return foreach_dim<c + 1, dim>::total_size(dims) * std::get<c>(dims).size; }
    template<std::size_t... Ns>
    static constexpr std::array<std::size_t, dim> begin() {
        return foreach_dim<c + 1, dim>::template begin<Ns..., 0>();
    }
    template<std::size_t... Ns>
    static constexpr std::array<std::size_t, dim> end(const std::array<Slice, dim>& dims) {
        return foreach_dim<c + 1, dim>::template end<Ns..., c>(dims);
    }
    template<typename Tref, typename Iterator>
    static constexpr Tref dereference(std::size_t index, std::array<std::size_t, dim>& pos, const std::array<Slice, dim>& dims, Iterator&& it) {
        return foreach_dim<c + 1, dim>::template dereference<Tref>(index + (std::get<c>(pos) + std::get<c>(dims).begin) * std::get<c>(dims).stride, pos, dims,
                                                                   std::forward<Iterator>(it));
    }
    static constexpr void increase(std::array<std::size_t, dim>& pos, const std::array<Slice, dim>& dims) {
        if (std::get<dim - 1 - c>(pos) == std::get<dim - 1 - c>(dims).size - 1) {
            std::get<dim - 1 - c>(pos) = 0;
            foreach_dim<c + 1, dim>::increase(pos, dims);
        } else {
            ++std::get<dim - 1 - c>(pos);
        }
    }
    static constexpr void increase(std::array<std::size_t, dim>& pos, const std::array<Slice, dim>& dims, std::size_t by) {
        const auto d = std::ldiv(std::get<dim - 1 - c>(pos) + by, std::get<dim - 1 - c>(dims).size);
        std::get<dim - 1 - c>(pos) = d.rem;
        if (d.quot > 0) {
            foreach_dim<c + 1, dim>::increase(pos, dims, d.quot);
        }
    }
    template<std::size_t... Ns, typename Function>
    static constexpr void pass_parameters_void(const std::array<std::size_t, dim>& pos, Function&& func, Args&&... args) {
        foreach_dim<c + 1, dim, Args...>::template pass_parameters_void<Ns..., c>(pos, std::forward<Function>(func), std::forward<Args>(args)...);
    }
    template<std::size_t... Ns, typename Function>
    static constexpr bool pass_parameters(const std::array<std::size_t, dim>& pos, Function&& func, Args&&... args) {
        return foreach_dim<c + 1, dim, Args...>::template pass_parameters<Ns..., c>(pos, std::forward<Function>(func), std::forward<Args>(args)...);
    }
};

template<std::size_t dim, typename... Args>
struct foreach_dim<dim, dim, Args...> {
    template<typename... Slices>
    static constexpr bool all_sizes_equal(Slices&&... args) {
        pass(args...);
        return true;
    }
    static constexpr std::size_t total_size(const std::array<Slice, dim>& dims) {
        (void)dims;
        return 1;
    }
    template<std::size_t... Ns>
    static constexpr std::array<std::size_t, dim> begin() {
        return {Ns...};
    }
    template<std::size_t... Ns>
    static constexpr std::array<std::size_t, dim> end(const std::array<Slice, dim>& dims) {
        return {std::get<Ns>(dims).size...};
    }
    template<typename Tref, typename Iterator>
    static constexpr Tref dereference(std::size_t index, std::array<std::size_t, dim>& pos, const std::array<Slice, dim>& dims, Iterator&& it) {
        (void)pos;
        (void)dims;
        return it[index];
    }
    static constexpr void increase(std::array<std::size_t, dim>& pos, const std::array<Slice, dim>& dims) {
        std::get<0>(pos) = std::get<0>(dims).size;  // reset pos[0] to 1 beyond max value -> represents "end"
    }
    static constexpr void increase(std::array<std::size_t, dim>& pos, const std::array<Slice, dim>& dims, std::size_t by) {
        (void)by;
        std::get<0>(pos) = std::get<0>(dims).size;  // reset pos[0] to 1 beyond max value -> represents "end"
    }
    template<std::size_t... Ns, typename Function>
    static constexpr void pass_parameters_void(const std::array<std::size_t, dim>& pos, Function&& func, Args&&... args) {
        func(std::get<Ns>(pos)..., args...);
    }
    template<std::size_t... Ns, typename Function>
    static constexpr bool pass_parameters(const std::array<std::size_t, dim>& pos, Function&& func, Args&&... args) {
        return func(std::get<Ns>(pos)..., args...);
    }
};

template<typename Function, typename Additional, typename Arg, typename... Args>
constexpr bool loop_foreach_iterator(Function&& func, Additional&& temp, Arg&& it, Args&&... its) {
    (void)temp;  // temp is used to keep views for the iterators in memory for the lifespan of this function call
    while (none_ended(std::forward<Arg>(it), std::forward<Args>(its)...)) {
        if (!foreach_dim<0, Arg::dimensions, typename Arg::reference_type, typename Args::reference_type...>::template pass_parameters(
                it.pos(), std::forward<Function>(func), *it, *its...)) {
            return false;
        }
        ++it;
        pass(++its...);
    }
    return true;
}

template<typename Function, typename Arg, typename... Args>
constexpr void loop_foreach_view_parallel(Function&& func, Arg&& view, Args&&... views) {
    if (!foreach_dim<0, view.dimensions>::all_sizes_equal(view.slices(), views.slices()...)) {
        throw std::runtime_error("dimensions of different views have different sizes");
    }
#pragma omp parallel for default(shared)
    for (std::size_t i = 0; i < view.total_size(); ++i) {
        std::array<std::size_t, view.dimensions> pos = foreach_dim<0, view.dimensions>::begin();
        foreach_dim<0, view.dimensions>::increase(pos, view.slices(), i);
        foreach_dim<0, view.dimensions, typename std::remove_reference<Arg>::type::reference_type,
                    typename std::remove_reference<Args>::type::reference_type...>::
            template pass_parameters_void(pos, std::forward<Function>(func),
                                          foreach_dim<0, view.dimensions>::template dereference<typename std::remove_reference<Arg>::type::reference_type>(
                                              0, pos, view.slices(), view.data()),
                                          foreach_dim<0, views.dimensions>::template dereference<typename std::remove_reference<Args>::type::reference_type>(
                                              0, pos, views.slices(), views.data())...);
    }
}

template<typename Function, typename Arg, typename... Args>
constexpr void loop_foreach_aligned_view_parallel(Function&& func, Arg&& view, Args&&... views) {
    if (!all_values_equal(view.slices(), views.slices()...)) {
        throw std::runtime_error("views have different slices");
    }
#pragma omp parallel for default(shared)
    for (std::size_t i = 0; i < view.total_size(); ++i) {
        func(i, view[i], views[i]...);
    }
}

template<std::size_t i, std::size_t n, typename... Args>
struct foreach_helper {
    template<typename Function, std::size_t... Ns>
    static constexpr bool foreach_view(Function&& func, const std::tuple<Args...>& views) {
        return foreach_helper<i + 1, n, Args...>::template foreach_view<Function, Ns..., i>(std::forward<Function>(func), views);
    }
    template<typename Function, std::size_t... Ns>
    static constexpr void foreach_view_parallel(Function&& func, const std::tuple<Args...>& views) {
        foreach_helper<i + 1, n, Args...>::template foreach_view_parallel<Function, Ns..., i>(std::forward<Function>(func), views);
    }
    template<typename Function, std::size_t... Ns>
    static constexpr void foreach_aligned_parallel(Function&& func, const std::tuple<Args...>& views) {
        foreach_helper<i + 1, n, Args...>::template foreach_aligned_parallel<Function, Ns..., i>(std::forward<Function>(func), views);
    }
    template<typename Function, typename Splittype, std::size_t... Ns>
    static constexpr bool foreach_split(Function&& func, const std::tuple<Args...>& views) {
        return foreach_helper<i + 1, n, Args...>::template foreach_split<Function, Splittype, Ns..., i>(std::forward<Function>(func), views);
    }
    template<typename Function, typename Splittype, std::size_t... Ns>
    static constexpr void foreach_split_parallel(Function&& func, const std::tuple<Args...>& views) {
        foreach_helper<i + 1, n, Args...>::template foreach_split_parallel<Function, Splittype, Ns..., i>(std::forward<Function>(func), views);
    }
};

template<typename Function, typename... Args>
constexpr bool foreach_split_helper(Function&& func, Args&&... splits) {
    return loop_foreach_iterator(func, collect(splits...), std::begin(splits)...);
}
template<typename Function, typename... Args>
constexpr void foreach_split_helper_parallel(Function&& func, Args&&... splits) {
    loop_foreach_view_parallel(func, std::forward<Args>(splits)...);
}

template<std::size_t n, typename... Args>
struct foreach_helper<n, n, Args...> {
    template<typename Function, std::size_t... Ns>
    static constexpr bool foreach_view(Function&& func, const std::tuple<Args...>& views) {
        return loop_foreach_iterator(std::forward<Function>(func), 0, std::begin(std::get<Ns>(views))...);
    }
    template<typename Function, std::size_t... Ns>
    static constexpr void foreach_view_parallel(Function&& func, const std::tuple<Args...>& views) {
        loop_foreach_view_parallel(std::forward<Function>(func), std::forward<Args>(std::get<Ns>(views))...);
    }
    template<typename Function, std::size_t... Ns>
    static constexpr void foreach_aligned_parallel(Function&& func, const std::tuple<Args...>& views) {
        loop_foreach_aligned_view_parallel(std::forward<Function>(func), std::forward<Args>(std::get<Ns>(views))...);
    }
    template<typename Function, typename Splittype, std::size_t... Ns>
    static constexpr bool foreach_split(Function&& func, const std::tuple<Args...>& views) {
        return foreach_split_helper(std::forward<Function>(func), std::get<Ns>(views).template split<Splittype>()...);
    }
    template<typename Function, typename Splittype, std::size_t... Ns>
    static constexpr void foreach_split_parallel(Function&& func, const std::tuple<Args...>& views) {
        foreach_split_helper_parallel(std::forward<Function>(func), std::get<Ns>(views).template split<Splittype>()...);
    }
};

}  // namespace detail

template<bool... Args>
struct Split {
    static const std::size_t inner_dim = 0;
    static const std::size_t outer_dim = 0;
};

template<bool... Args>
struct Split<true, Args...> {
    static const std::size_t inner_dim = 1 + Split<Args...>::inner_dim;
    static const std::size_t outer_dim = Split<Args...>::outer_dim;
};

template<bool... Args>
struct Split<false, Args...> {
    static const std::size_t inner_dim = Split<Args...>::inner_dim;
    static const std::size_t outer_dim = 1 + Split<Args...>::outer_dim;
};

struct Slice {
    long begin = 0;
    std::size_t size = 0;
    long stride = 1;
    constexpr Slice(long begin_p, std::size_t size_p, long stride_p) : begin(begin_p), size(size_p), stride(stride_p) {}
    constexpr Slice() = default;
    constexpr bool operator==(const Slice& other) const { return begin == other.begin && size == other.size && stride == other.stride; }
};

template<typename T, std::size_t dim, class Iterator = typename std::vector<T>::iterator, typename Tref = typename std::add_lvalue_reference<T>::type>
class View {
  public:
    template<std::size_t inner_dim>
    class SplitViewHandler {
      protected:
        Iterator it;
        std::array<Slice, inner_dim> dims;

      public:
        constexpr SplitViewHandler(Iterator it_p, std::array<Slice, inner_dim> dims_p) : it(std::move(it_p)), dims(std::move(dims_p)){};
        constexpr View<T, inner_dim, Iterator, Tref> operator[](std::size_t index) { return {it + index, dims}; }
        constexpr const View<T, inner_dim, Iterator, Tref> operator[](std::size_t index) const { return {it + index, dims}; }
    };

    template<std::size_t inner_dim, std::size_t outer_dim>
    using SplitView = View<View<T, inner_dim, Iterator, Tref>, outer_dim, SplitViewHandler<inner_dim>, View<T, inner_dim, Iterator, Tref>>;

    class iterator : public std::iterator<std::output_iterator_tag, T> {
        friend class View;

      protected:
        View* view = nullptr;
        std::array<std::size_t, dim> pos_m;
        std::size_t total_index = 0;
        const std::size_t end_index = 0;
        constexpr iterator(View* view_p, std::array<std::size_t, dim> pos_p, std::size_t total_index_p, std::size_t end_index_p)
            : view(view_p), pos_m(pos_p), total_index(total_index_p), end_index(end_index_p){};

      public:
        static constexpr std::size_t dimensions = dim;
        using type = T;
        using reference_type = Tref;

        static constexpr iterator begin(View* view_p) { return {view_p, detail::foreach_dim<0, dim>::begin(), 0, view_p->total_size()}; }
        static constexpr iterator end(View* view_p) {
            return {view_p, detail::foreach_dim<0, dim>::end(view_p->dims), view_p->total_size(), view_p->total_size()};
        }
        constexpr bool ended() const { return total_index == end_index; }
        constexpr std::size_t get_end_index() const { return end_index; }
        constexpr std::size_t get_index() const { return total_index; }
        constexpr const std::array<std::size_t, dim>& pos() const { return pos_m; }
        constexpr Tref operator*() { return detail::foreach_dim<0, dim>::template dereference<Tref>(0, pos_m, view->dims, view->it); }
        constexpr iterator operator++() {
            detail::foreach_dim<0, dim>::increase(pos_m, view->dims);
            ++total_index;
            return *this;
        }
        constexpr iterator operator+(std::size_t i) const {
            if (total_index + i >= end_index) {
                return end(view);
            }
            std::array<std::size_t, dim> pos_l = pos_m;
            detail::foreach_dim<0, dim>::increase(pos_l, view->dims, i);
            return iterator(view, std::move(pos_l), total_index + i, end_index);
        }
        constexpr bool operator<(const iterator& other) const { return total_index < other.total_index; }
        constexpr bool operator<(std::size_t other) const { return total_index < other; }
        constexpr bool operator==(const iterator& other) const { return total_index == other.total_index; }
        constexpr bool operator!=(const iterator& other) const { return total_index != other.total_index; }
    };
    friend class iterator;

    class const_iterator : public std::iterator<std::output_iterator_tag, T> {
        friend class View;

      protected:
        const View* const view = nullptr;
        std::array<std::size_t, dim> pos_m;
        std::size_t total_index = 0;
        const std::size_t end_index = 0;
        constexpr const_iterator(const View* const view_p, std::array<std::size_t, dim> pos_p, std::size_t total_index_p, std::size_t end_index_p)
            : view(view_p), pos_m(pos_p), total_index(total_index_p), end_index(end_index_p){};

      public:
        static constexpr std::size_t dimensions = dim;
        using type = T;
        using reference_type = Tref;

        static constexpr const_iterator begin(const View* const view_p) { return {view_p, detail::foreach_dim<0, dim>::begin(), 0, view_p->total_size()}; }
        static constexpr const_iterator end(const View* const view_p) {
            return {view_p, detail::foreach_dim<0, dim>::end(view_p->dims), view_p->total_size(), view_p->total_size()};
        }
        constexpr bool ended() const { return total_index == end_index; }
        constexpr std::size_t get_end_index() const { return end_index; }
        constexpr std::size_t get_index() const { return total_index; }
        constexpr const std::array<std::size_t, dim>& pos() const { return pos_m; }
        constexpr const Tref operator*() { return detail::foreach_dim<0, dim>::template dereference<Tref>(0, pos_m, view->dims, view->it); }
        constexpr const_iterator operator++() {
            detail::foreach_dim<0, dim>::increase(pos_m, view->dims);
            ++total_index;
            return *this;
        }
        constexpr const_iterator operator+(std::size_t i) const {
            if (total_index + i >= end_index) {
                return end(view);
            }
            std::array<std::size_t, dim> pos_l = pos_m;
            detail::foreach_dim<0, dim>::increase(pos_l, view->dims, i);
            return const_iterator(view, std::move(pos_l), total_index + i, end_index);
        }
        constexpr bool operator<(const const_iterator& other) const { return total_index < other.total_index; }
        constexpr bool operator<(std::size_t other) const { return total_index < other; }
        constexpr bool operator==(const const_iterator& other) const { return total_index == other.total_index; }
        constexpr bool operator!=(const const_iterator& other) const { return total_index != other.total_index; }
    };
    friend class const_iterator;

  protected:
    std::array<Slice, dim> dims;
    Iterator it;

    template<std::size_t c, typename... Args>
    constexpr Tref i_(std::size_t index, const std::size_t& i, Args&&... args) noexcept {
        return i_<c + 1>(index + (i + std::get<c>(dims).begin) * std::get<c>(dims).stride, std::forward<Args>(args)...);
    }

    template<std::size_t c>
    constexpr Tref i_(std::size_t index) noexcept {
        static_assert(c == dim, "wrong number of arguments");
        return it[index];
    }

    template<std::size_t c, typename... Args>
    constexpr const Tref i_(std::size_t index, const std::size_t& i, Args&&... args) const noexcept {
        return i_<c + 1>(index + (i + std::get<c>(dims).begin) * std::get<c>(dims).stride, std::forward<Args>(args)...);
    }

    template<std::size_t c>
    constexpr const Tref i_(std::size_t index) const noexcept {
        static_assert(c == dim, "wrong number of arguments");
        return it[index];
    }

    template<std::size_t c, typename... Args>
    constexpr Tref at_(std::size_t index, const std::size_t& i, Args&&... args) {
        if (i >= std::get<c>(dims).size) {
            throw std::out_of_range("index out of bounds");
        }
        return at_<c + 1>(index + (i + std::get<c>(dims).begin) * std::get<c>(dims).stride, std::forward<Args>(args)...);
    }

    template<std::size_t c>
    constexpr Tref at_(std::size_t index) {
        static_assert(c == dim, "wrong number of arguments");
        return it[index];
    }

    template<std::size_t c, typename... Args>
    constexpr const Tref at_(std::size_t index, const std::size_t& i, Args&&... args) const {
        if (i >= std::get<c>(dims).size) {
            throw std::out_of_range("index out of bounds");
        }
        return at_<c + 1>(index + (i + std::get<c>(dims).begin) * std::get<c>(dims).stride, std::forward<Args>(args)...);
    }

    template<std::size_t c>
    constexpr const Tref at_(std::size_t index) const {
        static_assert(c == dim, "wrong number of arguments");
        return it[index];
    }

    template<std::size_t c, typename... Args>
    constexpr void initialize_slices(const Slice& i, Args&&... args) {
        std::get<c>(dims) = i;
        initialize_slices<c + 1>(std::forward<Args>(args)...);
    }

    template<std::size_t c>
    constexpr void initialize_slices() {
        static_assert(c == dim, "wrong number of arguments");
    }

    template<std::size_t c, typename... Args>
    constexpr void initialize_sizes(std::size_t size, Args&&... args) {
        std::get<c>(dims) = {0, size, detail::multiply_all<int>(std::forward<Args>(args)...)};
        initialize_sizes<c + 1>(std::forward<Args>(args)...);
    }

    template<std::size_t c>
    constexpr void initialize_sizes() {
        static_assert(c == dim, "wrong number of arguments");
    }

    template<std::size_t c, std::size_t inner_c, std::size_t inner_dim, std::size_t outer_c, std::size_t outer_dim, bool... Args>
    struct splitter {
        static constexpr SplitView<inner_dim, outer_dim> split(Iterator it,
                                                               std::array<Slice, inner_dim> inner_dims,
                                                               std::array<Slice, outer_dim> outer_dims,
                                                               const std::array<Slice, dim>& dims);
    };

    template<std::size_t c, std::size_t inner_c, std::size_t inner_dim, std::size_t outer_c, std::size_t outer_dim, bool... Args>
    struct splitter<c, inner_c, inner_dim, outer_c, outer_dim, true, Args...> {
        static constexpr SplitView<inner_dim, outer_dim> split(Iterator it,
                                                               std::array<Slice, inner_dim> inner_dims,
                                                               std::array<Slice, outer_dim> outer_dims,
                                                               const std::array<Slice, dim>& dims) {
            std::get<inner_c>(inner_dims) = std::get<c>(dims);
            return splitter<c + 1, inner_c + 1, inner_dim, outer_c, outer_dim, Args...>::split(std::move(it), std::move(inner_dims), std::move(outer_dims),
                                                                                               dims);
        }
    };

    template<std::size_t c, std::size_t inner_c, std::size_t inner_dim, std::size_t outer_c, std::size_t outer_dim, bool... Args>
    struct splitter<c, inner_c, inner_dim, outer_c, outer_dim, false, Args...> {
        static constexpr SplitView<inner_dim, outer_dim> split(Iterator it,
                                                               std::array<Slice, inner_dim> inner_dims,
                                                               std::array<Slice, outer_dim> outer_dims,
                                                               const std::array<Slice, dim>& dims) {
            std::get<outer_c>(outer_dims) = std::get<c>(dims);
            return splitter<c + 1, inner_c, inner_dim, outer_c + 1, outer_dim, Args...>::split(std::move(it), std::move(inner_dims), std::move(outer_dims),
                                                                                               dims);
        }
    };

    template<std::size_t c, std::size_t inner_c, std::size_t inner_dim, std::size_t outer_c, std::size_t outer_dim>
    struct splitter<c, inner_c, inner_dim, outer_c, outer_dim> {
        static constexpr SplitView<inner_dim, outer_dim> split(Iterator it,
                                                               std::array<Slice, inner_dim> inner_dims,
                                                               std::array<Slice, outer_dim> outer_dims,
                                                               const std::array<Slice, dim>& dims) {
            (void)dims;
            return SplitView<inner_dim, outer_dim>(std::move(SplitViewHandler<inner_dim>(std::move(it), std::move(inner_dims))), std::move(outer_dims));
        }
    };

    template<typename Splittype>
    struct splitter_helper {};

    template<bool... Args>
    struct splitter_helper<Split<Args...>> {
        static constexpr SplitView<Split<Args...>::inner_dim, Split<Args...>::outer_dim> split(Iterator it, const std::array<Slice, dim>& dims) {
            const std::size_t inner_dim = Split<Args...>::inner_dim;
            const std::size_t outer_dim = Split<Args...>::outer_dim;
            static_assert(inner_dim + outer_dim == dim, "wrong number of arguments");
            std::array<Slice, inner_dim> inner_dims;
            std::array<Slice, outer_dim> outer_dims;
            return splitter<0, 0, inner_dim, 0, outer_dim, Args...>::split(std::move(it), std::move(inner_dims), std::move(outer_dims), std::move(dims));
        }
    };

  public:
    static constexpr std::size_t dimensions = dim;
    using type = T;
    using reference_type = Tref;
    using iterator_type = Iterator;
    template<std::size_t inner_dim>
    using split_type = View<T, inner_dim, Iterator, Tref>;

    constexpr View() = default;
    View(const View&) = delete;
    constexpr View(View&&) noexcept = default;
    constexpr View(Iterator it_p, std::array<Slice, dim> dims_p) : it(std::move(it_p)), dims(std::move(dims_p)) {}

    template<typename... Args, typename detail::only_for_sizes<Args...>* = nullptr>
    explicit constexpr View(Iterator it_p, Args&&... args) : it(std::move(it_p)) {
        initialize_sizes<0>(std::forward<Args>(args)...);
    }

    template<typename... Args, typename detail::only_for_slices<Args...>* = nullptr>
    explicit constexpr View(Iterator it_p, Args&&... args) : it(std::move(it_p)) {
        initialize_slices<0>(std::forward<Args>(args)...);
    }

    template<bool... Args>
    constexpr SplitView<Split<Args...>::inner_dim, Split<Args...>::outer_dim> split() {
        return splitter_helper<Split<Args...>>::split(it, dims);
    }

    template<typename Splittype>
    constexpr SplitView<Splittype::inner_dim, Splittype::outer_dim> split() {
        return splitter_helper<Splittype>::split(it, dims);
    }

    template<bool... Args>
    constexpr SplitView<Split<Args...>::inner_dim, Split<Args...>::outer_dim> split() const {
        return splitter_helper<Split<Args...>>::split(it, dims);
    }

    template<typename Splittype>
    constexpr SplitView<Splittype::inner_dim, Splittype::outer_dim> split() const {
        return splitter_helper<Splittype>::split(it, dims);
    }

    template<typename Function>
    constexpr bool foreach_element(Function&& func) {
        iterator it_l = begin();
        for (; !it_l.ended(); ++it_l) {
            if (!detail::foreach_dim<0, dim, Tref>::template pass_parameters(it_l.pos(), std::forward<Function>(func), *it_l)) {
                return false;
            }
        }
        return true;
    }

    constexpr Iterator& data() { return it; }
    constexpr const Iterator& data() const { return it; }

    template<typename Function>
    constexpr void foreach_parallel(Function&& func) {
        const iterator bg = begin();
#pragma omp parallel for default(shared)
        for (std::size_t i = 0; i < bg.get_end_index(); ++i) {
            iterator it_l = bg + i;
            detail::foreach_dim<0, dim, Tref>::template pass_parameters_void(it_l.pos(), std::forward<Function>(func), *it_l);
        }
    }

    constexpr void swap_dims(std::size_t i, std::size_t j) {
        if (i >= dims.size() || j >= dims.size()) {
            throw std::out_of_range("index out of bounds");
        }
        std::swap(dims[i], dims[j]);
    }

    template<typename... Args>
    constexpr Tref operator()(Args&&... args) noexcept {
        return i_<0>(0, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr const Tref operator()(Args&&... args) const noexcept {
        return i_<0>(0, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr Tref at(Args&&... args) {
        return at_<0>(0, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr const Tref at(Args&&... args) const {
        return at_<0>(0, std::forward<Args>(args)...);
    }

    template<std::size_t c>
    constexpr const Slice& slice() const {
        static_assert(c < dim, "dimension index out of bounds");
        return std::get<c>(dims);
    }

    constexpr const Slice& slice(std::size_t i) const { return dims.at(i); }
    constexpr const std::array<Slice, dim>& slices() const { return dims; }

    template<std::size_t c>
    constexpr std::size_t size() const {
        static_assert(c < dim, "dimension index out of bounds");
        return std::get<c>(dims).size;
    }
    constexpr std::size_t size(std::size_t i) const { return dims.at(i).size; }

    constexpr std::size_t total_size() const { return detail::foreach_dim<0, dim>::total_size(dims); }
    constexpr Tref operator[](std::size_t i) { return it[i]; }
    constexpr const Tref operator[](std::size_t i) const { return it[i]; }

    constexpr void reset(const T& initial_value) { std::fill(iterator::begin(this), iterator::end(this), initial_value); }

    constexpr iterator begin() { return iterator::begin(this); }
    constexpr iterator end() { return iterator::end(this); }
    constexpr const_iterator begin() const { return const_iterator::begin(this); }
    constexpr const_iterator end() const { return const_iterator::end(this); }
};

template<typename T, std::size_t dim, class Storage = std::vector<T>, typename Iterator = typename Storage::iterator>
class Vector : public View<T, dim, Iterator> {
  protected:
    using View<T, dim, Iterator>::it;
    Storage data_m;

  public:
    using View<T, dim, Iterator>::dimensions;
    using View<T, dim, Iterator>::reference_type;
    using View<T, dim, Iterator>::type;
    using View<T, dim, Iterator>::iterator_type;
    using View<T, dim, Iterator>::split_type;

    constexpr Vector() { it = std::begin(data_m); }
    constexpr Vector(const T& initial_value, std::array<Slice, dim> dims_p) : View<T, dim, Iterator>(std::begin(data_m), std::move(dims_p)) {
        data_m.resize(this->total_size(), initial_value);
        it = std::begin(data_m);
    }

    template<typename... Args, typename detail::only_for_sizes<Args...>* = nullptr>
    explicit Vector(const T& initial_value, Args&&... args) {
        this->template initialize_sizes<0>(std::forward<Args>(args)...);
        data_m.resize(detail::multiply_all(std::forward<Args>(args)...), initial_value);
        it = std::begin(data_m);
    }

    template<typename... Args, typename detail::only_for_slices<Args...>* = nullptr>
    explicit Vector(const T& initial_value, Args&&... args) {
        this->template initialize_slices<0>(std::forward<Args>(args)...);
        data_m.resize(detail::multiply_all(args.size...), initial_value);
        it = std::begin(data_m);
    }

    template<typename... Args, typename detail::only_for_sizes<Args...>* = nullptr>
    explicit Vector(Storage data_p, Args&&... args) : data_m(std::move(data_p)) {
        this->template initialize_sizes<0>(std::forward<Args>(args)...);
        if (detail::multiply_all<std::size_t>(std::forward<Args>(args)...) != data_m.size()) {
            throw std::runtime_error("wrong size of underlying data");
        }
        it = std::begin(data_m);
    }

    template<typename... Args, typename detail::only_for_slices<Args...>* = nullptr>
    explicit Vector(Storage data_p, Args&&... args) : data_m(std::move(data_p)) {
        this->template initialize_slices<0>(std::forward<Args>(args)...);
        if (detail::multiply_all<std::size_t>(args.size...) != data_m.size()) {
            throw std::runtime_error("wrong size of underlying data");
        }
        it = std::begin(data_m);
    }

    template<typename... Args>
    constexpr void resize(const T& initial_value, Args&&... args) {
        this->template initialize_sizes<0>(std::forward<Args>(args)...);
        data_m.resize(detail::multiply_all(std::forward<Args>(args)...), initial_value);
        it = std::begin(data_m);
    }

    constexpr void reset(const T& initial_value) { std::fill(std::begin(data_m), std::end(data_m), initial_value); }

    constexpr Storage& data() { return data_m; }
    constexpr const Storage& data() const { return data_m; }
};

template<typename... Args, typename Function>
constexpr bool foreach_view(const std::tuple<Args...>& views, Function&& func) {
    return detail::foreach_helper<0, std::tuple_size<std::tuple<Args...>>::value, Args...>::foreach_view(std::forward<Function>(func), views);
}

template<typename... Args, typename Function>
constexpr void foreach_view_parallel(const std::tuple<Args...>& views, Function&& func) {
    detail::foreach_helper<0, std::tuple_size<std::tuple<Args...>>::value, Args...>::foreach_view_parallel(std::forward<Function>(func), views);
}

template<typename... Args, typename Function>
constexpr void foreach_aligned_parallel(const std::tuple<Args...>& views, Function&& func) {
    detail::foreach_helper<0, std::tuple_size<std::tuple<Args...>>::value, Args...>::foreach_aligned_parallel(std::forward<Function>(func), views);
}

template<typename Splittype, typename... Args, typename Function>
constexpr bool foreach_split(const std::tuple<Args...>& views, Function&& func) {
    return detail::foreach_helper<0, sizeof...(Args), Args...>::template foreach_split<Function, Splittype>(std::forward<Function>(func), views);
}

template<typename Splittype, typename... Args, typename Function>
constexpr void foreach_split_parallel(const std::tuple<Args...>& views, Function&& func) {
    detail::foreach_helper<0, sizeof...(Args), Args...>::template foreach_split_parallel<Function, Splittype>(std::forward<Function>(func), views);
}

}  // namespace nvector

#endif
