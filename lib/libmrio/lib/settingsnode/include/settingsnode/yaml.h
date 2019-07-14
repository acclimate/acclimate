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

#ifndef SETTINGSNODE_YAML_H
#define SETTINGSNODE_YAML_H

#include <yaml-cpp/node/impl.h>
#include <yaml-cpp/yaml.h>  // IWYU pragma: keep
#include "settingsnode/inner.h"

namespace settings {

namespace YAMLlib = YAML;

class YAML : public Inner {
    friend class SettingsNode;

  protected:
    YAMLlib::Node node;
    inline bool as_bool() const override { return node.as<bool>(); }
    inline int as_int() const override { return node.as<int>(); }
    inline unsigned int as_uint() const override { return node.as<unsigned int>(); }
    inline unsigned long as_ulint() const override { return node.as<unsigned long>(); }
    inline double as_double() const override { return node.as<double>(); }
    inline float as_float() const override { return node.as<float>(); }
    inline std::string as_string() const override { return node.as<std::string>(); }

    inline Inner* get(const char* key) const override { return new YAML{node[key]}; }
    inline Inner* get(const std::string& key) const override { return new YAML{node[key]}; }
    inline bool empty() const override { return !node; }
    inline bool has(const char* key) const override { return node[key] != nullptr; }
    inline bool has(const std::string& key) const override { return node[key] != nullptr; }
    inline bool is_map() const override { return node.IsMap(); }
    inline bool is_scalar() const override { return node.IsScalar(); }
    inline bool is_sequence() const override { return node.IsSequence(); }
    inline std::ostream& to_stream(std::ostream& os) const override { return os << node; }

    class map_iterator : public Inner::map_iterator {
        friend class YAML;

      protected:
        YAMLlib::Node::const_iterator it;
        explicit map_iterator(YAMLlib::Node::const_iterator it_p) : it(std::move(it_p)){};

      public:
        void next() override { ++it; }
        std::string name() const override { return (*it).first.as<std::string>(); }
        Inner* value() const override { return new YAML((*it).second); }
        bool equals(const Inner::map_iterator* rhs) const override { return it == dynamic_cast<const map_iterator*>(rhs)->it; }
    };

    std::pair<Inner::map_iterator*, Inner::map_iterator*> as_map() const override {
        return std::make_pair(new map_iterator(node.begin()), new map_iterator(node.end()));
    }

    class sequence_iterator : public Inner::sequence_iterator {
        friend class YAML;

      protected:
        YAMLlib::Node::const_iterator it;
        explicit sequence_iterator(YAMLlib::Node::const_iterator it_p) : it(std::move(it_p)){};

      public:
        void next() override { ++it; }
        Inner* value() const override { return new YAML(*it); }
        bool equals(const Inner::sequence_iterator* rhs) const override { return it == dynamic_cast<const sequence_iterator*>(rhs)->it; }
    };

    std::pair<Inner::sequence_iterator*, Inner::sequence_iterator*> as_sequence() const override {
        return std::make_pair(new sequence_iterator(node.begin()), new sequence_iterator(node.end()));
    }

  public:
    explicit YAML(const YAMLlib::Node& node_p) : node(node_p) {}  // YAMLlib::Node does not support move
    explicit YAML(std::istream& stream) : node(YAMLlib::Load(stream)) {}
};

}  // namespace settings

#endif
