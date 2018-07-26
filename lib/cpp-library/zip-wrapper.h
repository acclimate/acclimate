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

#ifndef ZIP_WRAPPER_H
#define ZIP_WRAPPER_H

#include <zip.h>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace libzip {
class exception : public std::runtime_error {
  public:
    exception(const std::string& msg) : std::runtime_error(msg){};
};

class streambuf : public std::streambuf {
  private:
    zip_file* m;
    std::vector<char> buffer;

  public:
    streambuf(zip_file* file, std::size_t buffer_size = 4096) : m(file), buffer(buffer_size) {
        char* end = &buffer[0] + buffer_size;
        setg(end, end, end);
    }
    streambuf(const streambuf&) = delete;
    streambuf(streambuf&&) = default;
    streambuf& operator=(const streambuf&) = delete;
    streambuf& operator=(streambuf&&) = default;

  private:
    std::streambuf::int_type underflow() override {
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }
        char* start = &buffer[0];
        const size_t n = zip_fread(m, start, buffer.size());
        if (n < 0) {
            throw exception("could not read from zipped file");
        }
        if (n == 0) {
            return traits_type::eof();
        }
        setg(start, start, start + n);
        return traits_type::to_int_type(*gptr());
    }
};

class ifstream : public std::istream {
  private:
    zip_file* m;

  public:
    ifstream(zip_file* file) : m(file), std::istream(new streambuf(file)){};
    ifstream(ifstream&& other) : std::istream(std::move(other)), m(other.m){};
    virtual ~ifstream() {
        delete rdbuf();
        zip_fclose(m);
    };
};

class File {};

class Archive {
  private:
    zip* m;

  public:
    Archive(const std::string& filename) {
        int err;
        m = zip_open(filename.c_str(), 0, &err);
        if (!m) {
            char buf[256];
            zip_error_to_str(buf, sizeof(buf), err, errno);
            throw exception(buf);
        }
    }
    ~Archive() { zip_close(m); }
    zip* get_handle() { return m; }
    ifstream open(const std::string& name, zip_flags_t flags = 0, const std::string& password = "") {
        zip_file* file;

        if (password.size() > 0) {
            file = zip_fopen_encrypted(m, name.c_str(), flags, password.c_str());
        } else {
            file = zip_fopen(m, name.c_str(), flags);
        }

        if (file == nullptr) {
            throw exception(zip_strerror(m));
        }

        return ifstream(file);
    }
};
}

#endif
