/*
  Miscellaneous helpers from various sources
*/

#ifndef MISC_H
#define MISC_H

//
// Scope exit helper
// Call on scope exit (like finally)
// Example:
//   FILE * fp = fopen("test.out", "wb");
//   SCOPE_EXIT(fclose(fp));
//
template<typename F>
struct ScopeExit {
    ScopeExit(F f) : f(f) {}
    ~ScopeExit() { f(); }
    F f;
};

template<typename F>
ScopeExit<F> MakeScopeExit(F f) {
    return ScopeExit<F>(f);
};

#define SCOPE_EXIT(code) auto STRING_JOIN2(scope_exit_, __LINE__) = MakeScopeExit([=]() { code; })

//
// Some definitions of C++14+ constructs
//
template<class T>
using remove_const_t = typename remove_const<T>::type;
template<class T>
using remove_reference_t = typename remove_reference<T>::type;
template<class T>
using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

template<class C>
auto cbegin(const C& container) -> decltype(std::begin(container)) {
    return std::begin(container);
}
template<class C>
auto cend(const C& container) -> decltype(std::end(container)) {
    return std::end(container);
}

//
// Debug output
//
template<typename... Types>
void debug(Types... args);
template<typename Type1, typename... Types>
void debug(Type1 arg1, Types... rest) {
    std::cout << typeid(arg1).name() << ": " << arg1 << std::endl;
    debug(rest...);
}
template<>
void debug() {}

//
// Convenient constructor for std::array
// from https://a4z.bitbucket.io/blog/2017/01/19/A-convenient-constructor-for-std::array.html
//
template<typename... Args>
std::array<typename std::common_type<Args...>::type, sizeof...(Args)> array(Args&&... args) {
    return std::array<typename std::common_type<Args...>::type, sizeof...(Args)>{std::forward<Args>(args)...};
}

//
// iterator-zip
//
template<typename... Args>
class ContainerBundle {
  public:
    using iterator = IteratorBundle<typename Args::iterator...>;
    using iter_coll = std::tuple<typename Args::iterator...>;
    ContainerBundle(typename std::add_pointer<Args>::type... args) : dat{args...}, bg{args->begin()...}, nd{args->end()...} {}
    ~ContainerBundle() = default;
    ContainerBundle(const ContainerBundle&) = delete;
    ContainerBundle(ContainerBundle&&) = default;
    inline iterator begin() { return bg; }
    inline iterator end() const { return nd; }

  private:
    std::tuple<typename std::add_pointer<Args>::type...> dat;
    iterator bg, nd;
};

template<typename... Itr>
class IteratorBundle {
  public:
    using value_type = std::tuple<typename Itr::value_type...>;
    using internal_type = std::tuple<Itr...>;
    IteratorBundle() = default;  // etc.
    bool operator==(const IteratorBundle& it) const { return loc == it.loc; }
    // special implementation insisting that all
    // elements in the bundle compare unequal. This ensures proper
    // function for containers of different sizes.
    bool operator!=(const IteratorBundle& it) const;
    inline value_type operator*() { return deref(); }
    inline IteratorBundle& operator++() {
        advance_them<0, sizeof...(Itr)>::eval(loc);
        return
    }
    template<bool... b>
    struct static_all_of;
    template<bool... tail>
    struct static_all_of<true, tail...> : static_all_of<tail...> {};
    // no need to look further if first argument is false
    template<bool... tail>
    struct static_all_of<false, tail...> : std::false_type {};
    template<>
    struct static_all_of<> : std::true_type {};
    template<typename... Args>
    ContainerBundle<typename std::remove_pointer<Args>::type...> zip(Args... args) {
        static_assert(static_all_of<std::is_pointer<Args>::value...>::value, "Each argument to zip must be a pointer to a container! ");
        return {args...};
    }
};

//
// output stream << std::tuple
//
namespace details {
template<std::size_t i>
struct ostream_tuple_helper {
    template<typename T>
    static std::ostream& write(std::ostream& os, T&& t) {
        return ostream_tuple_helper<i - 1>::write(os, t) << ", " << std::get<i - 1>(t);
    }
};

template<>
struct ostream_tuple_helper<1> {
    template<typename T>
    static std::ostream& write(std::ostream& os, T&& t) {
        return os << std::get<0>(t);
    }
};

template<>
struct ostream_tuple_helper<0> {
    template<typename T>
    static std::ostream& write(std::ostream& os, T&& t) {
        return os;
    }
};
}

template<typename... Args>
std::ostream& operator<<(std::ostream& os, const std::tuple<Args...>& t) {
    return details::ostream_tuple_helper<std::tuple_size<std::tuple<Args...>>::value>::write(os, t);
}

#endif
