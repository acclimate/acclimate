/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

// IWYU pragma: private, include "acclimate.h"

// IWYU pragma: begin_exports
#include "exceptions.h"
#include "options.h"
// IWYU pragma: end_exports

// TODO replace by comments of respective parameter names
#define UNUSED(x) (void)(x)

namespace acclimate {

#ifdef DEBUG
#define typeassert(expr)                                                 \
    if (!(expr)) {                                                       \
        throw acclimate::exception("assertion failed: " __STRING(expr)); \
    }
#else
#define typeassert(expr) \
    {}
#endif

using FloatType = double;
using IntType = long;
using IndexType = int;

using TransportDelay = unsigned int;
using Distance = TransportDelay;
using TimeStep = unsigned int;
using Ratio = FloatType;
using Forcing = Ratio;

inline FloatType fround(FloatType x) {
    if constexpr (options::BANKERS_ROUNDING) {
        return std::rint(x);
    } else {
        return std::round(x);
    }
}

inline IntType iround(FloatType x) {
    if constexpr (options::BANKERS_ROUNDING) {
        return std::lrint(x);
    } else {
        return std::round(x);
    }
}

template<bool rounded>
struct InternalType {};

template<>
struct InternalType<true> {
    using type = IntType;
};
template<>
struct InternalType<false> {
    using type = FloatType;
};

template<int precision_digits_p, bool rounded_p>
class Type {
  protected:
    static constexpr FloatType precision_from_digits(const unsigned char precision_digits_p_) {
        return precision_digits_p_ == 0 ? 1 : 0.1 * precision_from_digits(precision_digits_p_ - 1);
    }
    static constexpr typename InternalType<rounded_p>::type maybe_round(FloatType f) {
        if constexpr (rounded) {
            return iround(f / precision);
        } else {
            return f;
        }
    }

  public:
    using base_type = FloatType;  // used for SettingsNode when reading from config
    static constexpr bool rounded = rounded_p;
    static constexpr int precision_digits = precision_digits_p;
    static constexpr FloatType precision = precision_from_digits(precision_digits_p);

    constexpr Type() : t(0) {}

  protected:
    typename InternalType<rounded_p>::type t;

  protected:
    constexpr FloatType get_float() const {
        if constexpr (rounded) {
            return t * precision;
        } else {
            return t;
        }
    }

  public:
    constexpr explicit Type(FloatType f) : t(maybe_round(f)) {}
    friend constexpr std::ostream& operator<<(std::ostream& lhs, const Type& rhs) {
        return lhs << std::setprecision(precision_digits_p) << std::fixed << to_float(rhs);
    }
    friend constexpr FloatType to_float(const Type& other) { return other.get_float(); }
};

#define INCLUDE_STANDARD_OPS(T)                                                                                                    \
    using Type<precision_digits, rounded>::Type;                                                                                   \
    constexpr T(const T& other) = default;                                                                                         \
    constexpr T(T&& other) = default;                                                                                              \
    constexpr T& operator=(const T& other) {                                                                                       \
        t = other.t;                                                                                                               \
        return *this;                                                                                                              \
    }                                                                                                                              \
    constexpr T operator+(const T& other) const {                                                                                  \
        T res;                                                                                                                     \
        res.t = t + other.t;                                                                                                       \
        return res;                                                                                                                \
    }                                                                                                                              \
    constexpr T operator-(const T& other) const {                                                                                  \
        T res;                                                                                                                     \
        res.t = t - other.t;                                                                                                       \
        return res;                                                                                                                \
    }                                                                                                                              \
    static constexpr const T quiet_NaN() {                                                                                         \
        T res;                                                                                                                     \
        res.t = std::numeric_limits<decltype(t)>::quiet_NaN();                                                                     \
        return res;                                                                                                                \
    }                                                                                                                              \
    constexpr T& operator+=(const T& rhs) {                                                                                        \
        t += rhs.t;                                                                                                                \
        return *this;                                                                                                              \
    }                                                                                                                              \
    constexpr T& operator-=(const T& rhs) {                                                                                        \
        t -= rhs.t;                                                                                                                \
        return *this;                                                                                                              \
    }                                                                                                                              \
    constexpr bool operator>(FloatType other) const { return get_float() > other; }                                                \
    constexpr bool operator>=(FloatType other) const { return get_float() >= other; }                                              \
    constexpr bool operator<(FloatType other) const { return get_float() < other; }                                                \
    constexpr bool operator<=(FloatType other) const { return get_float() <= other; }                                              \
    constexpr bool operator>(const T& other) const { return t > other.t; }                                                         \
    constexpr bool operator>=(const T& other) const { return t >= other.t; }                                                       \
    constexpr bool operator<(const T& other) const { return t < other.t; }                                                         \
    constexpr bool operator<=(const T& other) const { return t <= other.t; }                                                       \
    constexpr T operator*(FloatType other) const { return T(get_float() * other); }                                                \
    constexpr T operator/(FloatType other) const { return T(get_float() / other); }                                                \
    constexpr friend T operator*(FloatType lhs, const T& rhs) { return T(lhs * to_float(rhs)); }                                   \
    constexpr Ratio operator/(const T& other) const { return Ratio(static_cast<FloatType>(t) / static_cast<FloatType>(other.t)); } \
    friend constexpr bool same_sgn(const T& lhs, const T& rhs) { return (lhs.t >= 0) == (rhs.t >= 0); }                            \
    friend constexpr bool isnan(const T& other) { return std::isnan(other.t); }                                                    \
    friend inline T round(const T& other) {                                                                                        \
        if constexpr (options::BASED_ON_INT && rounded) {                                                                          \
            return other;                                                                                                          \
        } else {                                                                                                                   \
            return T(fround(other.t / precision) * precision);                                                                     \
        }                                                                                                                          \
    }                                                                                                                              \
    friend inline T abs(const T& other) {                                                                                          \
        T res;                                                                                                                     \
        res.t = std::abs(other.t);                                                                                                 \
        return res;                                                                                                                \
    }

template<int precision_digits_p>
using NonRoundedType = Type<precision_digits_p, false>;
template<int precision_digits_p>
using RoundedType = Type<precision_digits_p, options::BASED_ON_INT>;

class Time : public RoundedType<0> {
  public:
    constexpr bool operator==(const Time& other) const { return t <= other.t && t >= other.t; }
    INCLUDE_STANDARD_OPS(Time);
};

class Value;
class FlowQuantity;
class Price;

class FlowValue : public NonRoundedType<8> {
  public:
    constexpr Value operator*(const Time& other) const;
    constexpr FlowQuantity operator/(const Price& other) const;
    INCLUDE_STANDARD_OPS(FlowValue);
};

class Quantity;

class Value : public NonRoundedType<8> {
  public:
    constexpr Quantity operator/(const Price& other) const;
    constexpr FlowValue operator/(const Time& other) const;
    INCLUDE_STANDARD_OPS(Value);
};

class PriceGrad;
class FlowQuantity;

class Price : public RoundedType<6> {
  public:
    constexpr PriceGrad operator/(const FlowQuantity& other) const;
    INCLUDE_STANDARD_OPS(Price);
};

class FlowQuantity : public RoundedType<3> {
  public:
    constexpr FlowValue operator*(const Price& other) const { return FlowValue(get_float() * to_float(other)); }
    friend constexpr FlowValue operator*(const Price& lhs, const FlowQuantity& rhs) { return FlowValue(to_float(lhs) * to_float(rhs)); }
    friend constexpr Price operator/(const FlowValue& lhs, const FlowQuantity& rhs) { return Price(to_float(lhs) / to_float(rhs)); }
    constexpr Quantity operator*(const Time& other) const;
    INCLUDE_STANDARD_OPS(FlowQuantity);
};

class PriceGrad : public NonRoundedType<8> {
  public:
    constexpr Price operator*(const FlowQuantity& other) const { return Price(get_float() * to_float(other)); }
    INCLUDE_STANDARD_OPS(PriceGrad);
};

class Quantity : public RoundedType<3> {
  public:
    constexpr Value operator*(const Price& other) const { return Value(get_float() * to_float(other)); }
    friend constexpr Price operator/(const Value& lhs, const Quantity& rhs) { return Price(to_float(lhs) / to_float(rhs)); }
    constexpr FlowQuantity operator/(const Time& other) const { return FlowQuantity(get_float() / to_float(other)); }
    INCLUDE_STANDARD_OPS(Quantity);
};

#undef INCLUDE_STANDARD_OPS

constexpr PriceGrad Price::operator/(const FlowQuantity& other) const { return PriceGrad(get_float() / to_float(other)); }
constexpr FlowQuantity FlowValue::operator/(const Price& other) const { return FlowQuantity(get_float() / to_float(other)); }
constexpr FlowValue Value::operator/(const Time& other) const { return FlowValue(t / to_float(other)); }
constexpr Value FlowValue::operator*(const Time& other) const { return Value(t * to_float(other)); }
constexpr Quantity FlowQuantity::operator*(const Time& other) const { return Quantity(get_float() * to_float(other)); }
constexpr Quantity Value::operator/(const Price& other) const { return Quantity(get_float() / to_float(other)); }

template<typename Q, typename V>
class PricedQuantity {
  private:
    Q quantity;
    V value;
    constexpr PricedQuantity(Q quantity_p, V value_p, bool)  // bool parameter is only used to differentiate this constructor
                                                             // from the one without bool that also checks for non-negative values
        : quantity(std::move(quantity_p)), value(std::move(value_p)) {
        typeassert(!isnan(quantity));
        typeassert(!isnan(value));
    }

  public:
    constexpr PricedQuantity(Q quantity_p, V value_p) : quantity(std::move(quantity_p)), value(std::move(value_p)) {
        typeassert(!isnan(quantity));
        typeassert(!isnan(value));
        typeassert(quantity >= 0.0);
        typeassert(value >= 0.0);
    }
    constexpr PricedQuantity(Q quantity_p)  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
        : PricedQuantity(std::move(quantity_p), quantity_p * Price(1.0)) {}
    constexpr explicit PricedQuantity(FloatType quantity_p) : PricedQuantity(Q(quantity_p)) {}
    constexpr explicit PricedQuantity(Q quantity_p, const Price& price_p) : PricedQuantity(std::move(quantity_p), quantity_p * price_p) {}
    static constexpr PricedQuantity possibly_negative(Q quantity_p, V value_p) { return PricedQuantity(std::move(quantity_p), std::move(value_p), true); }
    static constexpr PricedQuantity possibly_negative(Q quantity_p, const Price& price_p) {
        return PricedQuantity::possibly_negative(std::move(quantity_p), quantity_p * price_p);
    }
    PricedQuantity(const PricedQuantity& other) : quantity(other.quantity), value(other.value) {
        if (quantity <= 0.0) {
            value = V(0.0);
        } else {
            value = other.value;
        }
        typeassert(quantity >= 0.0);
        typeassert(value >= 0.0);
    }
    constexpr const Q& get_quantity() const { return quantity; }
    constexpr const V& get_value() const { return value; }
    Price get_price() const {
        if (quantity <= 0.0) {
            return Price::quiet_NaN();
        }
        Price price(value / quantity);
        return round(price);
    }
    constexpr FloatType get_price_float() const {
        if (quantity <= 0.0) {
            return std::numeric_limits<FloatType>::quiet_NaN();
        }
        FloatType price(to_float(value) / to_float(quantity));
        typeassert(price >= 0.0);
        return price;
    }
    constexpr void set_price(const Price& price) {
        typeassert(price > 0.0);  // implies !isnan
        if (quantity <= 0.0) {
            value = V(0.0);
        } else {
            value = quantity * price;
        }
        typeassert(value >= 0.0);  // implies !isnan
    }
    constexpr void set_quantity_keep_value(const Q& quantity_p) {
        quantity = quantity_p;
        typeassert(quantity >= 0);  // implies !isnan
    }
    constexpr void set_value(const V& value_p) {
        value = value_p;
        typeassert(value >= 0);  // implies !isnan
    }
    constexpr PricedQuantity operator+(const PricedQuantity& other) const {
        return PricedQuantity::possibly_negative(quantity + other.quantity, value + other.value);
    }
    constexpr PricedQuantity operator-(const PricedQuantity& other) const {
        return PricedQuantity::possibly_negative(quantity - other.quantity, value - other.value);
    }
    constexpr PricedQuantity operator*(const Ratio& other) const { return PricedQuantity(quantity * other, value * other); }
    constexpr PricedQuantity operator/(const Ratio& other) const {
        typeassert(other > 0.0);
        return PricedQuantity(quantity / other, value / other);
    }
    constexpr Ratio operator/(const PricedQuantity& other) const {
        typeassert(other.quantity > 0.0);
        return Ratio(quantity / other.quantity);
    }
    constexpr bool operator<(const PricedQuantity& other) const { return quantity < other.quantity; }
    constexpr bool operator<=(const PricedQuantity& other) const { return quantity <= other.quantity; }
    constexpr bool operator>(const PricedQuantity& other) const { return quantity > other.quantity; }
    constexpr bool operator>=(const PricedQuantity& other) const { return quantity >= other.quantity; }
    constexpr PricedQuantity& operator=(const PricedQuantity& other) {
        quantity = other.quantity;
        if (quantity <= 0.0) {
            value = V(0.0);
        } else {
            value = other.value;
        }
        typeassert(quantity >= 0.0);
        typeassert(value >= 0.0);
        return *this;
    }
    constexpr PricedQuantity& operator+=(const PricedQuantity& other) {
        quantity += other.quantity;
        value += other.value;
        typeassert(quantity >= 0.0);
        typeassert(value >= 0.0);
        return *this;
    }
    PricedQuantity& operator-=(const PricedQuantity& other) {
        quantity -= other.quantity;
        value -= other.value;
        typeassert(quantity >= 0.0);
        if (round(quantity) <= 0.0) {
            quantity = Q(0.0);
            value = V(0.0);
        }
        typeassert(value >= 0.0);
        return *this;
    }
    constexpr PricedQuantity& add_possibly_negative(const PricedQuantity& other) {
        quantity += other.quantity;
        value += other.value;
        return *this;
    }
    constexpr PricedQuantity& subtract_possibly_negative(const PricedQuantity& other) {
        quantity -= other.quantity;
        value -= other.value;
        return *this;
    }
    friend constexpr std::ostream& operator<<(std::ostream& os, const PricedQuantity& op) { return os << op.quantity << " [@" << op.get_price() << "]"; }

    friend inline PricedQuantity round(const PricedQuantity& flow) {
        if constexpr (options::BASED_ON_INT) {
            return std::move(flow);
        } else {
            if (round(flow.get_quantity()) <= 0.0) {
                return PricedQuantity(0.0);
            } else {
                Price p = round(flow.get_price());
                Q rounded_quantity = round(flow.get_quantity());
                const auto rounded_flow = PricedQuantity(rounded_quantity, rounded_quantity * p);
                if ((rounded_flow.get_quantity() > 0.0 && rounded_flow.get_value() <= 0.0)
                    || (rounded_flow.get_quantity() <= 0.0 && rounded_flow.get_value() > 0.0)) {
                    return PricedQuantity(0.0);
                }
                return rounded_flow;
            }
        }
    }

    friend constexpr PricedQuantity absdiff(const PricedQuantity& a, const PricedQuantity& b) {
        if (a.quantity < b.quantity) {
            return b - a;
        }
        return a - b;
    }
};

using Flow = PricedQuantity<FlowQuantity, FlowValue>;
using Stock = PricedQuantity<Quantity, Value>;
using Demand = Flow;

constexpr Stock operator*(const Flow& flow, const Time& time) {
    typeassert(time >= 0.0);
    return Stock::possibly_negative(flow.get_quantity() * time, flow.get_value() * time);
}

constexpr Flow operator/(const Stock& stock, const Time& time) {
    typeassert(time >= 0.0);
    return Flow::possibly_negative(stock.get_quantity() / time, stock.get_value() / time);
}

template<typename Current, typename Initial>
class AnnotatedType {
  public:
    Current current;
    Initial initial;

    constexpr AnnotatedType(Current current_p, Initial initial_p) : current(current_p), initial(initial_p) {}

    constexpr explicit AnnotatedType(Initial initial_p) : current(initial_p), initial(initial_p) {}
};

using AnnotatedFlow = AnnotatedType<Flow, FlowQuantity>;
using AnnotatedStock = AnnotatedType<Stock, Quantity>;

#undef typeassert

}  // namespace acclimate

#endif
