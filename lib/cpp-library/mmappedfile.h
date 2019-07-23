/*
  Copyright (C) 2017-2019 Sven Willner <sven.willner@gmail.com>

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

#ifndef MMAPPED_FILE_H
#define MMAPPED_FILE_H

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iomanip>
#include <string>
#include <vector>

template<typename T>
inline T* open_mmapped_file(const std::string& filename, std::size_t size, char mode, int& fd) {
    T* res = nullptr;
    switch (mode) {
        case 'r': {
            fd = open(filename.c_str(), O_RDONLY | O_CLOEXEC);  // NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,hicpp-signed-bitwise)
            if (fd < 0) {
                throw std::runtime_error("could not open file " + filename);
            }
            res = static_cast<T*>(mmap(nullptr, size * sizeof(T), PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0));  // NOLINT(hicpp-signed-bitwise)
        } break;

        case 'w': {
            fd = open(filename.c_str(), O_RDWR | O_CREAT | O_CLOEXEC,  // NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,hicpp-signed-bitwise)
                      static_cast<mode_t>(0600));
            if (fd < 0) {
                throw std::runtime_error("could not create file " + filename);
            }
            int rc = lseek(fd, size * sizeof(T) - 1, SEEK_SET);
            if (rc < 0) {
                throw std::runtime_error("lseek failed");
            }
            rc = write(fd, "", 1);
            if (rc < 0) {
                throw std::runtime_error("write failed");
            }
            res = static_cast<T*>(mmap(nullptr, size * sizeof(T), PROT_WRITE, MAP_SHARED, fd, 0));
        } break;

        default:
            throw std::runtime_error("unknown file mode");
    }

    if (res == MAP_FAILED) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
        throw std::runtime_error("mmap failed");
    }
    int rc = madvise(res, size * sizeof(T), MADV_WILLNEED | MADV_SEQUENTIAL);  // NOLINT(hicpp-signed-bitwise)
    if (rc < 0) {
        delete[] res;
        throw std::runtime_error("madvice failed");
    }
    return res;
}

template<typename T>
inline void close_mmapped_file(T* p, std::size_t size, int fd) {
    munmap(p, size * sizeof(T));
    close(fd);
}

template<typename T = char>
class MMappedFile {
  protected:
    int fd = 0;
    std::unique_ptr<T[]> data;
    std::size_t size_m = 0;

  public:
    MMappedFile() = default;
    MMappedFile(const MMappedFile&) = delete;
    MMappedFile(MMappedFile&&) noexcept = default;

    void open(const std::string& filename, std::size_t size_p, char mode = '\r') {
        close();
        data.reset(open_mmapped_file<T>(filename, size_p, mode, fd));
        size_m = size_p;
    }

    void close() {
        if (data) {
            close_mmapped_file<T>(data.get(), size_m, fd);
            data.release();
            size_m = 0;
        }
    }

    ~MMappedFile() { close(); }
    constexpr operator bool() const { return data; }  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
    constexpr T* get() { return data.get(); }
    constexpr T* get() const { return data.get(); }
    constexpr std::size_t size() const { return size_m; }
};

#endif
