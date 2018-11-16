/*
  Copyright (C) 2016-2018 Sven Willner <sven.willner@gmail.com>

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

#ifndef SETTINGSNODE_H
#define SETTINGSNODE_H

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "settingsnode/inner.h"

namespace settings {

class exception : public std::runtime_error {
  public:
    explicit exception(const std::string& s) : std::runtime_error(s){};
};

class hstring {
  public:
    using base_type = std::string;
    using hash_type = uint64_t;

  protected:
    const base_type str_m;
    const hash_type hash_m;
    hstring(const base_type& str_p, hash_type hash_p) : str_m(str_p), hash_m(hash_p){};

  public:
    static constexpr hash_type hash(const char* str, hash_type prev = 5381) { return *str ? hash(str + 1, prev * 33 + *str) : prev; }
    static hstring null() { return hstring("", 0); }
    explicit hstring(const base_type& str_p) : str_m(str_p), hash_m(hash(str_p.c_str())){};
    operator hash_type() const { return hash_m; }
    operator const base_type&() const { return str_m; }
    hash_type operator^(hash_type other) const { return hash_m * 5381 * 5381 + other; }
    friend std::ostream& operator<<(std::ostream& lhs, const hstring& rhs) { return lhs << rhs.str_m; }
};

class SettingsNode {
  protected:
    struct Path {
        std::string name;
        int index;
        std::shared_ptr<Path> parent;
    };
    std::shared_ptr<Path> path;
    std::shared_ptr<Inner> inner;
    SettingsNode(Inner* inner_p, const std::shared_ptr<Path>& path_p) : inner(inner_p), path(path_p){};

    template<class T, class S = void>
    struct enable_if_type {
        typedef S type;
    };
    template<typename T, typename enable = void>
    struct basetype {
        using type = T;
    };
    template<typename T>
    struct basetype<T, typename enable_if_type<typename T::base_type>::type> {
        using type = typename T::base_type;
    };

    inline void check() const {
        if (empty()) {
            throw settings::exception("Settings '" + get_path() + "' not found");
        }
    }

    inline void check_scalar() const {
        if (!is_scalar()) {
            throw settings::exception("Settings '" + get_path() + "' is not a scalar value");
        }
    }

    template<typename T>
    T as_inner() const;

    template<typename T>
    inline T as_inner(const T& fallback) const {
        if (empty()) {
            return fallback;
        }
        return as_inner<T>();
    }

  public:
    class Map {
        friend class SettingsNode;

      protected:
        const std::shared_ptr<Path> path;
        std::unique_ptr<Inner::map_iterator> begin_m;
        std::unique_ptr<Inner::map_iterator> end_m;
        Map(Inner::map_iterator* begin_p, Inner::map_iterator* end_p, const std::shared_ptr<Path>& path_p) : begin_m(begin_p), end_m(end_p), path(path_p){};

      public:
        class iterator {
            friend class Map;

          protected:
            const std::shared_ptr<Path>& path;
            Inner::map_iterator* const it;
            iterator(Inner::map_iterator* const it_p, const std::shared_ptr<Path>& path_p) : it(it_p), path(path_p){};

          public:
            void operator++() { it->next(); }
            std::pair<std::string, SettingsNode> operator*() const {
                const std::string& name = it->name();
                return std::make_pair(name, SettingsNode(it->value(), std::make_shared<Path>(Path{name, -1, path})));
            }
            bool operator==(const iterator& rhs) const { return it->equals(rhs.it); }
            bool operator!=(const iterator& rhs) const { return !it->equals(rhs.it); }
        };
        iterator begin() const { return iterator(begin_m.get(), path); }
        iterator end() const { return iterator(end_m.get(), path); }
    };

    class Sequence {
        friend class SettingsNode;

      protected:
        const std::shared_ptr<Path> path;
        std::unique_ptr<Inner::sequence_iterator> begin_m;
        std::unique_ptr<Inner::sequence_iterator> end_m;
        Sequence(Inner::sequence_iterator* begin_p, Inner::sequence_iterator* end_p, const std::shared_ptr<Path>& path_p)
            : begin_m(begin_p), end_m(end_p), path(path_p){};

      public:
        class iterator {
            friend class Sequence;

          protected:
            const std::shared_ptr<Path>& path;
            Inner::sequence_iterator* const it;
            int index = 0;
            iterator(Inner::sequence_iterator* const it_p, const std::shared_ptr<Path>& path_p) : it(it_p), path(path_p){};

          public:
            void operator++() {
                ++index;
                it->next();
            }
            SettingsNode operator*() const { return SettingsNode(it->value(), std::make_shared<Path>(Path{"", index, path})); }
            bool operator==(const iterator& rhs) const { return it->equals(rhs.it); }
            bool operator!=(const iterator& rhs) const { return !it->equals(rhs.it); }
        };
        iterator begin() const { return iterator(begin_m.get(), path); }
        iterator end() const { return iterator(end_m.get(), path); }
    };

    std::string get_path() const {
        std::string result = "";
        const std::shared_ptr<Path>* current = &path;
        while (*current) {
            if ((*current)->index >= 0) {
                result = "[" + std::to_string((*current)->index) + "]" + result;
            } else {
                result = "/" + (*current)->name + result;
            }
            current = &(*current)->parent;
        }
        return result;
    }

    inline bool empty() const { return !inner || inner->empty(); }
    inline bool is_scalar() const { return inner && inner->is_scalar(); }
    inline bool is_sequence() const { return inner && inner->is_sequence(); }
    inline bool is_map() const { return inner && inner->is_map(); }
    inline bool has(const char* key) const { return inner && inner->has(key); }
    inline bool has(const std::string& key) const { return inner && inner->has(key); }

    SettingsNode::Map as_map() const {
        check();
        if (!inner->is_map()) {
            throw settings::exception("Settings '" + get_path() + "' is not a map");
        }
        const auto& it = inner->as_map();
        return Map(it.first, it.second, path);
    }
    SettingsNode::Sequence as_sequence() const {
        check();
        if (!inner->is_sequence()) {
            throw settings::exception("Settings '" + get_path() + "' is not a sequence");
        }
        const auto& it = inner->as_sequence();
        return Sequence(it.first, it.second, path);
    }

    inline SettingsNode operator[](const char* key) const {
        check();
        return SettingsNode(inner->get(key), std::make_shared<Path>(Path{key, -1, path}));
    }
    inline SettingsNode operator[](const std::string& key) const {
        check();
        return SettingsNode(inner->get(key), std::make_shared<Path>(Path{key, -1, path}));
    }

    template<typename T>
    inline T as() const {
        return T(as_inner<typename basetype<T>::type>());
    }

    template<typename T>
    inline T as(const typename basetype<T>::type& fallback) const {
        return T(as_inner<typename basetype<T>::type>(fallback));
    }

    SettingsNode(){};
    SettingsNode(std::unique_ptr<Inner> inner_p, const std::string& root = "")
        : path(std::make_shared<Path>(Path{root, -1, nullptr})), inner(std::move(inner_p)) {}

    SettingsNode& operator=(const SettingsNode& rhs) {
        if (rhs.empty()) {
            inner.reset();
        } else {
            inner = rhs.inner;
        }
        path = rhs.path;
        return *this;
    }

    SettingsNode(const SettingsNode& rhs) {
        if (rhs.empty()) {
            inner.reset();
        } else {
            inner = rhs.inner;
        }
        path = rhs.path;
    }

    void json(std::ostream& os, const std::string& indent = "", bool first = true) const {
        if (first) {
            os << indent;
        }
        if (is_sequence()) {
            os << "[\n";
            bool noindent = true;
            for (const auto& i : as_sequence()) {
                if (noindent) {
                    noindent = false;
                } else {
                    os << ",\n";
                }
                os << indent << "  ";
                i.json(os, indent + "  ", false);
            }
            os << "\n" << indent << "]";
        } else if (is_map()) {
            os << "{\n";
            bool noindent = true;
            for (const auto& i : as_map()) {
                if (noindent) {
                    noindent = false;
                } else {
                    os << ",\n";
                }
                os << indent << "  \"" << i.first << "\": ";
                i.second.json(os, indent + "  ", false);
            }
            os << "\n" << indent << "}";
        } else {
            os << "\"" << as<std::string>() << "\"";
        }
        if (first) {
            os << "\n";
        }
    }

    void yaml(std::ostream& os, const std::string& indent = "", bool first = true) const {
        if (first) {
            os << indent;
        }
        if (is_sequence()) {
            bool noindent = true;
            for (const auto& i : as_sequence()) {
                if (noindent) {
                    noindent = false;
                } else {
                    os << "\n" << indent;
                }
                os << "- ";
                i.yaml(os, indent + "  ", false);
            }
        } else if (is_map()) {
            bool noindent = true;
            for (const auto& i : as_map()) {
                if (noindent) {
                    noindent = false;
                } else {
                    os << "\n" << indent;
                }
                os << "\"" << i.first << "\": ";
                i.second.yaml(os, indent + "  ", false);
            }
        } else {
            os << "\"" << as<std::string>() << "\"";
        }
        if (first) {
            os << "\n";
        }
    }

    inline friend std::ostream& operator<<(std::ostream& os, const SettingsNode& node) { return node.inner->to_stream(os); }
};

template<>
inline bool SettingsNode::as_inner<bool>() const {
    check();
    check_scalar();
    return inner->as_bool();
}

template<>
inline int SettingsNode::as_inner<int>() const {
    check();
    check_scalar();
    return inner->as_int();
}

template<>
inline unsigned int SettingsNode::as_inner<unsigned int>() const {
    check();
    check_scalar();
    return inner->as_uint();
}

template<>
inline unsigned long SettingsNode::as_inner<unsigned long>() const {
    check();
    check_scalar();
    return inner->as_ulint();
}

template<>
inline double SettingsNode::as_inner<double>() const {
    check();
    check_scalar();
    return inner->as_double();
}

template<>
inline float SettingsNode::as_inner<float>() const {
    check();
    check_scalar();
    return inner->as_float();
}

template<>
inline std::string SettingsNode::as_inner<std::string>() const {
    check();
    check_scalar();
    return inner->as_string();
}

}  // namespace settings

#endif
