//
// Created by David Yang on 2025-10-01.
//

#include "header/comm.hpp"

Comm_Status Comm::get_status() {
    return status;
}

void Comm::set_status(Comm_Status status) {
    this->status = status;
}

Pair<char*, ssize_t> Comm::read_line() {
    return {nullptr, 0}; // placeholder implementation
}