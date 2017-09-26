/*
  Copyright (C) 2016-2017 Sven Willner <sven.willner@gmail.com>

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

#ifdef SETTINGSNODE_WITH_YAML
#include <yaml-cpp/node/impl.h>
#include <yaml-cpp/yaml.h>  // IWYU pragma: keep
#else
#error Only YAML supported yet. Must set SETTINGSNODE_WITH_YAML.
#endif
#include <iostream>
#include <stdexcept>

namespace settings {

class exception : public std::runtime_error {
  public:
    explicit exception(const std::string& s) : std::runtime_error(s){};
};

class hstring {
  protected:
    const std::string str_m;
    const uint32_t hash_m;

  public:
    using base_type = std::string;
    static constexpr uint32_t hash(const char* str, unsigned int prev = 5381) { return *str ? hash(str + 1, prev * 33 + *str) : prev; }
    explicit hstring(const std::string& str_p) : str_m(str_p), hash_m(hash(str_p.c_str())){};
    operator const uint32_t&() const { return hash_m; }
    operator const std::string&() const { return str_m; }
};

class SettingsNode;

class Inner {
    friend class SettingsNode;

  protected:
    virtual bool as_bool() const = 0;
    virtual int as_int() const = 0;
    virtual std::size_t as_size_t() const = 0;
    virtual double as_double() const = 0;
    virtual float as_float() const { return as_double(); }
    virtual std::string as_string() const = 0;

    virtual Inner* get(const char* key) const = 0;
    virtual Inner* get(const std::string& key) const = 0;
    virtual bool empty() const { return false; }
    virtual bool has(const char* key) const = 0;
    virtual bool has(const std::string& key) const = 0;
    virtual bool is_map() const = 0;
    virtual bool is_scalar() const = 0;
    virtual bool is_sequence() const = 0;

    class map_iterator {
      public:
        virtual void next() = 0;
        virtual std::string name() const = 0;
        virtual Inner* value() const = 0;
        virtual bool equals(const map_iterator* rhs) const = 0;
    };
    virtual std::pair<map_iterator*, map_iterator*> as_map() const = 0;

    class sequence_iterator {
      public:
        virtual void next() = 0;
        virtual Inner* value() const = 0;
        virtual bool equals(const sequence_iterator* rhs) const = 0;
    };
    virtual std::pair<sequence_iterator*, sequence_iterator*> as_sequence() const = 0;
};

#ifdef SETTINGSNODE_WITH_YAML
class InnerYAML : public Inner {
    friend class SettingsNode;

  protected:
    YAML::Node node;
    explicit InnerYAML(const YAML::Node node_p) : node(node_p){};
    inline bool as_bool() const override { return node.as<bool>(); }
    inline int as_int() const override { return node.as<int>(); }
    inline std::size_t as_size_t() const override { return node.as<std::size_t>(); }
    inline double as_double() const override { return node.as<double>(); }
    inline float as_float() const override { return node.as<float>(); }
    inline std::string as_string() const override { return node.as<std::string>(); }

    inline Inner* get(const char* key) const override { return new InnerYAML{node[key]}; }
    inline Inner* get(const std::string& key) const override { return new InnerYAML{node[key]}; }
    inline bool empty() const override { return !node; }
    inline bool has(const char* key) const override { return node[key]; }
    inline bool has(const std::string& key) const override { return node[key]; }
    inline bool is_map() const override { return node.IsMap(); }
    inline bool is_scalar() const override { return node.IsScalar(); }
    inline bool is_sequence() const override { return node.IsSequence(); }

    class map_iterator : public Inner::map_iterator {
        friend class InnerYAML;

      protected:
        YAML::Node::const_iterator it;
        explicit map_iterator(const YAML::Node::const_iterator it_p) : it(it_p){};

      public:
        void next() override { ++it; }
        std::string name() const override { return (*it).first.as<std::string>(); }
        Inner* value() const override { return new InnerYAML((*it).second); }
        bool equals(const Inner::map_iterator* rhs) const override { return it == static_cast<const map_iterator*>(rhs)->it; }
    };

    std::pair<Inner::map_iterator*, Inner::map_iterator*> as_map() const override {
        return std::make_pair(new map_iterator(node.begin()), new map_iterator(node.end()));
    }

    class sequence_iterator : public Inner::sequence_iterator {
        friend class InnerYAML;

      protected:
        YAML::Node::const_iterator it;
        explicit sequence_iterator(const YAML::Node::const_iterator it_p) : it(it_p){};

      public:
        void next() override { ++it; }
        Inner* value() const override { return new InnerYAML(*it); }
        bool equals(const Inner::sequence_iterator* rhs) const override { return it == static_cast<const sequence_iterator*>(rhs)->it; }
    };

    std::pair<Inner::sequence_iterator*, Inner::sequence_iterator*> as_sequence() const override {
        return std::make_pair(new sequence_iterator(node.begin()), new sequence_iterator(node.end()));
    }
};
#endif

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
            // TODO using iterator_category = std::forward_iterator_tag;
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
            // TODO using iterator_category = std::forward_iterator_tag;
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

    enum class Format {
#ifdef SETTINGSNODE_WITH_YAML
        YAML
#endif
    };

    SettingsNode(){};
#ifdef SETTINGSNODE_WITH_YAML
    SettingsNode(std::istream& stream, const std::string& root = "", const Format format = Format::YAML)
        : path(std::make_shared<Path>(Path{root, -1, nullptr})) {
        switch (format) {
            case Format::YAML:
                inner.reset(new InnerYAML(YAML::Load(stream)));
                break;
        }
    }
#endif

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
inline std::size_t SettingsNode::as_inner<std::size_t>() const {
    check();
    check_scalar();
    return inner->as_size_t();
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
