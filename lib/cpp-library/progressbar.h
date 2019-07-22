/*
  Copyright (C) 2018 Sven Willner <sven.willner@gmail.com>

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

#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WINDOWS
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif

namespace progressbar {

class ProgressBar {
  protected:
    using ticks = std::chrono::steady_clock::rep;
    std::vector<char> buf;
    bool is_tty;
    bool closed = false;
    bool subbar;
    std::mutex mutex_m;
    std::atomic<std::size_t> current;
    std::size_t reprint_next = 1;
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    std::size_t eta_from_iter = 0;
    std::chrono::time_point<std::chrono::steady_clock> eta_from_time;
    std::size_t last_reprint_iter = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_reprint_time;
    const ticks min_reprint_time;
#ifndef PROGRESSBAR_SILENT
    std::FILE* out;
#endif

    inline void control_endl() noexcept {
#ifndef PROGRESSBAR_SILENT
        fputc('\n', out);
#endif
    }

    inline void control_goto_bol() noexcept {
#ifndef PROGRESSBAR_SILENT
        fputc('\r', out);
#endif
    }

    inline void control_clear_to_eol() noexcept {
#ifndef PROGRESSBAR_SILENT
        fputs("\x1b[K", out);
#endif
    }

    inline void control_go_up() noexcept {
#ifndef PROGRESSBAR_SILENT
        fputs("\x1b[F", out);
#endif
    }

    inline void control_print(const char* s) noexcept {
#ifndef PROGRESSBAR_SILENT
        fputs(s, out);
#endif
    }

    inline void control_flush() noexcept {
#ifndef PROGRESSBAR_SILENT
        fflush(out);
#endif
    }

    void update(std::size_t n) noexcept {
        current = std::min(current + n, total);
#ifndef PROGRESSBAR_SILENT
        if (current >= reprint_next) {
            std::lock_guard<std::mutex> guard(mutex_m);
            if (!closed) {
                recalc_and_print();
            }
        }
#endif
    }

    void recalc_and_print(bool force = false) noexcept {
        auto now = std::chrono::steady_clock::now();
        auto duration = (now - last_reprint_time).count();
        reprint_next = current + (current - last_reprint_iter) * min_reprint_time / std::max(duration, min_reprint_time) + 1;
        if (duration >= min_reprint_time || force) {
            auto freq = std::chrono::steady_clock::period::den
                        * ((1 - smoothing) * (current - last_reprint_iter) / static_cast<float>(duration)
                           + smoothing * (current - eta_from_iter) / static_cast<float>((now - eta_from_time).count()))
                        / std::chrono::steady_clock::period::num;
            ticks etr = 0;
            if (current != eta_from_iter) {
                etr = std::lrint((total - current)
                                 * ((1 - smoothing) * duration / static_cast<float>((current - last_reprint_iter))
                                    + smoothing * (now - eta_from_time).count() / static_cast<float>(current - eta_from_iter)));
            }

            print_bar(freq, (now - start_time).count(), etr);

            last_reprint_time = now;
            last_reprint_iter = current;
        }
    }

    template<typename... Args>
    static inline bool safe_print(char*& c, std::size_t& buf_remaining, const char* fmt, Args... args) noexcept {
        const auto res = std::snprintf(c, buf_remaining, fmt, args...);  // NOLINT(hicpp-vararg,cppcoreguidelines-pro-type-vararg)
        if (res < 0 || buf_remaining <= static_cast<std::size_t>(res)) {
            return false;
        }
        buf_remaining -= res;
        c += res;
        return true;
    }

    static inline bool safe_print(char*& c, std::size_t& buf_remaining, const char* str) noexcept {
        const auto res = std::snprintf(c, buf_remaining, "%s", str);  // NOLINT(hicpp-vararg,cppcoreguidelines-pro-type-vararg)
        if (res < 0 || buf_remaining <= static_cast<std::size_t>(res)) {
            return false;
        }
        buf_remaining -= res;
        c += res;
        return true;
    }

    inline static bool safe_print_duration(char*& c, std::size_t& buf_remaining, ticks t) noexcept {
        auto dur = std::chrono::steady_clock::duration(t);
        auto d = std::chrono::duration_cast<std::chrono::duration<ticks, std::ratio<3600 * 24>>>(dur);
        auto h = std::chrono::duration_cast<std::chrono::hours>(dur -= d);
        auto m = std::chrono::duration_cast<std::chrono::minutes>(dur -= h);
        auto s = std::chrono::duration_cast<std::chrono::seconds>(dur -= m);
        if (d.count() > 0) {
            return safe_print(c, buf_remaining, "%u-%02u:%02u:%02u", d.count(), h.count(), m.count(), s.count());
        }
        if (h.count() > 0) {
            return safe_print(c, buf_remaining, "%02u:%02u:%02u", h.count(), m.count(), s.count());
        }
        if (m.count() > 0) {
            return safe_print(c, buf_remaining, "%02u:%02u", m.count(), s.count());
        }
        return safe_print(c, buf_remaining, "%us", s.count());
    }

    inline void print_to_buf(float freq, ticks runtime, ticks etr) noexcept {
        char* c = &buf[0];
        auto buf_remaining = buf.size() - 1;
        buf[buf_remaining] = '\0';

        // write prefix (before actual bar including percentage):
        if (!description.empty()) {
            if (!safe_print(c, buf_remaining, "%s ", description.c_str())) {
                return;
            }
        }
        const auto prefix_len = buf.size() - buf_remaining - 1;

        // write postfix (after actual bar):
        if (!safe_print(c, buf_remaining, " %u/%u  ", static_cast<std::size_t>(current), total)) {
            return;
        }
        if (!safe_print_duration(c, buf_remaining, runtime)) {
            return;
        }
        if (freq >= 1 || freq <= 1e-9) {
            if (!safe_print(c, buf_remaining, "  %.1f/s  ", freq)) {
                return;
            }
        } else {
            if (!safe_print(c, buf_remaining, "  %.1fs  ", 1 / freq)) {
                return;
            }
        }
        if (current == total) {
            if (!safe_print(c, buf_remaining, "done")) {
                return;
            }
        } else if (current == eta_from_iter) {
            if (!safe_print(c, buf_remaining, "--")) {
                return;
            }
        } else {
            if (!safe_print_duration(c, buf_remaining, etr)) {
                return;
            }
        }
        const auto postfix_len = buf.size() - buf_remaining - prefix_len - 1;

        c = &buf[prefix_len];

        // move postfix to end of buffer
        std::memmove(c + buf_remaining, c, postfix_len);

        if (buf_remaining > 5 * buf.size() / 7) {
            // for wide terminals make actual bar shorter
            if (buf.size() / 8 > prefix_len + 4) {
                const auto padding_before = buf.size() / 8 - prefix_len - 4;
                std::memset(c, ' ', padding_before);
                c += padding_before;
                buf_remaining -= padding_before;
            }
            if (buf.size() / 4 > postfix_len) {
                const auto padding_after = buf.size() / 4 - postfix_len;
                buf_remaining -= padding_after;
                std::memset(c + buf_remaining, ' ', padding_after);
            }
        }

        if (buf_remaining >= 4) {
            if (!safe_print(c, buf_remaining, "%3u%% ", std::lrint(current * 100 / static_cast<float>(total)))) {
                return;
            }
        }

        if (buf_remaining >= 3) {
            *c = bar_open;
            *(c + buf_remaining - 1) = bar_close;
            ++c;
            buf_remaining -= 2;
            auto done_chars = static_cast<std::size_t>(current * buf_remaining / total);
            std::memset(c, bar_done, done_chars);
            std::memset(c + done_chars, bar_left, buf_remaining - done_chars);
            if (current < total) {
                *(c + done_chars) = bar_cur;
            }
        }
    }

    inline void print_bar(float freq, ticks runtime, ticks etr) noexcept {
        if (is_tty) {
#ifndef PROGRESSBAR_SILENT
#ifdef _WINDOWS
            CONSOLE_SCREEN_BUFFER_INFO c;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &c);
            buf.resize(c.srWindow.Right - c.srWindow.Left);
#else
            winsize w = {};
            if (ioctl(fileno(out), TIOCGWINSZ, &w) >= 0) {  // NOLINT(hicpp-vararg,cppcoreguidelines-pro-type-vararg)
                buf.resize(w.ws_col);
            } else {
                buf.resize(80);
            }
#endif
#else
            buf.resize(80);
#endif
        }
        print_to_buf(freq, runtime, etr);
        if (is_tty) {
            control_goto_bol();
        }
        control_print(&buf[0]);
        if (!is_tty) {
            control_endl();
        }
        control_flush();
    }

  public:
    const std::size_t total;
    std::string description;
    float smoothing = 0.75;
    char bar_open = '[';
    char bar_close = ']';
    char bar_done = '=';
    char bar_cur = '>';
    char bar_left = ' ';

    explicit ProgressBar(
        std::size_t total_p, std::string description_p = "", bool subbar_p = false, std::FILE* out_p = stdout, std::size_t min_reprint_time_ms = 100) noexcept
        : min_reprint_time(std::chrono::steady_clock::duration(std::chrono::milliseconds(min_reprint_time_ms)).count()),
#ifndef PROGRESSBAR_SILENT
          out(out_p),
#endif
          total(total_p),
          subbar(subbar_p),
          current(0),
          description(std::move(description_p)) {
        std::lock_guard<std::mutex> guard(mutex_m);
        start_time = std::chrono::steady_clock::now();
        eta_from_time = start_time;
        last_reprint_time = start_time;
        is_tty = isatty(fileno(out_p)) != 0;
        if (!is_tty) {
            buf.resize(65);
        }
        if (subbar) {
            control_endl();
        }
        print_bar(0, 0, 0);
    }
    ~ProgressBar() noexcept { close(); }

    inline ProgressBar& operator++() noexcept {
        update(1);
        return *this;
    }

    inline void operator+=(std::size_t n) noexcept { update(n); }
    inline ProgressBar& operator=(std::size_t n) noexcept {
        if (n > current) {
            update(n - current);
        }
        return *this;
    }

    inline void reset_eta() noexcept {
        std::lock_guard<std::mutex> guard(mutex_m);
        eta_from_iter = current;
        eta_from_time = std::chrono::steady_clock::now();
    }

    inline void close(bool remove = false) noexcept {
        if (closed) {
            return;
        }
        std::lock_guard<std::mutex> guard(mutex_m);
        auto total_duration = (std::chrono::steady_clock::now() - start_time).count();
        auto freq = current * std::chrono::steady_clock::period::den / static_cast<float>(total_duration * std::chrono::steady_clock::period::num);
        current = total;
        if (remove && is_tty) {
            control_goto_bol();
            control_clear_to_eol();
            if (subbar) {
                control_go_up();
            }
        } else {
            print_bar(freq, total_duration, 0);
            if (is_tty) {
                control_endl();
            }
        }
        control_flush();
        closed = true;
    }

    inline void abort() noexcept {
        if (closed) {
            return;
        }
        std::lock_guard<std::mutex> guard(mutex_m);
        if (is_tty) {
            control_endl();
        }
        control_flush();
        closed = true;
    }

    inline void resume() noexcept {
        closed = false;
        refresh();
    }

    inline void println(const std::string& s = "", bool reprint = true) noexcept {
        std::lock_guard<std::mutex> guard(mutex_m);
        if (is_tty && !closed) {
            control_goto_bol();
            control_clear_to_eol();
        }
        control_print(s.c_str());
        control_endl();
        if (!closed && reprint) {
            recalc_and_print(true);
        }
    }

    inline void refresh() noexcept {
        std::lock_guard<std::mutex> guard(mutex_m);
        if (!closed) {
            recalc_and_print(true);
        }
    }

    inline void flush() noexcept {
        std::lock_guard<std::mutex> guard(mutex_m);
        control_flush();
    }
};

}  // namespace progressbar

#endif
