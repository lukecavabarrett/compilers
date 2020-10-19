#include <util/bash_executor.h>

namespace util {

pid_t bash_executor::make_exc(const char *command, const int IN_FILENO, const int OUT_FILENO, const int ERR_FILENO, const int DIR_FD) {
  pid_t pid = fork();
  if (pid == 0) {

    //chdir
    if (DIR_FD) {
      if (fchdir(DIR_FD) == -1) {
        std::perror("subprocess: fchdir() failed");
        exit(-1);
      }
    }

    //dupping file descriptors
    if (IN_FILENO != STDIN_FILENO) {
      if (dup2(IN_FILENO, STDIN_FILENO) == -1) {
        std::perror("subprocess: dup2() failed");
        exit(-1);
      }
    }
    if (OUT_FILENO != STDOUT_FILENO) {
      if (dup2(OUT_FILENO, STDOUT_FILENO) == -1) {
        std::perror("subprocess: dup2() failed");
        exit(-1);
      }
    }
    if (ERR_FILENO != STDERR_FILENO) {
      if (dup2(ERR_FILENO, STDERR_FILENO) == -1) {
        std::perror("subprocess: dup2() failed");
        exit(-1);
      }
    }

    //execlp
    if (execlp("bash", "bash", "-c", command, nullptr) == -1) {
      std::perror("subprocess: execlp() failed");
      std::cerr << "errno: " << errno << std::endl;
      exit(-1);
    }
  }
  return pid;
}
void bash_executor::poll_state_(int opts) {
  assert(pid_ != 0);
  if (w_ == 1)return;
  assert(w_ != 1);
  w_ = waitpid(pid_, &status_, opts);
  if (w_ == -1) {
    //IDEA: maybe not fail
    std::perror("bash_executor: waitpid() failed");
    std::cerr << "errno: " << errno << std::endl;
    exit(-1);
  }
  if (w_ == pid_ && (WIFEXITED(status_) || WIFSIGNALED(status_) || WCOREDUMP(status_)))w_ = 1;
}
bash_executor::~bash_executor() {
  if constexpr (BASH_EXEC_KILL) {
    kill();
  } else {
    release();
  }
}
[[maybe_unused]] pid_t bash_executor::pid() const noexcept { return pid_; }
void bash_executor::release() { pid_ = 0; }
bool bash_executor::running() {
  poll_state_();
  return w_ != 1;
}
}