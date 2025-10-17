//
// Created by David Yang on 2025-09-16.
//

#ifndef GAO_HPP
#define GAO_HPP

#include <spawn.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <csignal>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

namespace Gao::exceptions {
    ///
    /// @class Failed_To_Create_Gaolette
    /// @brief Exception thrown with added context based off code.
    ///
    /// Gaolette creation may result in failure, in such cases this exception handles
    /// all known error codes with added context based off the code.
    class Failed_To_Create_Gaolette : public std::exception {
        std::vector<std::string> msg_;
        int code = -1;
    public:
        explicit Failed_To_Create_Gaolette(int code);

        [[nodiscard]] const char* what() const noexcept override;
    };

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
    inline namespace colors {
        constexpr auto RESET = "\033[0m";
        constexpr auto RED = "\033[31m";
        constexpr auto GREEN = "\033[32m";
        constexpr auto YELLOW = "\033[33m";
        constexpr auto BLUE = "\033[34m";
        constexpr auto MAGENTA = "\033[35m";
        constexpr auto CYAN = "\033[36m";
        constexpr auto BOLD = "\033[1m";
        constexpr auto UNDERLINE = "\033[4m";

        constexpr auto BG_RED = "\033[41m";
        constexpr auto BG_GREEN = "\033[42m";
        constexpr auto BG_YELLOW = "\033[43m";
        constexpr auto BG_BLUE = "\033[44m";
        constexpr auto BG_MAGENTA = "\033[45m";
        constexpr auto BG_CYAN = "\033[46m";
        constexpr auto BG_WHITE = "\033[47m";
        constexpr auto BG_RESET = "\033[49m";


        constexpr inline auto ERROR = std::string("\033[31m") + "\033[1m";
        constexpr inline auto SUCCESS = std::string("\033[32m") + "\033[1m";
        constexpr inline auto WARNING = std::string("\033[33m") + "\033[1m";
        constexpr inline auto NOTICE = std::string("\033[34m") + "\033[1m";
    }

    /// @enum State
    /// @brief Possible states a Gaolette may present itself.
    ///
    /// Allows Gao to restrict and place constraints on what the user may
    /// do to a Gaolette at any given time.
    enum class State {
        Operational, ///< the Gaolette is open for read/write
        ShutDown, ///< the Gaolette is closed for read/write (Gaolette has been killed)
        Locked, ///< the Gaolette is locked and is read-only
        Operating, ///< the Gaolette is running code and is open for read/write for console
        Illformed ///< the Gaolette is ill-formed and cannot be used
    };

    // I kept the State::Operating to write/read in
    // case the code operating in the Gaolette requires I/O

    /// @enum Memory_Policy
    /// @brief Specifies ability for Gaolette to expand/contract when needed
    enum class Memory_Policy {
        // defines contraction/expansion policies of the Gaolette's memory
        STATIC,    ///< fixed size, no contraction/expansion
        DYNAMIC    ///< allow contraction/expansion within limits
    };

    /// @struct Perf_Spec
    /// @brief Performance Specification for Gaolette instance.
    ///
    /// Specifies constraints for the Gaolette instance if needed.
    /// All fields have default values if not specified.
    struct Perf_Spec {
        // basic performance specification
        std::size_t size_;           ///< total memory delegated to the sandbox in bytes
        Memory_Policy memory_policy_;
        std::size_t max_memory_usage_;    ///< in bytes
        std::size_t max_cpu_cores_;

        // extended process/resource limits
        /*std::size_t max_open_files_;
        std::size_t max_threads_;
        std::size_t max_stack_size_;
        std::size_t max_runtime_ms_;
        std::size_t max_cpu_time_ms_;

        // security and isolation
        bool allow_network_;
        bool allow_filesystem_;
        bool restrict_privileges_;
        std::vector<std::string> allowed_syscalls_;
        std::vector<std::string> allowed_env_vars_;

        // scheduling and priorities
        int cpu_priority_;
        int io_priority_;
        std::vector<int> cpu_affinity_;

        // sandboxing features
        std::string chroot_dir_;
        std::vector<std::string> allowed_mounts_;
        bool enable_seccomp_;
        bool enable_cgroups_;*/
    };

    /// Gaolette ID type used to reference some Gaolette.
    /// -1 specifies an ill-formed gaolette
    using gaolette_id_t = int;

    /// @struct Gaolette
    /// @brief Gaolette instance
    ///
    /// Represents a Gaolette instance.
    struct Gaolette {
        gaolette_id_t id;
        State state;
        Perf_Spec perf_spec;
    };

    extern char **environ;

    /// @class Orchestrator
    /// @brief Low-level controller for a Gao process, aka the gateway API that links Gao processes to the Gao API.
    ///
    /// Base of all base.
    /// Handles IPC with the Gao process and manages its lifecycle.
    class Orchestrator {
        pid_t pid_ = 0;
        int status_ = 0;
        static const char* arch_;
        posix_spawn_file_actions_t actions_;
        int socket_;
        bool logging_ = false;

        static std::filesystem::path get_gao_binary();

        // NOTICE improve logging, its so ass rn

        enum class Log_Type {
            GENERIC_SUCCESS,
            GENERIC_FAILURE,
            GENERIC_NOTICE,
            GENERIC_WARNING,

        };

        /// Log priority, specifies whether a log is to be printed regardless of the logging_ member
        enum class Log_Prio {
            ONLY_IF_LOGGING, /// only if logging_ is true
            ANY /// will always be printed if dump_logs/print_log is invoked
        };

        struct Log {
            std::string msg;
            std::string time;
            Log_Type type;
            Log_Prio prio;

            explicit Log(Log_Type type, const std::string& msg_, Log_Prio p);
        };

        std::vector<Log> logs_;

        ///@brief Add to logs_ regardless of logging_ because of Log_Prio
        /// @param log Log object that would be added to logs_
        void log(const Log& log);

        ///@brief Dumps all logs in logs_
        /// @param clear whether to clear logs_ after execution
        void dump_logs(bool clear = true);

        ///@brief Prints oldest log in logs_
        ///@param clear whether to clear logs_ after execution
        void print_log(bool clear = true);


    public:
        /// @brief Constructor and initializer for Orchestrator instances and Gao processes respectively.
        ///
        /// Initializes and modifies Gao processes on launch to ensure IPC
        /// @param terminate_with_parent whether the child process should after the death of the parent
        /// continue and become an orphan (possibly adopted by reaper) or terminate with the parent.
        explicit Orchestrator(bool terminate_with_parent = true);

        /// Destructor for Orchestrator instances and Gao processes.
        ~Orchestrator();

        ///@brief reads from socket_ into a buffer until either the buffer is maxed out or it hits a newline.
        ///@return the read line as a std::string, empty string on error.
        [[nodiscard]] std::string read_line() const;

        ///@brief writes a line to the Gao process via writing to socket_.
        ///
        /// @param line the line to write, a newline is appended automatically.
        /// @return number of bytes written, -1 on error.
        int write_line(const std::string& line) const; // NOLINT
    };

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

    ///@brief Creates a Gaolette on the Gao process held by the gao_p Orchestrator instance.
    ///
    /// @param spec Performance specification for the Gaolette to be created.
    /// @param gao_p Orchestrator instance holding the Gao process to create the Gaolette on.
    /// @return the created Gaolette instance.
    /// @throws exceptions::Failed_To_Create_Gaolette if creation fails.
    inline Gaolette create_gaolette(Perf_Spec spec, const Orchestrator& gao_p) {
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

    ///@brief Destroys a Gaolette held by the gao_p Orchestrator instance.
    ///
    /// @param gaolette the Gaolette instance to destroy.
    /// @param gao_p Orchestrator instance holding the Gao process to destroy the Gaolette on.
    /// @return 0 on success, -1 on failure.
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

    ///@brief Updates state of Gaolette instance held by gao_p Orchestrator instance.
    ///
    ///@param gaolette the Gaolette instance to update.
    ///@param gao_p Orchestrator instance holding the Gao process to query the Gaolette state from.
    ///@return void, gaolette.state is updated in place.
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

#endif //GAO_HPP