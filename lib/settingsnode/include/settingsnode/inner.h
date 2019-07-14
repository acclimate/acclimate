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

#ifndef SETTINGSNODE_INNER_H
#define SETTINGSNODE_INNER_H

#include <iostream>

namespace settings {

class SettingsNode;

class Inner {
    friend class SettingsNode;
    friend std::ostream& operator<<(std::ostream& os, const SettingsNode& node);

  protected:
    virtual bool as_bool() const = 0;
    virtual int as_int() const = 0;
    virtual unsigned int as_uint() const = 0;
    virtual unsigned long as_ulint() const = 0;
    virtual double as_double() const = 0;
    virtual float as_float() const { return static_cast<float>(as_double()); }
    virtual std::string as_string() const = 0;

    virtual Inner* get(const char* key) const = 0;
    virtual Inner* get(const std::string& key) const = 0;
    virtual bool empty() const { return false; }
    virtual bool has(const char* key) const = 0;
    virtual bool has(const std::string& key) const = 0;
    virtual bool is_map() const = 0;
    virtual bool is_scalar() const = 0;
    virtual bool is_sequence() const = 0;
    virtual std::ostream& to_stream(std::ostream& os) const = 0;

    class map_iterator {
      public:
        virtual void next() = 0;
        virtual std::string name() const = 0;
        virtual Inner* value() const = 0;
        virtual bool equals(const map_iterator* rhs) const = 0;
        virtual ~map_iterator() = default;
    };
    virtual std::pair<map_iterator*, map_iterator*> as_map() const = 0;

    class sequence_iterator {
      public:
        virtual void next() = 0;
        virtual Inner* value() const = 0;
        virtual bool equals(const sequence_iterator* rhs) const = 0;
        virtual ~sequence_iterator() = default;
    };
    virtual std::pair<sequence_iterator*, sequence_iterator*> as_sequence() const = 0;

  public:
    virtual ~Inner() = default;
};

}  // namespace settings

#endif
