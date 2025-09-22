//
// Created by David Yang on 2025-09-17.
//

#ifndef COMM_HPP
#define COMM_HPP

// Communications hub for interprocess transmission; hosts the main loop
// that runs the I/O required for IPC between the wrapper and Gao.

enum class Comm_Status {
    Active, // actively transmits all thats transferable
    Passive // only transmits when necessary
};

class Comm {
    // by default, Gao is passive and only receives incoming packets.
    // transmission by Gao only occurs at program outputs and errors
    // as well as if logging is turned on
    Comm_Status status;

public:

};

#endif //COMM_HPP
