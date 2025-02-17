// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_H
#define ACCLIMATE_H

#include <features.h>  // for __STRING

#include <cassert>
#include <memory>
#include <ostream>

#include "types.h"  // IWYU pragma: export

namespace acclimate {

#define ACCLIMATE_ADD_ITERATION_STEPS         \
    ADD_ENUM_ITEM(INITIALIZATION)             \
    ADD_ENUM_ITEM(SCENARIO)                   \
    ADD_ENUM_ITEM(CONSUMPTION_AND_PRODUCTION) \
    ADD_ENUM_ITEM(EXPECTATION)                \
    ADD_ENUM_ITEM(PURCHASE)                   \
    ADD_ENUM_ITEM(INVESTMENT)                 \
    ADD_ENUM_ITEM(OUTPUT)                     \
    ADD_ENUM_ITEM(CLEANUP)                    \
    ADD_ENUM_ITEM(UNDEFINED)  // to be used when function is not used yet

#define ADD_ENUM_ITEM(e) e,
enum class IterationStep : unsigned char { ACCLIMATE_ADD_ITERATION_STEPS };
#undef ADD_ENUM_ITEM

#define ADD_ENUM_ITEM(e) __STRING(e),
constexpr std::array<const char*, static_cast<int>(IterationStep::UNDEFINED) + 1> ITERATION_STEP_NAMES = {ACCLIMATE_ADD_ITERATION_STEPS};
#undef ADD_ENUM_ITEM

#undef ACCLIMATE_ADD_ITERATIONS_STEPS

class Model;
std::string timeinfo(const Model& m);
IterationStep current_step(const Model& m);

namespace log {

namespace detail {

template<class Stream, typename Arg>
inline void to_stream(Stream& s, Arg&& arg) {
    s << arg;
}

template<class Stream, typename Arg, typename... Args>
inline void to_stream(Stream& s, Arg&& arg, Args&&... args) {
    s << arg;
    to_stream(s, std::forward<Args>(args)...);
}

template<typename... Args>
inline void output(Args&&... args) {
#pragma omp critical(output)
    {
        to_stream(std::cout, std::forward<Args>(args)...);
        std::cout << std::endl;
    }
}

template<class>
struct to_void {
    typedef void type;
};
template<class T, class = void, class = void>
struct has_model_and_name : std::false_type {};
template<class T>
struct has_model_and_name<T, typename to_void<decltype(std::declval<T>().model())>::type, typename to_void<decltype(std::declval<T>().name())>::type>
    : std::true_type {};

}  // namespace detail

template<typename Arg, typename... Args>
inline acclimate::exception error(Arg&& arg, Args&&... args) {
    std::ostringstream ss;
    if constexpr (std::is_pointer<Arg>::value && detail::has_model_and_name<typename std::remove_pointer<Arg>::type>::value) {
        detail::to_stream(ss, timeinfo(*arg->model()), ", ", arg->name(), ": ", std::forward<Args>(args)...);
    } else {
        detail::to_stream(ss, std::forward<Arg>(arg), std::forward<Args>(args)...);
    }
    return acclimate::exception(ss.str());
}

template<typename Arg, typename... Args>
inline void warning(Arg&& arg, Args&&... args) {
    if constexpr (Options::DEBUGGING) {
        if constexpr (std::is_pointer<Arg>::value && detail::has_model_and_name<typename std::remove_pointer<Arg>::type>::value) {
            detail::output(timeinfo(*arg->model()), ", ", arg->name(), " Warning: ", std::forward<Args>(args)...);
        } else {
            detail::output("Warning: ", std::forward<Arg>(arg), std::forward<Args>(args)...);
        }
    }
}

template<typename Arg, typename... Args>
inline void info(Arg&& arg, Args&&... args) {
    if constexpr (Options::DEBUGGING) {
        if constexpr (std::is_pointer<Arg>::value && detail::has_model_and_name<typename std::remove_pointer<Arg>::type>::value) {
            detail::output(timeinfo(*arg->model()), ", ", arg->name(), ": ", std::forward<Args>(args)...);
        } else {
            detail::output(std::forward<Arg>(arg), std::forward<Args>(args)...);
        }
    }
}

template<typename Arg, typename... Args>
inline void debug(Arg&& arg, Args&&... args) {
    if constexpr (std::is_pointer<Arg>::value && detail::has_model_and_name<typename std::remove_pointer<Arg>::type>::value) {
        detail::output(timeinfo(*arg->model()), ", ", arg->name(), ": ", std::forward<Args>(args)...);
    } else {
        detail::output(std::forward<Arg>(arg), std::forward<Args>(args)...);
    }
}

}  // namespace log

namespace debug {

template<class Caller>
inline void assertstep(const Caller* c, IterationStep s) {
    if constexpr (Options::DEBUGGING) {
        if (current_step(*c->model()) != s) {
            throw log::error(c, "should be in ", ITERATION_STEP_NAMES[static_cast<int>(s)], " step");
        }
    } else {
        (void)c;
        (void)s;
    }
}

template<class Caller>
inline void assertstepnot(const Caller* c, IterationStep s) {
    if constexpr (Options::DEBUGGING) {
        if (current_step(*c->model()) == s) {
            throw log::error(c, "should NOT be in ", ITERATION_STEP_NAMES[static_cast<int>(s)], " step");
        }
    } else {
        (void)c;
        (void)s;
    }
}

template<class Caller>
inline void assertstepor(const Caller* c, IterationStep s1, IterationStep s2) {
    if constexpr (Options::DEBUGGING) {
        if (current_step(*c->model()) != s1 && current_step(*c->model()) != s2) {
            throw log::error(c, "should be in ", ITERATION_STEP_NAMES[static_cast<int>(s1)], " or ", ITERATION_STEP_NAMES[static_cast<int>(s2)], " step");
        }
    } else {
        (void)c;
        (void)s1;
        (void)s2;
    }
}

}  // namespace debug

}  // namespace acclimate

#endif
