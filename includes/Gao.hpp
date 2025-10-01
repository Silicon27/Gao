//
// Created by David Yang on 2025-09-16.
//

#ifndef GAO_HPP
#define GAO_HPP

#include <spawn.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstddef>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <string>

namespace Gao {
    enum class State {
        Operational, // the Gaolette is open for read/write
        ShutDown, // the Gaolette is closed for read/write
        Locked, // the Gaolette is locked and is read-only
        Operating // the Gaolette is running code and is open for read/write for console
    };

    // I kept the State::Operating to write and read for console in case the code operating in the Gaolette
    // requires I/O

    enum class Memory_Policy {
        // defines contraction/expansion policies of the sandbox memory
        STATIC,    // fixed size, no contraction/expansion
        DYNAMIC    // allow contraction/expansion within limits
    };

    struct Perf_Spec {
        // basic performance specification
        std::size_t size_;           // total memory delegated to the sandbox in bytes
        Memory_Policy memory_policy_;
        std::size_t max_memory_usage_;    // in bytes
        std::size_t max_cpu_cores_;

        // extended process/resource limits
        /*std::size_t max_open_files_;
        std::size_t max_threads_;
        std::size_t max_stack_size_;
        std::size_t max_runtime_ms_;
        std::size_t max_cpu_time_ms_;

        // security & isolation
        bool allow_network_;
        bool allow_filesystem_;
        bool restrict_privileges_;
        std::vector<std::string> allowed_syscalls_;
        std::vector<std::string> allowed_env_vars_;

        // scheduling & priorities
        int cpu_priority_;
        int io_priority_;
        std::vector<int> cpu_affinity_;

        // sandboxing features
        std::string chroot_dir_;
        std::vector<std::string> allowed_mounts_;
        bool enable_seccomp_;
        bool enable_cgroups_;*/
    };

    struct Gaolette {
        void* start;
        size_t size;
        State state;
        Perf_Spec perf_spec;
    };

    extern char **environ;

    /// Launch is, well if I were to define it, the API gateway at the base
    /// that connects Gao to the wrapper
    class Launch {
        pid_t pid_ = 0;
        int status_ = 0;
        static const char* arch_;
        posix_spawn_file_actions_t actions_;
        int socket_;

        static std::filesystem::path get_gao_binary() {
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

    public:
        explicit Launch() {
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

        ~Launch() {
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

        [[nodiscard]] char* read_line() const {
            
        }

        int write_line(const char* line) const {
            return static_cast<int>(write(socket_, line, strlen(line)));
        }
    };


    Gaolette create_gaolette(Perf_Spec spec,  Launch& gao_p) {
        std::string gao_instruction = "crt:";
        gao_instruction.append(std::to_string(spec.size_) + ",");
        gao_instruction.append(std::to_string(static_cast<int>(spec.memory_policy_)) + ",");
        gao_instruction.append(std::to_string(spec.max_memory_usage_) + ",");
        gao_instruction.append(std::to_string(spec.max_cpu_cores_));
        gao_p.write_line(gao_instruction.c_str());

        // receive the SUC_INIT_GAOLETTE package from gao
        std::string gao_response = gao_p.read_line();


    }

    std::string convert_to_gao_instruction(Perf_Spec perf_spec) {
        std::string gao_instruction = "create:";
    }
}

#endif //GAO_HPP
