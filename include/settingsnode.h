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
    exception(const std::string& s) : std::runtime_error(s){};
};

class hstring {
  protected:
    const std::string str_;
    const uint32_t hash_;

  public:
    using base_type = std::string;
    static constexpr uint32_t hash(const char* str, unsigned int prev = 5381) { return *str ? hash(str + 1, prev * 33 + *str) : prev; };
    hstring(const std::string str_p) : str_(str_p), hash_(hash(str_p.c_str())){};
    operator const uint32_t&() const { return hash_; };
    operator const std::string&() const { return str_; };
};

class SettingsNode {
  protected:
    struct Path {
        std::string name;
        int index;
        std::shared_ptr<Path> parent;
    };
#ifdef SETTINGSNODE_WITH_YAML
    YAML::Node node;
#endif
    std::shared_ptr<Path> path;

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

#ifdef SETTINGSNODE_WITH_YAML
    SettingsNode(const YAML::Node node_p, const std::shared_ptr<Path>& path_p) : node(node_p), path(path_p){};
#endif

    inline void check() const {
        if (empty()) {
            throw settings::exception("Settings '" + get_path() + "' not found");
        }
    }

  public:
    class Map {
        friend class SettingsNode;

      protected:
#ifdef SETTINGSNODE_WITH_YAML
        const YAML::Node node;
#endif
        const std::shared_ptr<Path> path;
#ifdef SETTINGSNODE_WITH_YAML
        Map(const YAML::Node node_p, const std::shared_ptr<Path> path_p) : node(node_p), path(path_p){};
#endif

      public:
        class iterator {
            friend class Map;

          protected:
            YAML::Node::const_iterator it;
            const std::shared_ptr<Path>& path;
            iterator(const YAML::Node::const_iterator it_p, const std::shared_ptr<Path>& path_p) : it(it_p), path(path_p){};

          public:
            using iterator_category = std::forward_iterator_tag;
            iterator operator++() {
                ++it;
                return *this;
            };
            std::pair<std::string, SettingsNode> operator*() const {
                const std::string& name = (*it).first.as<std::string>();
                return std::make_pair(name, SettingsNode((*it).second, std::make_shared<Path>(Path{name, -1, path})));
            };
            bool operator==(const iterator& rhs) { return it == rhs.it; };
            bool operator!=(const iterator& rhs) { return it != rhs.it; };
        };
        iterator begin() const { return iterator(node.begin(), path); };
        iterator end() const { return iterator(node.end(), path); };
    };

    class Sequence {
        friend class SettingsNode;

      protected:
#ifdef SETTINGSNODE_WITH_YAML
        const YAML::Node node;
#endif
        const std::shared_ptr<Path> path;
#ifdef SETTINGSNODE_WITH_YAML
        Sequence(const YAML::Node node_p, const std::shared_ptr<Path> path_p) : node(node_p), path(path_p){};
#endif

      public:
        class iterator {
            friend class Sequence;

          protected:
#ifdef SETTINGSNODE_WITH_YAML
            YAML::Node::const_iterator it;
#endif
            const std::shared_ptr<Path>& path;
            int index;
#ifdef SETTINGSNODE_WITH_YAML
            iterator(const YAML::Node::const_iterator it_p, const std::shared_ptr<Path>& path_p) : it(it_p), path(path_p), index(0){};
#endif

          public:
            using iterator_category = std::forward_iterator_tag;
            iterator operator++() {
                ++it;
                ++index;
                return *this;
            };
            SettingsNode operator*() const { return SettingsNode(*it, std::make_shared<Path>(Path{"", index, path})); };
            bool operator==(const iterator& rhs) { return it == rhs.it; };
            bool operator!=(const iterator& rhs) { return it != rhs.it; };
        };
        iterator begin() const { return iterator(node.begin(), path); };
        iterator end() const { return iterator(node.end(), path); };
    };

    enum class Format {
#ifdef SETTINGSNODE_WITH_YAML
        YAML
#endif
    };

    SettingsNode() : node() {}
#ifdef SETTINGSNODE_WITH_YAML
    SettingsNode(std::istream& stream, const Format format = Format::YAML) : node(YAML::Load(stream)) {
        (void)format;  // currently only YAML supported
    }
#endif
    void operator=(const SettingsNode& rhs) {
        if (rhs.node) {
            node = rhs.node;
        } else {
            node.reset();
        }
        path = rhs.path;
    }

    inline bool empty() const { return !node; }
    bool has(const std::string& key) const { return node[key]; }
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

    Map as_map() const {
        check();
        if (!node.IsMap()) {
            throw settings::exception("Settings '" + get_path() + "' is not a map");
        }
        return Map(node, path);
    }
    Sequence as_sequence() const {
        check();
        if (!node.IsSequence()) {
            throw settings::exception("Settings '" + get_path() + "' is not a sequence");
        }
        return Sequence(node, path);
    }

    SettingsNode operator[](const char* key) const {
        check();
        return SettingsNode(node[key], std::make_shared<Path>(Path{key, -1, path}));
    }
    SettingsNode operator[](const std::string& key) const {
        check();
        return SettingsNode(node[key], std::make_shared<Path>(Path{key, -1, path}));
    }

    template<typename T>
    T as() const {
        check();
        if (!node.IsScalar()) {
            throw settings::exception("Settings '" + get_path() + "' is not a scalar value");
        }
        return T(node.as<typename basetype<T>::type>());
    }

    template<typename T>
    T as(const typename basetype<T>::type& fallback) const {
        return T(node.as<typename basetype<T>::type>(fallback));
    }

    friend std::ostream& operator<<(std::ostream& os, const SettingsNode& node) {
        os << node.node;
        return os;
    }
};
}  // namespace settings

#endif
