//
// Created by David Yang on 2025-10-10.
//
#include "header/thread.hpp"

template<typename Ret, typename... Args>
Thread::Thread(Ret (*func_pointer)(Args...), Args... args) : func_pointer_(func_pointer(args)) {

}
