//
// Created by David Yang on 2025-10-13.
//

#include <spawn.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <csignal>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <stdexcept>
#include <Gao.hpp>

namespace Gao::exceptions {
    Failed_To_Create_Gaolette::Failed_To_Create_Gaolette(int code) : code(code) {
        msg_.emplace_back("Gaolette creation failed with error code: " + std::to_string(code));

        switch (code) {
            case -1: msg_.emplace_back("Unknown error occurred during Gaolette creation.");
            case 1:  msg_.emplace_back("Invalid performance specification provided.");
            case 2:  msg_.emplace_back("Insufficient system resources to create Gaolette.");
            case 3:  msg_.emplace_back("Permission denied to create Gaolette.");
            default: msg_.emplace_back("Unrecognized error code.");
        };
    }

    const char *Failed_To_Create_Gaolette::what() const noexcept {
            return msg_.data()->c_str();
    }
}

namespace Gao {
    std::filesystem::path Orchestrator::get_gao_binary() {
#if defined(__x86_64__)
        arch_ = "x86_64";
        return std::string(std::getenv("GAO_BIN_DIR")) + std::string("/x86_64");
#elif defined(__i386__)
        arch_ = "i386";
        return std::string(std::getenv("GAO_BIN_DIR")) + std::string("/i386");
#elif defined(__arm__)
        arch_ = "arm";
        return {std::getenv("GAO_BIN_DIR") + std::string("/arm")};
#elif defined(__aarch64__)
        arch_ = "aarch64";
        return {std::getenv("GAO_BIN_DIR") + std::string("/aarch64")};
#endif
    }

    Orchestrator::Log::Log(const Log_Type type, const std::string &msg_, const Log_Prio p) : msg(msg_), type(type), prio(p) {
        switch (type) {
            case Log_Type::GENERIC_SUCCESS:
                msg = "[" + colors::SUCCESS + "SUCCESS" + colors::RESET + "] " + msg_;
                break;
            case Log_Type::GENERIC_FAILURE:
                msg = "[" + colors::ERROR + "FAILURE" + colors::RESET + "] " + msg_;
                break;
            case Log_Type::GENERIC_NOTICE:
                msg = "[" + colors::NOTICE + "NOTICE" + colors::RESET + "] " + msg_;
                break;
            case Log_Type::GENERIC_WARNING:
                msg = "[" + colors::WARNING + "WARNING" + colors::RESET + "] " + msg_;
                break;
        }
    }

    void Orchestrator::log(const Log &log) {
        logs_.emplace_back(log);
    }

    void Orchestrator::dump_logs(const bool clear) {
        for (const auto& log : logs_) {
            if (log.prio == Log_Prio::ANY || (log.prio == Log_Prio::ONLY_IF_LOGGING && logging_)) {
                std::cout << log.msg << "\n";
            }
        }
        if (clear) {
            logs_.clear();
        }
    }

    void Orchestrator::print_log(bool clear) {
        if (logs_[0].prio == Log_Prio::ANY || (logs_[0].prio == Log_Prio::ONLY_IF_LOGGING && logging_)) {
            std::cout << logs_[0].msg << "\n";
        }
        if (clear) {
            // erase logs_[0]
            logs_.erase(logs_.begin());
        }
    }

    Orchestrator::Orchestrator(bool terminate_with_parent) {    // NOLINT : issue with actions_ initialization
        int sv[2]; // socket pair
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
            throw std::runtime_error("socketpair failed");
        }
        posix_spawn_file_actions_init(&actions_);

        // duplicate the child's socket to stdin/stdout
        posix_spawn_file_actions_adddup2(&actions_, sv[1], STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions_, sv[1], STDOUT_FILENO);

        status_ = posix_spawn(&pid_, get_gao_binary().c_str(),
                             &actions_, nullptr,
                             nullptr, environ);
        if (status_ != 0) {
            throw std::runtime_error("posix_spawn failed");
        }

        socket_ = sv[0];

        // we can now close the child's end on the parent as it only needs it's end of the socket
        close(sv[1]);
    }

    Orchestrator::~Orchestrator() {
        posix_spawn_file_actions_destroy(&actions_);
        if (socket_ != -1) {
            close(socket_);
        }
        if (pid_ > 0) {
            if (waitpid(pid_, &status_, 0) > 0) {
                if (WIFEXITED(status_)) {
                    std::cout << "Exited with status " << WEXITSTATUS(status_) << std::endl;
                } else if (WIFSIGNALED(status_)) {
                    std::cout << "Killed by signal " << WTERMSIG(status_) << std::endl;
                } else {
                    std::cout << "Exited with unknown status" << std::endl;
                }
            }
        }
    }


    std::string Orchestrator::read_line() const {
        constexpr size_t BUF_SIZE = 1024;
        char* buf = new char[BUF_SIZE];  // caller must delete[]
        size_t len = 0;

        while (true) {
            ssize_t nread = ::read(socket_, buf + len, BUF_SIZE - len - 1);
            if (nread == -1) {
                delete[] buf;
                throw std::runtime_error("read failed");
            }
            if (nread == 0) {  // EOF
                break;
            }
            len += nread;

            // stop if newline is seen
            if (buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
                std::string temp = buf;
                delete[] buf;
                return temp;
            }

            // safety: stop if buffer full (no newline)
            if (len >= BUF_SIZE - 1) {
                buf[len] = '\0';
                std::string temp = buf;
                delete[] buf;
                return temp;
            }
        }

        // no newline before EOF
        buf[len] = '\0';
        std::string temp = buf;
        delete[] buf;
        return temp;
    }

    int Orchestrator::write_line(const std::string &line) const {
        return static_cast<int>(write(socket_, (line + "\n").c_str(), strlen(line.c_str())));
    }

    Gaolette create_gaolette(Perf_Spec spec, const Orchestrator &gao_p) {
        std::string gao_instruction = "crt:";
        gao_instruction.append(std::to_string(spec.size_) + ",");
        gao_instruction.append(std::to_string(static_cast<int>(spec.memory_policy_)) + ",");
        gao_instruction.append(std::to_string(spec.max_memory_usage_) + ",");
        gao_instruction.append(std::to_string(spec.max_cpu_cores_));

        gao_p.write_line(gao_instruction);  // NOLINT

        // we need to wait until read_line contains valid buffer space that we can read
        const std::string response = gao_p.read_line();
        if (response.empty() || response.substr(0, 2) == "-1") {
            throw std::runtime_error("Gaolette creation failed"); // change to custom error
        }

        // response comes in the form:
        // OK:<gaolette_id_t>
        // or
        // ERR:<error_code>
        if (response.substr(0, 3) == "ERR") {
            int error_code = std::stoi(response.substr(4));
            throw exceptions::Failed_To_Create_Gaolette(error_code);
        }

        if (response.substr(0, 2) == "OK") {
            //parse until ,
            gaolette_id_t id = std::stoi(response.substr(3));
            return Gaolette{id, State::Operational, spec};
        }

        throw std::runtime_error("Invalid response from Gaolette creation");
    }

    inline int destroy_gaolette(Gaolette& gaolette, const Orchestrator& gao_p) {
        std::string gao_instruction = "del:";
        gao_instruction.append(std::to_string(gaolette.id));
        gao_p.write_line(gao_instruction);  // NOLINT
        std::string response = gao_p.read_line();
        if (response.substr(0, 2) == "OK") {
            gaolette.id = -1;
            gaolette.state = State::ShutDown;
            return 0; // success
        }

        return -1; // failure
    }

    inline void fetch_state(Gaolette& gaolette, const Orchestrator& gao_p) {
        gao_p.write_line(std::string("get:state"));
        std::string response = gao_p.read_line();
        if (response.substr(0, 2) == "OK") {
            switch (int state_code = std::stoi(response.substr(3))) { //NOLINT
                case 0:
                    gaolette.state = State::Operational;
                    break;
                case 1:
                    gaolette.state = State::ShutDown;
                    break;
                case 2:
                    gaolette.state = State::Locked;
                    break;
                case 3:
                    gaolette.state = State::Operating;
                    break;
                case 4:
                    gaolette.state = State::Illformed;
                    break;
                default:
                    throw std::runtime_error("Unknown state code received from Gaolette");
            }
        } else {
            throw std::runtime_error("Failed to fetch Gaolette state");
        }
    }

}