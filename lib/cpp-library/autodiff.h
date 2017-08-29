/*
  Copyright (C) 2017 Sven Willner <sven.willner@gmail.com>

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

#ifndef AUTODIFF_H
#define AUTODIFF_H

#include <math.h>
#include <valarray>
#include <vector>

namespace autodiff {

template<typename T, typename Vector = std::valarray<T>>
class Variable;
template<typename T, typename Vector = std::valarray<T>>
class Value;
}

namespace std {

template<typename T, typename Vector>
autodiff::Value<T, Vector> pow(const autodiff::Value<T, Vector>& lhs, const autodiff::Value<T, Vector>& rhs);
template<typename T, typename Vector>
autodiff::Value<T, Vector> pow(const T& val, const autodiff::Value<T, Vector>& rhs);
template<typename T, typename Vector>
autodiff::Value<T, Vector> pow(const autodiff::Value<T, Vector>& lhs, const T& val);

template<typename T, typename Vector>
autodiff::Value<T, Vector> log(const autodiff::Value<T, Vector>& v);
template<typename T, typename Vector>
autodiff::Value<T, Vector> log2(const autodiff::Value<T, Vector>& v);
template<typename T, typename Vector>
autodiff::Value<T, Vector> log10(const autodiff::Value<T, Vector>& v);
template<typename T, typename Vector>
autodiff::Value<T, Vector> exp(const autodiff::Value<T, Vector>& v);

template<typename T, typename Vector>
autodiff::Value<T, Vector> min(const T& val, const autodiff::Value<T, Vector>& rhs);
template<typename T, typename Vector>
autodiff::Value<T, Vector> min(const autodiff::Value<T, Vector>& lhs, const T& val);

template<typename T, typename Vector>
autodiff::Value<T, Vector> max(const T& val, const autodiff::Value<T, Vector>& rhs);
template<typename T, typename Vector>
autodiff::Value<T, Vector> max(const autodiff::Value<T, Vector>& lhs, const T& val);
}

namespace autodiff {

template<typename T, typename Vector>
class Value {
    friend class Variable<T, Vector>;

  protected:
    T val;
    Vector dev;
    Value(const T& val_p, const Vector& dev_p) : val(val_p), dev(dev_p){};

  public:
    Value(size_t n, const T& val_p) : val(val_p), dev(0.0, n){};
    Value(size_t i, size_t n, const T& val_p) : val(val_p), dev(0.0, n) {
        dev[i] = 1;
    }
    inline const T value() const {
        return val;
    }
    explicit inline operator T() const {
        return val;
    }
    inline const Vector& derivative() const {
        return dev;
    }

    inline Value operator-() {
        return {-val, -dev};
    }

    inline friend Value operator+(const Value& lhs, const Value& rhs) {
        return {lhs.val + rhs.val, lhs.dev + rhs.dev};
    }
    inline friend Value operator+(const T& val, const Value& rhs) {
        return {val + rhs.val, rhs.dev};
    }
    inline friend Value operator+(const Value& lhs, const T& val) {
        return {lhs.val + val, lhs.dev};
    }

    inline friend Value operator-(const Value& lhs, const Value& rhs) {
        return {lhs.val - rhs.val, lhs.dev - rhs.dev};
    }
    inline friend Value operator-(const T& val, const Value& rhs) {
        return {val - rhs.val, -rhs.dev};
    }
    inline friend Value operator-(const Value& lhs, const T& val) {
        return {lhs.val - val, lhs.dev};
    }

    inline friend Value operator*(const Value& lhs, const Value& rhs) {
        return {lhs.val * rhs.val, lhs.dev * rhs.val + lhs.val * rhs.dev};
    }
    inline friend Value operator*(const T& val, const Value& rhs) {
        return {val * rhs.val, val * rhs.dev};
    }
    inline friend Value operator*(const Value& lhs, const T& val) {
        return {lhs.val * val, lhs.dev * val};
    }

    inline friend Value operator/(const Value& lhs, const Value& rhs) {
        return {lhs.val / rhs.val, lhs.dev / rhs.val - rhs.dev * (lhs.val / rhs.val / rhs.val)};
    }
    inline friend Value operator/(const T& val, const Value& rhs) {
        return {val / rhs.val, rhs.dev * (-val / rhs.val / rhs.val)};
    }
    inline friend Value operator/(const Value& lhs, const T& val) {
        return {lhs.val / val, lhs.dev / val};
    }

    inline Value& operator+=(const Value& v) {
        val += v.val;
        dev += v.dev;
        return *this;
    }
    inline Value& operator+=(const T& val) {
        val += val;
        return *this;
    }

    inline Value& operator-=(const Value& v) {
        val -= v.val;
        dev -= v.dev;
        return *this;
    }
    inline Value& operator-=(const T& val) {
        val -= val;
        return *this;
    }

    inline Value& operator*=(const Value& v) {
        val *= v.val;
        dev = dev * v.val + v.dev * val;
        return *this;
    }
    inline Value& operator*=(const T& val) {
        val *= val;
        dev *= val;
        return *this;
    }

    inline Value& operator/=(const Value& v) {
        val /= v.val;
        dev = dev / v.val - v.dev * val / v.val / v.val;
        return *this;
    }
    inline Value& operator/=(const T& val) {
        val /= val;
        dev /= val;
        return *this;
    }

    inline friend bool operator<(const Value& lhs, const Value& rhs) {
        return lhs.val < rhs.val;
    }
    inline friend bool operator<(const T& val, const Value& rhs) {
        return val < rhs.val;
    }
    inline friend bool operator<(const Value& lhs, const T& val) {
        return lhs.val < val;
    }

    inline friend bool operator<=(const Value& lhs, const Value& rhs) {
        return lhs.val <= rhs.val;
    }
    inline friend bool operator<=(const T& val, const Value& rhs) {
        return val <= rhs.val;
    }
    inline friend bool operator<=(const Value& lhs, const T& val) {
        return lhs.val <= val;
    }

    inline friend bool operator>(const Value& lhs, const Value& rhs) {
        return lhs.val > rhs.val;
    }
    inline friend bool operator>(const T& val, const Value& rhs) {
        return val > rhs.val;
    }
    inline friend bool operator>(const Value& lhs, const T& val) {
        return lhs.val > val;
    }

    inline friend bool operator>=(const Value& lhs, const Value& rhs) {
        return lhs.val >= rhs.val;
    }
    inline friend bool operator>=(const T& val, const Value& rhs) {
        return val >= rhs.val;
    }
    inline friend bool operator>=(const Value& lhs, const T& val) {
        return lhs.val >= val;
    }

    inline friend bool operator==(const Value& lhs, const Value& rhs) {
        return lhs.val == rhs.val;
    }
    inline friend bool operator==(const T& val, const Value& rhs) {
        return val == rhs.val;
    }
    inline friend bool operator==(const Value& lhs, const T& val) {
        return lhs.val == val;
    }

    inline friend bool operator!=(const Value& lhs, const Value& rhs) {
        return lhs.val != rhs.val;
    }
    inline friend bool operator!=(const T& val, const Value& rhs) {
        return val != rhs.val;
    }
    inline friend bool operator!=(const Value& lhs, const T& val) {
        return lhs.val != val;
    }

    friend Value std::pow<T, Vector>(const Value& lhs, const Value& rhs);
    friend Value std::pow<T, Vector>(const T& val, const Value& rhs);
    friend Value std::pow<T, Vector>(const Value& lhs, const T& val);

    friend Value std::log<T, Vector>(const Value& v);
    friend Value std::log2<T, Vector>(const Value& v);
    friend Value std::log10<T, Vector>(const Value& v);
    friend Value std::exp<T, Vector>(const Value& v);

    friend Value std::min<T, Vector>(const T& val, const Value& rhs);
    friend Value std::min<T, Vector>(const Value& lhs, const T& val);

    friend Value std::max<T, Vector>(const T& val, const Value& rhs);
    friend Value std::max<T, Vector>(const Value& lhs, const T& val);
};

template<typename T, typename Vector>
class Variable {
  protected:
    std::vector<T> val;
    const size_t variables_num;
    const size_t variables_offset;

  public:
    Variable(size_t offset, size_t num, size_t length, const T& initial_value) : variables_num(num), variables_offset(offset), val(length, initial_value){};
    inline const Variable& operator=(const std::vector<T>& val_p) {
        val.assign(val_p);
        return *this;
    }
    inline size_t size() const {
        return val.size();
    }
    inline std::vector<T>& value() {
        return val;
    }
    inline Value<T, Vector> operator[](size_t i) const {
        if (variables_offset < variables_num) {
            return {i + variables_offset, variables_num, val[i]};
        } else {
            return {variables_num, val[i]};
        }
    }
    inline Value<T, Vector> at(size_t i) const {
        if (variables_offset < variables_num) {
            return {i + variables_offset, variables_num, val.at(i)};
        } else {
            return {variables_num, val.at(i)};
        }
    }
};
}

namespace std {

template<typename T, typename Vector>
inline autodiff::Value<T, Vector> pow(const autodiff::Value<T, Vector>& lhs, const autodiff::Value<T, Vector>& rhs) {
    const T p = pow(lhs.val, rhs.val);
    return {p, rhs.dev * (log(lhs.val) * p) + lhs.dev * (p * rhs.val / lhs.val)};
}
template<typename T, typename Vector>
inline autodiff::Value<T, Vector> pow(const T& val, const autodiff::Value<T, Vector>& rhs) {
    const T p = pow(val, rhs.val);
    return {p, rhs.dev * (p * log(val))};
}
template<typename T, typename Vector>
inline autodiff::Value<T, Vector> pow(const autodiff::Value<T, Vector>& lhs, const T& val) {
    return {pow(lhs.val, val), lhs.dev * (val * pow(lhs.val, val - 1))};
}

template<typename T, typename Vector>
inline autodiff::Value<T, Vector> log(const autodiff::Value<T, Vector>& v) {
    return {log(v.val), v.dev / v.val};
}
template<typename T, typename Vector>
inline autodiff::Value<T, Vector> log2(const autodiff::Value<T, Vector>& v) {
    return {log2(v.val), v.dev / (v.val * M_LN2)};
}
template<typename T, typename Vector>
inline autodiff::Value<T, Vector> log10(const autodiff::Value<T, Vector>& v) {
    return {log10(v.val), v.dev / (v.val * M_LN10)};
}
template<typename T, typename Vector>
inline autodiff::Value<T, Vector> exp(const autodiff::Value<T, Vector>& v) {
    const T e = exp(v.val);
    return {e, v.dev * e};
}

template<typename T, typename Vector>
inline autodiff::Value<T, Vector> min(const T& val, const autodiff::Value<T, Vector>& rhs) {
    if (val < rhs.val) {
        return {rhs.dev.size(), val};
    } else {
        return rhs;
    }
}
template<typename T, typename Vector>
inline autodiff::Value<T, Vector> min(const autodiff::Value<T, Vector>& lhs, const T& val) {
    if (lhs.val < val) {
        return lhs;
    } else {
        return {lhs.dev.size(), val};
    }
}

template<typename T, typename Vector>
inline autodiff::Value<T, Vector> max(const T& val, const autodiff::Value<T, Vector>& rhs) {
    if (val < rhs.val) {
        return rhs;
    } else {
        return {rhs.dev.size(), val};
    }
}
template<typename T, typename Vector>
inline autodiff::Value<T, Vector> max(const autodiff::Value<T, Vector>& lhs, const T& val) {
    if (lhs.val < val) {
        return {lhs.dev.size(), val};
    } else {
        return lhs;
    }
}
}

#endif
