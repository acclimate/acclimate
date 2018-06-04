/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>
                          Christian Otto <christian.otto@pik-potsdam.de>

  This file is part of Acclimate.

  Acclimate is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Acclimate is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with Acclimate.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ACCLIMATE_TYPES_H
#define ACCLIMATE_TYPES_H

#include <cmath>
#include <iostream>
#include <limits>
#include "helpers.h"

namespace acclimate {

using FloatType = double;
using IntType = long;

using TransportDelay = unsigned char;
using Distance = TransportDelay;
using TimeStep = unsigned int;
using Ratio = FloatType;
using Forcing = Ratio;

inline FloatType fround(const FloatType x) {
#ifdef BANKERS_ROUNDING
    return std::rint(x);
#else
    return std::round(x);
#endif
}

inline IntType iround(const FloatType x) {
#ifdef BANKERS_ROUNDING
    return std::lrint(x);
#else
    return std::round(x);
#endif
}

template<class T>
inline FloatType to_float(const T& a) {
    return a.get_float();
}

template<int precision_digits_p>
class Type {
  protected:
    static constexpr FloatType precision_from_digits(const unsigned char precision_digits_p_) {
        return precision_digits_p_ == 0 ? 1 : 0.1 * precision_from_digits(precision_digits_p_ - 1);
    }
    virtual void set_float(const FloatType f) = 0;

  public:
    using base_type = FloatType;
    static constexpr int precision_digits = precision_digits_p;
    static constexpr FloatType precision = precision_from_digits(precision_digits_p);
    virtual FloatType get_float() const = 0;
    friend std::ostream& operator<<(std::ostream& lhs, const Type& rhs) {
        return lhs << std::setprecision(precision_digits_p) << std::fixed << rhs.get_float();
    }
    virtual ~Type(){};
};

#define INCLUDE_STANDARD_OPS(T)                                                                                          \
  protected:                                                                                                             \
    T() {}                                                                                                               \
                                                                                                                         \
  public:                                                                                                                \
    explicit T(const FloatType t_) { set_float(t_); }                                                                    \
    T operator+(const T& other) const {                                                                                  \
        T res;                                                                                                           \
        res.t = t + other.t;                                                                                             \
        return res;                                                                                                      \
    }                                                                                                                    \
    T operator-(const T& other) const {                                                                                  \
        T res;                                                                                                           \
        res.t = t - other.t;                                                                                             \
        return res;                                                                                                      \
    }                                                                                                                    \
    static inline const T quiet_NaN() {                                                                                  \
        T res;                                                                                                           \
        res.t = std::numeric_limits<decltype(t)>::quiet_NaN();                                                           \
        return res;                                                                                                      \
    }                                                                                                                    \
    T& operator=(const T& other) {                                                                                       \
        t = other.t;                                                                                                     \
        return *this;                                                                                                    \
    }                                                                                                                    \
    T& operator+=(const T& rhs) {                                                                                        \
        t += rhs.t;                                                                                                      \
        return *this;                                                                                                    \
    }                                                                                                                    \
    T& operator-=(const T& rhs) {                                                                                        \
        t -= rhs.t;                                                                                                      \
        return *this;                                                                                                    \
    }                                                                                                                    \
    bool operator>(const FloatType other) const { return get_float() > other; }                                          \
    bool operator>=(const FloatType other) const { return get_float() >= other; }                                        \
    bool operator<(const FloatType other) const { return get_float() < other; }                                          \
    bool operator<=(const FloatType other) const { return get_float() <= other; }                                        \
    bool operator>(const T& other) const { return t > other.t; }                                                         \
    bool operator>=(const T& other) const { return t >= other.t; }                                                       \
    bool operator<(const T& other) const { return t < other.t; }                                                         \
    bool operator<=(const T& other) const { return t <= other.t; }                                                       \
    T operator*(const FloatType other) const { return T(get_float() * other); }                                          \
    T operator/(const FloatType other) const { return T(get_float() / other); }                                          \
    friend T operator*(const FloatType lhs, const T& rhs) { return T(lhs * rhs.get_float()); }                           \
    Ratio operator/(const T& other) const { return Ratio(static_cast<FloatType>(t) / static_cast<FloatType>(other.t)); } \
    friend bool same_sgn(const T& lhs, const T& rhs) { return (lhs.t >= 0) == (rhs.t >= 0); }                            \
    friend bool isnan(const T& other) { return std::isnan(other.t); }                                                    \
    friend T abs(const T& other) {                                                                                       \
        T res;                                                                                                           \
        res.t = std::abs(other.t);                                                                                       \
        return res;                                                                                                      \
    }

template<int precision_digits_p>
class NonRoundedType : public Type<precision_digits_p> {
  protected:
    FloatType t;
    inline void set_float(const FloatType f) override { t = f; }

  public:
    inline FloatType get_float() const override { return t; }
};

#ifdef BASED_ON_INT

template<int precision_digits_p>
class RoundedType : public Type<precision_digits_p> {
  protected:
    IntType t;
    inline void set_float(const FloatType f) override { t = iround(f / precision); }

  public:
    using Type<precision_digits_p>::precision;
    inline FloatType get_float() const override { return t * precision; }
};

#define INCLUDE_ROUNDED_OPS(T)                              \
    friend inline T round(const T& other) { return other; } \
    INCLUDE_STANDARD_OPS(T)

#else  // !def BASED_ON_INT

template<int precision_digits_p>
using RoundedType = NonRoundedType<precision_digits_p>;

#define INCLUDE_ROUNDED_OPS(T)                                                                \
    friend inline T round(const T& other) { return fround(other.t / precision) * precision; } \
    INCLUDE_STANDARD_OPS(T)

#endif

#define INCLUDE_NONROUNDED_OPS(T) INCLUDE_STANDARD_OPS(T)

class Time : public RoundedType<0> {
  public:
    bool operator==(const Time& other) const { return t <= other.t && t >= other.t; }
    INCLUDE_ROUNDED_OPS(Time);
};

class Value;
class FlowQuantity;
class Price;
class FlowValue : public NonRoundedType<8> {
  public:
    Value operator*(const Time& other) const;
    FlowQuantity operator/(const Price& other) const;
    INCLUDE_NONROUNDED_OPS(FlowValue);
};

class Quantity;
class Value : public NonRoundedType<8> {
  public:
    Quantity operator/(const Price& other) const;
    FlowValue operator/(const Time& other) const;
    INCLUDE_NONROUNDED_OPS(Value);
};

class PriceGrad;
class FlowQuantity;
class Price : public RoundedType<6> {
  public:
    PriceGrad operator/(const FlowQuantity& other) const;
    INCLUDE_ROUNDED_OPS(Price);
};

class FlowQuantity : public RoundedType<3> {
  public:
    FlowValue operator*(const Price& other) const { return FlowValue(get_float() * other.get_float()); }
    friend FlowValue operator*(const Price& lhs, const FlowQuantity& rhs) { return FlowValue(lhs.get_float() * rhs.get_float()); }
    friend Price operator/(const FlowValue& lhs, const FlowQuantity& rhs) { return Price(lhs.get_float() / rhs.get_float()); }
    Quantity operator*(const Time& other) const;
    INCLUDE_ROUNDED_OPS(FlowQuantity);
};

class PriceGrad : public NonRoundedType<8> {
  public:
    Price operator*(const FlowQuantity& other) const { return Price(get_float() * other.get_float()); }
    INCLUDE_ROUNDED_OPS(PriceGrad);
};

class Quantity : public RoundedType<3> {
  public:
    Value operator*(const Price& other) const { return Value(get_float() * other.get_float()); }
    friend Price operator/(const Value& lhs, const Quantity& rhs) { return Price(lhs.get_float() / rhs.get_float()); }
    FlowQuantity operator/(const Time& other) const { return FlowQuantity(get_float() / other.get_float()); }
    INCLUDE_ROUNDED_OPS(Quantity);
};

inline PriceGrad Price::operator/(const FlowQuantity& other) const { return PriceGrad(get_float() / other.get_float()); }

inline FlowQuantity FlowValue::operator/(const Price& other) const { return FlowQuantity(get_float() / other.get_float()); }

inline FlowValue Value::operator/(const Time& other) const { return FlowValue(t / other.get_float()); }

inline Value FlowValue::operator*(const Time& other) const { return Value(t * other.get_float()); }

inline Quantity FlowQuantity::operator*(const Time& other) const { return Quantity(get_float() * other.get_float()); }

inline Quantity Value::operator/(const Price& other) const { return Quantity(get_float() / other.get_float()); }

template<typename Q, typename V>
class PricedQuantity {
  protected:
    Q quantity;
    V value;
    PricedQuantity() : quantity(0.0), value(0.0) {}

  public:
    PricedQuantity(const Q& quantity_p, const V& value_p, const bool maybe_negative = false) : quantity(quantity_p), value(value_p) {
        assert_(!isnan(quantity));
        assert_(!isnan(value));
        if (!maybe_negative) {
            assert_(quantity >= 0.0);
            assert_(value >= 0.0);
        }
    }
    PricedQuantity(const Q& quantity_p, const bool maybe_negative = false) : PricedQuantity(quantity_p, quantity_p * Price(1.0), maybe_negative){};
    explicit PricedQuantity(const FloatType quantity_p) : PricedQuantity(Q(quantity_p), Q(quantity_p) * Price(1.0)){};
    PricedQuantity(const Q& quantity_p, const Price& price_p, const bool maybe_negative = false)
        : PricedQuantity(quantity_p, quantity_p * price_p, maybe_negative){};
    PricedQuantity(const PricedQuantity& other) : quantity(other.quantity), value(other.value) {
        if (quantity <= 0.0) {
            value = V(0.0);
        } else {
            value = other.value;
        }
        assert_(quantity >= 0.0);
        assert_(value >= 0.0);
    }

    const Q& get_quantity() const { return quantity; }
    const V& get_value() const { return value; }
    const Price get_price() const {
        if (quantity <= 0.0) {
            return Price::quiet_NaN();
        } else {
            Price price(value / quantity);
            return round(price);
        }
    }
    FloatType get_price_float() const {
        if (quantity <= 0.0) {
            return std::numeric_limits<FloatType>::quiet_NaN();
        } else {
            FloatType price(to_float(value) / to_float(quantity));
            assert_(price >= 0.0);
            return price;
        }
    }

    void set_price(const Price& price) {
        assert_(price > 0.0);
        if (quantity <= 0.0) {
            value = V(0.0);
        } else {
            value = quantity * price;
        }
        assert_(value >= 0.0);
    }
    void set_quantity_keep_value(const Q& quantity_p) {
        quantity = quantity_p;
        assert_(quantity >= 0);
    }
    void set_value(const V& value_p) {
        value = value_p;
        assert_(value >= 0);
    }

    PricedQuantity operator+(const PricedQuantity& other) const { return PricedQuantity(quantity + other.quantity, value + other.value, true); }
    PricedQuantity operator-(const PricedQuantity& other) const { return PricedQuantity(quantity - other.quantity, value - other.value, true); }
    PricedQuantity operator*(const Ratio& other) const { return PricedQuantity(quantity * other, value * other); }
    PricedQuantity operator/(const Ratio& other) const {
        assert_(other > 0.0);
        return PricedQuantity(quantity / other, value / other);
    }
    Ratio operator/(const PricedQuantity& other) const {
        assert_(other.quantity > 0.0);
        return Ratio(quantity / other.quantity);
    }

    bool operator<(const PricedQuantity& other) const { return quantity < other.quantity; }
    bool operator<=(const PricedQuantity& other) const { return quantity <= other.quantity; }
    bool operator>(const PricedQuantity& other) const { return quantity > other.quantity; }
    bool operator>=(const PricedQuantity& other) const { return quantity >= other.quantity; }

    PricedQuantity& operator=(const PricedQuantity& other) {
        quantity = other.quantity;
        if (quantity <= 0.0) {
            value = V(0.0);
        } else {
            value = other.value;
        }
        assert_(quantity >= 0.0);
        assert_(value >= 0.0);
        return *this;
    }
    const PricedQuantity& operator+=(const PricedQuantity& other) {
        quantity += other.quantity;
        value += other.value;
        assert_(quantity >= 0.0);
        assert_(value >= 0.0);
        return *this;
    }
    const PricedQuantity& operator-=(const PricedQuantity& other) {
        quantity -= other.quantity;
        value -= other.value;
        assert_(quantity >= 0.0);
        if (round(quantity) <= 0.0) {
            quantity = Q(0.0);
            value = V(0.0);
        }
        assert_(value >= 0.0);
        return *this;
    }
    const PricedQuantity& add_possibly_negative(const PricedQuantity& other) {
        quantity += other.quantity;
        value += other.value;
        return *this;
    }
    const PricedQuantity& subtract_possibly_negative(const PricedQuantity& other) {
        quantity -= other.quantity;
        value -= other.value;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const PricedQuantity& op) { return os << op.quantity << " [@" << op.get_price() << "]"; }

#ifdef BASED_ON_INT
    friend const PricedQuantity& round(const PricedQuantity& flow, bool maybe_negative = false) {
        UNUSED(maybe_negative);
        return flow;
    }
#else
    friend const PricedQuantity round(const PricedQuantity& flow, bool maybe_negative = false) {
        if (round(flow.get_quantity()) <= 0.0) {
            return PricedQuantity(0.0, 0.0);
        } else {
            Price p = round(flow.get_price());
            Q rounded_quantity = round(flow.get_quantity());
            PricedQuantity rounded_flow = PricedQuantity(rounded_quantity, rounded_quantity * p, maybe_negative);
            if ((rounded_flow.get_quantity() > 0.0 && rounded_flow.get_value() <= 0.0)
                || (rounded_flow.get_quantity() <= 0.0 && rounded_flow.get_value() > 0.0)) {
                rounded_flow = PricedQuantity(0.0, 0.0);
            }
            return rounded_flow;
        }
    }
#endif

    friend const PricedQuantity absdiff(const PricedQuantity& a, const PricedQuantity& b) {
        if (a.quantity < b.quantity) {
            return b - a;
        } else {
            return a - b;
        }
    }
};

using Flow = PricedQuantity<FlowQuantity, FlowValue>;
using Stock = PricedQuantity<Quantity, Value>;
using Demand = Flow;

inline Stock operator*(const Flow& flow, const Time& time) {
    assert_(time >= 0.0);
    return Stock(flow.get_quantity() * time, flow.get_value() * time, true);
}

inline Flow operator/(const Stock& stock, const Time& time) {
    assert_(time >= 0.0);
    return Flow(stock.get_quantity() / time, stock.get_value() / time, true);
}
}  // namespace acclimate

#endif
