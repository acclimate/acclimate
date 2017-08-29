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

#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <exception>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <typeinfo>

namespace csv {

class parser_exception : public std::runtime_error {
  public:
    const unsigned long row;
    const unsigned long col;

  protected:
    parser_exception(const char* msg_p, const unsigned long row_p, const unsigned long col_p) : std::runtime_error(msg_p), row(row_p), col(col_p){};
};

class bad_int_cast : public parser_exception, public std::bad_cast {
  public:
    bad_int_cast(const unsigned long row_p, const unsigned long col_p) : parser_exception("could not read unsigned integer", row_p, col_p){};
};

class bad_float_cast : public parser_exception, public std::bad_cast {
  public:
    bad_float_cast(const unsigned long row_p, const unsigned long col_p) : parser_exception("could not read floating point number", row_p, col_p){};
};

class col_end : public parser_exception {
  public:
    col_end(const unsigned long row_p, const unsigned long col_p) : parser_exception("column already ended", row_p, col_p){};
};

class row_end : public parser_exception {
  public:
    row_end(const unsigned long row_p, const unsigned long col_p) : parser_exception("row already ended", row_p, col_p){};
};

class file_end : public parser_exception {
  public:
    file_end(const unsigned long row_p, const unsigned long col_p) : parser_exception("file already ended", row_p, col_p){};
};

class unclosed_quotes : public parser_exception {
  public:
    unclosed_quotes(const unsigned long row_p, const unsigned long col_p) : parser_exception("unclosed quotes", row_p, col_p){};
};

enum class ColumnType { EMPTY, STRING, INTEGER, FLOAT };

class Parser {
  protected:
    std::istream& in;
    char delimiter;
    unsigned long row_ = 0;
    bool col_has_been_read = false;
    bool row_finished = false;
    unsigned long col_ = 0;

  public:
    class iterator;

    class Row {
        friend class Parser::iterator;

      protected:
        Parser& p;

      protected:
        explicit Row(Parser& parser) : p(parser){};

      public:
        class iterator;

        class Col {
            friend class Row::iterator;

          protected:
            Parser& p;

          protected:
            explicit Col(Parser& parser) : p(parser){};

          public:
            template<typename T>
            inline T read() {
                return p.read<T>();
            }
        };

        class iterator {
            friend class Row;

          protected:
            Parser& p;
            long col;
            iterator(Parser& parser, const long col_p) : p(parser), col(col_p){};

          public:
            iterator operator++() {
                if (p.next_col()) {
                    ++col;
                } else {
                    col = -1;
                }
                return *this;
            };
            Col operator*() const { return Col(p); };
            bool operator==(const iterator& rhs) const { return col == rhs.col; };
            bool operator!=(const iterator& rhs) const { return col != rhs.col; };
        };

      public:
        iterator begin() { return iterator(p, 0); };
        iterator end() { return iterator(p, -1); };
    };

    class iterator {
        friend class Parser;

      protected:
        Parser& p;
        long row;
        iterator(Parser& parser, const long row_p) : p(parser), row(row_p){};

      public:
        iterator operator++() {
            if (p.next_row()) {
                ++row;
            } else {
                row = -1;
            }
            return *this;
        };
        Row operator*() const { return Row(p); };
        bool operator==(const iterator& rhs) const { return row == rhs.row; };
        bool operator!=(const iterator& rhs) const { return row != rhs.row; };
    };

  protected:
    inline bool read_skip_whitespace(char& c, bool& quoted);
    inline void begin_read() {
        if (col_has_been_read) {
            throw col_end(row_, col_);
        }
        if (row_finished) {
            throw row_end(row_, col_);
        }
        col_has_been_read = true;
    }

  public:
    Parser(std::istream& stream, char delimiter_ = ',') : in(stream), delimiter(delimiter_){};

    iterator begin() { return iterator(*this, 0); };
    iterator end() { return iterator(*this, -1); };

    inline long row() const { return row_; };
    inline long col() const { return col_; };

    inline bool eof() const { return in.eof(); };
    inline bool eol() const { return row_finished; };

    template<typename T>
    inline typename std::enable_if<std::is_same<T, void>::value, T>::type read();

    template<typename T>
    inline typename std::enable_if<std::is_same<T, std::string>::value, T>::type read();

    template<typename T>
    inline typename std::enable_if<std::is_same<T, ColumnType>::value, T>::type read();

    template<typename T>
    inline typename std::enable_if<std::is_integral<T>::value, T>::type read();

    template<typename T>
    inline typename std::enable_if<std::is_floating_point<T>::value, T>::type read();

    template<typename T>
    inline T read_and_next() {
        T&& tmp = read<T>();
        next_col();
        return tmp;
    }

    template<typename T1, typename T2, typename... Ts>
    inline std::tuple<T1, T2, Ts...> read() {
        std::tuple<T1, T2, Ts...>&& res =
            std::tuple<T1, T2, Ts...>{read_and_next<T1>(), read_and_next<T2>(), read_and_next<Ts>()...};  // initializer lists are evaluated in order
        // undo last next_col():
        --col_;
        col_has_been_read = true;
        return res;
    }

    inline bool next_col();

    bool next_row() {
        ++row_;
        char c;
        if (!row_finished) {
            bool quoted = false;
            while (true) {
                if (in.eof()) {
                    if (quoted) {
                        throw unclosed_quotes(row_, col_);
                    } else {
                        throw file_end(row_, col_);
                    }
                }
                c = in.get();
                if ((c == '\n' || c == '\r') && !quoted) {
                    break;
                } else if (c == '"') {
                    if (quoted && in.peek() == '"') {
                        in.get();
                    } else {
                        quoted = !quoted;
                    }
                }
            };
        }
        while (true) {
            c = in.peek();
            if (in.eof()) {
                return false;
            }
            if (c == '\n' || c == '\r') {
                in.get();
            } else if (c == '#') {
                while (true) {
                    c = in.get();
                    if (in.eof()) {
                        return false;
                    }
                    if (c == '\n' || c == '\r') {
                        break;
                    }
                }
            } else {
                break;
            }
        }
        col_has_been_read = false;
        row_finished = false;
        col_ = 0;
        return true;
    };
};

template<>
inline void Parser::read<void>() {
    begin_read();
    bool quoted = false;
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        const char c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                in.get();
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        }
    }
}

template<>
inline std::string Parser::read<std::string>() {
    begin_read();
    std::string res = "";
    bool quoted = false;
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        const char c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                res += '"';
                in.get();
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        } else {
            res += c;
        }
    }
    return res;
}

inline bool Parser::read_skip_whitespace(char& c, bool& quoted) {
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            return false;
        }
        c = in.get();
        if (c == delimiter && !quoted) {
            return false;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                return true;
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            return false;
        } else if (c == ' ' || c == '\t') {
            // skip
        } else {
            return true;
        }
    }
}

template<typename T>
inline typename std::enable_if<std::is_integral<T>::value, T>::type Parser::read() {
    begin_read();
    T res = 0;
    bool quoted = false;
    bool negative = false;
    char c;

    if (!read_skip_whitespace(c, quoted)) {
        throw bad_int_cast(row_, col_);
    }

    if (c == '-') {
        if (!std::numeric_limits<T>::is_signed) {
            throw bad_int_cast(row_, col_);
        }
        negative = true;
    } else if (c == '+') {
        // skip
    } else if (c >= '0' && c <= '9') {
        res = c - '0';
    } else {
        throw bad_int_cast(row_, col_);
    }

    // read actual number:
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                throw bad_int_cast(row_, col_);
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        } else if (c >= '0' && c <= '9') {
            res = 10 * res + (c - '0');
        } else if (c == ' ' || c == '\t') {
            if (read_skip_whitespace(c, quoted)) {
                throw bad_int_cast(row_, col_);
            }
            break;
        } else {
            throw bad_int_cast(row_, col_);
        }
    }

    if (negative) {
        return -res;
    } else {
        return res;
    }
}

template<typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, T>::type Parser::read() {
    begin_read();
    T a = 0;
    T b = 0.0;
    T scale = 1.0;
    bool quoted = false;
    bool negative = false;
    char c;

    if (!read_skip_whitespace(c, quoted)) {
        throw bad_float_cast(row_, col_);
    }

    if (c == '-') {
        negative = true;
    } else if (c == '+') {
        // skip
    } else if (c >= '0' && c <= '9') {
        a = c - '0';
    } else if (c == '.') {
        goto read_after_point;
    } else {
        throw bad_float_cast(row_, col_);
    }

    // read before point:
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                throw bad_float_cast(row_, col_);
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        } else if (c >= '0' && c <= '9') {
            a = 10.0 * a + (c - '0');
        } else if (c == '.') {
            goto read_after_point;
        } else if (c == 'e' || c == 'E') {
            goto read_exponent;
        } else if (c == ' ' || c == '\t') {
            if (read_skip_whitespace(c, quoted)) {
                throw bad_float_cast(row_, col_);
            }
            break;
        } else {
            throw bad_float_cast(row_, col_);
        }
    }

    if (negative) {
        return -a;
    } else {
        return a;
    }

read_after_point:
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                throw bad_float_cast(row_, col_);
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        } else if (c >= '0' && c <= '9') {
            b = 10.0 * b + (c - '0');
            scale *= 10.0;
        } else if (c == 'e' || c == 'E') {
            goto read_exponent;
        } else if (c == ' ' || c == '\t') {
            if (read_skip_whitespace(c, quoted)) {
                throw bad_float_cast(row_, col_);
            }
            break;
        } else {
            throw bad_float_cast(row_, col_);
        }
    }

    if (negative) {
        return -(a + b / scale);
    } else {
        return a + b / scale;
    }

read_exponent:
    bool e_negative = false;
    c = in.peek();
    if (c == '-') {
        e_negative = true;
        in.get();
    } else if (c == '+') {
        in.get();
    }
    T e = 0;
    T e_scale = 1;
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                throw bad_float_cast(row_, col_);
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        } else if (c >= '0' && c <= '9') {
            e = 10.0 * e + (c - '0');
            e_scale = e_scale * e_scale * e_scale * e_scale * e_scale * e_scale * e_scale * e_scale * e_scale * e_scale;  // e_scale^10
            while (c > '0') {
                e_scale *= 10.0;
                --c;
            }
        } else if (c == ' ' || c == '\t') {
            if (read_skip_whitespace(c, quoted)) {
                throw bad_float_cast(row_, col_);
            }
            break;
        } else {
            throw bad_float_cast(row_, col_);
        }
    }

    const double res = e_negative ? ((a + b / scale) / e_scale) : (a * e_scale + b * e_scale / scale);
    if (negative) {
        return -res;
    } else {
        return res;
    }
}

template<>
inline ColumnType Parser::read<ColumnType>() {
    begin_read();
    bool quoted = false;
    char c;
    ColumnType res = ColumnType::EMPTY;

    if (!read_skip_whitespace(c, quoted)) {
        return res;
    }

    if (c == '.') {
        goto read_after_point;
    }

    // read before point:
    res = ColumnType::INTEGER;
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                res = ColumnType::STRING;
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        } else if (c >= '0' && c <= '9') {
            // skip
        } else if (c == '.' && res == ColumnType::INTEGER) {
            goto read_after_point;
        } else if ((c == 'e' || c == 'E') && res == ColumnType::INTEGER) {
            goto read_exponent;
        } else if (c == ' ' || c == '\t') {
            if (read_skip_whitespace(c, quoted)) {
                res = ColumnType::STRING;
            }
            break;
        } else {
            res = ColumnType::STRING;
        }
    }
    return res;

read_after_point:
    res = ColumnType::FLOAT;
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                res = ColumnType::STRING;
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        } else if (c >= '0' && c <= '9') {
            // skip
        } else if ((c == 'e' || c == 'E') && res == ColumnType::FLOAT) {
            goto read_exponent;
        } else if (c == ' ' || c == '\t') {
            if (read_skip_whitespace(c, quoted)) {
                res = ColumnType::STRING;
            }
            break;
        } else {
            res = ColumnType::STRING;
        }
    }
    return res;

read_exponent:
    res = ColumnType::FLOAT;
    while (true) {
        if (in.eof()) {
            if (quoted) {
                throw unclosed_quotes(row_, col_);
            }
            row_finished = true;
            break;
        }
        c = in.get();
        if (c == delimiter && !quoted) {
            break;
        } else if (c == '"') {
            if (quoted && in.peek() == '"') {
                res = ColumnType::STRING;
            } else {
                quoted = !quoted;
            }
        } else if ((c == '\n' || c == '\r') && !quoted) {
            row_finished = true;
            break;
        } else if (c >= '0' && c <= '9') {
            // skip
        } else if (c == ' ' || c == '\t') {
            if (read_skip_whitespace(c, quoted)) {
                res = ColumnType::STRING;
            }
            break;
        } else {
            res = ColumnType::STRING;
        }
    }
    return res;
}

inline bool Parser::next_col() {
    if (row_finished) {
        return false;
    }
    ++col_;
    if (!col_has_been_read) {
        read<void>();
    }
    col_has_been_read = false;
    return true;
}

template<typename T>
inline Parser& operator>>(Parser& p, T& obj) {
    obj = p.read<T>();
    return p;
}
}  // namespace csv

#endif
