#ifndef HYDRA_LIBS_UTIL_BASH_EXECUTOR_H_
#define HYDRA_LIBS_UTIL_BASH_EXECUTOR_H_
#include <mutex>
#include <thread>
#include <iostream>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <zconf.h>
#include <wait.h>
#include <string_view>
#include <fcntl.h>
#include <cassert>
#include <csignal>
namespace util {

class bash_executor {
#define BASH_EXEC_KILL 0
  /*
   WARNING: The correctness of this class is up to PID recycle. Might fail on some system.
    Invariants:
     if pid==0, the bash_executor is not assigned to anything (we'll say its dangling), and status_ can be any value.
     if pid!=0, the bash_executor is responsible of the process, and status_ should contain the last call to wait_pid (one should be made when creating the process). w_ contains the last return value of wait_pid, or 1 if the execution terminated.
     Duties: if not w_==1, bash_executor needs either to kill or wait the process.
  */
 private:
  static pid_t make_exc(const char *command, const int IN_FILENO = STDIN_FILENO, const int OUT_FILENO = STDOUT_FILENO, const int ERR_FILENO = STDERR_FILENO, const int DIR_FD = 0);
  void poll_state_(int opts = WNOHANG);
 public:
  static inline int NULL_FILENO = open("/dev/null", O_RDONLY);
  bash_executor() : pid_(0), status_(0), w_(0) {}
//  bash_executor(std::string_view command, const int IN_FILENO = STDIN_FILENO, const int OUT_FILENO = STDOUT_FILENO, const int ERR_FILENO = STDERR_FILENO) : pid_(make_exc(std::string(command).c_str(), IN_FILENO, OUT_FILENO, ERR_FILENO)),
//                                                                                                                                                            status_(0),
//                                                                                                                                                            w_(0) {}
  bash_executor(const char *command) : pid_(make_exc(command)), status_(0), w_(0) {}
  bash_executor(const char *command, const int IN_FILENO = STDIN_FILENO, const int OUT_FILENO = STDOUT_FILENO, const int ERR_FILENO = STDERR_FILENO) : pid_(make_exc(command, IN_FILENO, OUT_FILENO, ERR_FILENO)), status_(0), w_(0) {}
  bash_executor(const char *command,const int DIR_FD = 0) : pid_(make_exc(command, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO)), status_(0), w_(0) {}
  bash_executor(const char *command, const int IN_FILENO = STDIN_FILENO, const int OUT_FILENO = STDOUT_FILENO, const int ERR_FILENO = STDERR_FILENO, const int DIR_FD = 0) : pid_(make_exc(command, IN_FILENO, OUT_FILENO, ERR_FILENO)), status_(0), w_(0) {}
  bash_executor(const bash_executor &) = delete;
  bash_executor(bash_executor &&o) : pid_(o.pid_), status_(o.status_) { o.pid_ = 0; };
  ~bash_executor();
  explicit operator bool() const noexcept { return bool(pid_); }
  [[maybe_unused]] [[nodiscard]] pid_t pid() const noexcept ;
  void release();
  [[maybe_unused]] [[nodiscard]] bool running();
  bool exited() {
    poll_state_();
    return w_ == 1 && WIFEXITED(status_);
  }
  int exit_code() { //throws if didn't exit
    assert(w_ == 1);
    assert(WIFEXITED(status_));
    return WEXITSTATUS(status_);
  }
  bool terminated() {
    poll_state_();
    return w_ == 1 && WIFSIGNALED(status_);
  }
  int terminating_signal() { //throws if wasn't terminated
    assert(w_ == 1);
    assert(WIFSIGNALED(status_));
    return WTERMSIG(status_);
  }
  void kill_thread(int sig = SIGTERM, int escalate_sig = SIGKILL, int timeout = 2000000) {
    if (pid_ == 0) {
      std::perror("bash_executor: kill() failed: no process is currently managed");
      exit(-1);
    }
    if (w_ == 1) {
      std::perror("bash_executor: kill() redundant: already killed");
      return;
    }

    if (::kill(pid_, sig) == -1) {
      std::perror("bash_executor: kill() failed: could not kill process (ESRCH is 3)");
      std::cerr << "errno: " << errno << std::endl;
      return;
    }

    auto t1 = std::chrono::system_clock::now();

    std::thread([timeout, escalate_sig](pid_t pid) {
      usleep(timeout);
      if (::kill(pid, escalate_sig) == -1) {
        if (errno != 3) {
          std::perror("bash_executor: kill() failed: could not escalate-kill process");
          exit(-1);
        } else {
          std::perror("bash_executor: kill() failed: could not escalate-kill process, it yielded");
        }
      } else {
        std::cerr << pid << " killed" << std::endl;
      }
    }, pid_).detach();
    assert(w_ != 1);
    // and now wait
    //exactly like poll_state_(0), but we might need to generate an artificial state
    if ((w_ = waitpid(pid_, &status_, 0)) == -1) {
      std::perror("bash_executor: artificial waitpid() failed");
      std::cerr << "errno: " << errno << std::endl;
      status_ = escalate_sig & 0x7f;
      w_ = pid_;
    }
    auto t2 = std::chrono::system_clock::now();
    std::cerr << "thread ended in " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "ms" << std::endl;

    if (w_ == pid_ && (WIFEXITED(status_) || WIFSIGNALED(status_) || WCOREDUMP(status_)))w_ = 1;

  }
  void kill_polled(int sig = SIGTERM, int escalate_sig = SIGKILL, int timeout = 2000000) {
    if (pid_ == 0) {
      std::perror("bash_executor: kill() failed: no process is currently managed");
      exit(-1);
    }
    if (w_ == 1) {
      std::perror("bash_executor: kill() redundant: already killed");
      return;
    }

    if (::kill(pid_, sig) == -1) {
      std::perror("bash_executor: kill() failed: could not kill process (ESRCH is 3)");
      std::cerr << "errno: " << errno << std::endl;
      return;
    }
    auto t1 = std::chrono::system_clock::now();

    if (timeout)
      for (int i = 0; i < 100 && w_ != 1; i++) {
        usleep(timeout / 100);
        poll_state_();
      }
    auto t2 = std::chrono::system_clock::now();
    std::cerr << "polling ended in " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "ms" << std::endl;
    if (w_ == 1)return;

    if (::kill(pid_, escalate_sig) == -1) {
      if (errno != 3) {
        std::perror("bash_executor: kill() failed: could not escalate-kill process");
        exit(-1);
      } else {
        std::perror("bash_executor: kill() failed: could not escalate-kill process, it yielded");
      }
    } else {
      std::cerr << pid_ << " killed" << std::endl;
    }

    // and now wait
    //exactly like poll_state_(0), but we might need to generate an artificial state
    if ((w_ = waitpid(pid_, &status_, 0)) == -1) {
      std::perror("bash_executor: artificial waitpid() failed");
      std::cerr << "errno: " << errno << std::endl;
      status_ = escalate_sig & 0x7f;
      w_ = pid_;
    }

    if (w_ == pid_ && (WIFEXITED(status_) || WIFSIGNALED(status_) || WCOREDUMP(status_)))w_ = 1;

  }
  void wait() {
    poll_state_(0);
  }
  void kill(int sig = SIGTERM, int escalate_sig = SIGKILL, int timeout = 2000000) {
    return kill_polled(sig, escalate_sig, timeout);
  }
 private:
  pid_t pid_;
  int status_, w_;
};

}

#endif //HYDRA_LIBS_UTIL_BASH_EXECUTOR_H_
