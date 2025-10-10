//
// Created by David Yang on 2025-10-10.
//

#ifndef THREAD_HPP
#define THREAD_HPP

#include <pthread.h>

/// Gao's own thread library for threading

class Thread {
    pthread_t thread_;
    bool joined_or_detached_;
    // NOTE function signature might be off, more signatures exists as well, make a separate function type to handle all forms of functions
    template <typename Ret, typename... Args>
    Ret (*func_pointer_)(Args...) = nullptr;
public:
    void join();
    void detach();
    void joinable() const;

    template <typename Ret, typename... Args>
    Thread(Ret (*func_pointer)(Args...), Args... args);
    ~Thread();
};

#endif //THREAD_HPP
