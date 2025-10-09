//
// Created by David Yang on 2025-09-17.
//

#ifndef COMM_HPP
#define COMM_HPP

// Communications hub for interprocess transmission; hosts the main loop
// that runs the I/O required for IPC between the wrapper and Gao.

#include <unistd.h>
#include <stddef.h>

struct nothrow_t {
    explicit nothrow_t() = default;
};

inline constexpr nothrow_t nothrow{};

void* operator new(size_t size, nothrow_t const&) noexcept;
void* operator new[](size_t size, nothrow_t const&) noexcept;
void operator delete(void* ptr, const nothrow_t&) noexcept;
void operator delete[](void* ptr, const nothrow_t&) noexcept;

template <typename T, typename U>
struct Pair {
    T first;
    U second;

    typedef T first_type;
    typedef U second_type;

    Pair(T first, U second) : first(first), second(second) {}
    Pair() : first(), second() {}
};

template <typename T, typename U>
Pair<T, U> make_pair(T first, U second) {
    return {first, second};
}

enum class Comm_Status {
    Active,  // actively transmits all thats transferable
    Passive  // only transmits when necessary
};

class Comm {
    // by default, Gao is passive and only receives incoming packets.
    // transmission by Gao only occurs at program outputs and errors
    // as well as if logging is turned on
    Comm_Status status = Comm_Status::Passive;

public:
    Comm_Status get_status();
    void set_status(Comm_Status status);
    Pair<char*, ssize_t> read_line();
    int write_line(const char* line);
};

#endif // COMM_HPP