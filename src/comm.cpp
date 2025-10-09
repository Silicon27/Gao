//
// Created by David Yang on 2025-10-01.
//

#include "header/comm.hpp"

#include <cstdlib>
#include <cstring>

void* operator new(size_t size, nothrow_t const&) noexcept {
    return ::malloc(size);
}

void* operator new[](size_t size, nothrow_t const&) noexcept {
    return ::malloc(size);
}

void operator delete(void* ptr, const nothrow_t&) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, const nothrow_t&) noexcept {
    free(ptr);
}

template <typename T, typename U>
Pair<T, U> make_pair(T first, U second) noexcept {
    return Pair<T, U>{first, second};
}

Comm_Status Comm::get_status() {
    return status;
}

void Comm::set_status(Comm_Status status) {
    this->status = status;
}

[[nodiscard]] Pair<char*, ssize_t> Comm::read_line() noexcept {
    constexpr size_t BUF_SIZE = 1024;
    char* buf = new(::nothrow) char[BUF_SIZE];  // caller must delete[]
    size_t len = 0;

    while (true) {
        ssize_t nread = ::read(0, buf + len, BUF_SIZE - len - 1);
        if (nread == -1) {
            delete[] buf;
            return Pair<char*, ssize_t>{nullptr, -1};
        }
        if (nread == 0) {  // EOF
            break;
        }
        len += nread;

        // stop if newline is seen
        for (size_t i = 0; i < len; ++i) {
            if (buf[i] == '\n') {
                buf[i] = '\0';
                return Pair<char*, ssize_t>{buf, static_cast<ssize_t>(i)};
            }
        }

        // safety: stop if buffer full (no newline)
        if (len >= BUF_SIZE - 1) {
            buf[len] = '\0';
            return Pair<char*, ssize_t>{buf, static_cast<ssize_t>(len)};
        }
    }

    // no newline before EOF
    buf[len] = '\0';
    return Pair<char*, ssize_t>{buf, static_cast<ssize_t>(len)};
}

int Comm::write_line(const char* line) noexcept {
    return ::write(1, line, strlen(line));
}