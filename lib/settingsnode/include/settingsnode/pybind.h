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

#ifndef SETTINGSNODE_PYBIND_H
#define SETTINGSNODE_PYBIND_H

#include <pybind11/pytypes.h>
#include "settingsnode/inner.h"

namespace settings {

namespace py = pybind11;

class PyNode : public Inner {
    friend class SettingsNode;

  protected:
    py::object node;
    inline bool as_bool() const override { return node.cast<bool>(); }
    inline int as_int() const override { return node.cast<int>(); }
    inline unsigned int as_uint() const override { return node.cast<unsigned int>(); }
    inline unsigned long as_ulint() const override { return node.cast<unsigned long>(); }
    inline double as_double() const override { return node.cast<double>(); }
    inline float as_float() const override { return node.cast<float>(); }
    inline std::string as_string() const override { return py::str(node).cast<std::string>(); }

    inline Inner* get(const char* key) const override { return new PyNode{node[key]}; }
    inline Inner* get(const std::string& key) const override { return new PyNode{node[key.c_str()]}; }
    inline bool empty() const override { return !node; }
    inline bool has(const char* key) const override { return node.contains(key); }
    inline bool has(const std::string& key) const override { return node.contains(key); }
    inline bool is_map() const override { return py::isinstance<py::dict>(node); }
    inline bool is_scalar() const override { return !is_map() && !is_sequence(); }
    inline bool is_sequence() const override { return py::isinstance<py::list>(node); }
    inline std::ostream& to_stream(std::ostream& os) const override { return os << py::repr(node).cast<std::string>(); }

    class map_iterator : public Inner::map_iterator {
        friend class PyNode;
        using inner_iterator = py::detail::generic_iterator<py::detail::iterator_policies::dict_readonly>;

      protected:
        inner_iterator it;
        explicit map_iterator(inner_iterator it_p) : it(std::move(it_p)){};

      public:
        void next() override { ++it; }
        std::string name() const override { return py::str((*it).first).cast<std::string>(); }
        Inner* value() const override { return new PyNode(py::reinterpret_steal<py::object>((*it).second)); }
        bool equals(const Inner::map_iterator* rhs) const override { return it == static_cast<const map_iterator*>(rhs)->it; }
    };

    std::pair<Inner::map_iterator*, Inner::map_iterator*> as_map() const override {
        return std::make_pair(new map_iterator(py::reinterpret_borrow<py::dict>(node).begin()), new map_iterator(py::reinterpret_borrow<py::dict>(node).end()));
    }

    class sequence_iterator : public Inner::sequence_iterator {
        friend class PyNode;
        using inner_iterator = py::detail::generic_iterator<py::detail::iterator_policies::sequence_fast_readonly>;

      protected:
        inner_iterator it;
        explicit sequence_iterator(inner_iterator it_p) : it(std::move(it_p)){};

      public:
        void next() override { ++it; }
        Inner* value() const override { return new PyNode(py::reinterpret_steal<py::object>(*it)); }
        bool equals(const Inner::sequence_iterator* rhs) const override { return it == static_cast<const sequence_iterator*>(rhs)->it; }
    };

    std::pair<Inner::sequence_iterator*, Inner::sequence_iterator*> as_sequence() const override {
        return std::make_pair(new sequence_iterator(py::reinterpret_borrow<py::list>(node).begin()),
                              new sequence_iterator(py::reinterpret_borrow<py::list>(node).end()));
    }

  public:
    explicit PyNode(py::object node_p) : node(std::move(node_p)){};
};

}  // namespace settings

#endif
