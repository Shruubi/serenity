#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>
#include <Kernel/Syscall.h>

extern "C" {

int kill(pid_t pid, int sig)
{
    int rc = syscall(SC_kill, pid, sig);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

int killpg(int pgrp, int sig)
{
    int rc = syscall(SC_killpg, pgrp, sig);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

int raise(int sig)
{
    // FIXME: Support multi-threaded programs.
    return kill(getpid(), sig);
}

sighandler_t signal(int signum, sighandler_t handler)
{
    struct sigaction new_act;
    struct sigaction old_act;
    new_act.sa_handler = handler;
    new_act.sa_flags = 0;
    new_act.sa_mask = 0;
    new_act.sa_restorer = nullptr;
    int rc = sigaction(signum, &new_act, &old_act);
    if (rc < 0)
        return SIG_ERR;
    return old_act.sa_handler;
}

int sigaction(int signum, const struct sigaction* act, struct sigaction* old_act)
{
    int rc = syscall(SC_sigaction, signum, act, old_act);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

int sigemptyset(sigset_t* set)
{
    *set = 0;
    return 0;
}

int sigfillset(sigset_t* set)
{
    *set = 0xffffffff;
    return 0;
}

int sigaddset(sigset_t* set, int sig)
{
    if (sig < 1 || sig > 32) {
        errno = EINVAL;
        return -1;
    }
    *set |= 1 << (sig);
    return 0;
}

int sigdelset(sigset_t* set, int sig)
{
    if (sig < 1 || sig > 32) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1 << (sig));
    return 0;
}

int sigismember(const sigset_t* set, int sig)
{
    if (sig < 1 || sig > 32) {
        errno = EINVAL;
        return -1;
    }
    if (*set & (1 << (sig)))
        return 1;
    return 0;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* old_set)
{
    int rc = syscall(SC_sigprocmask, how, set, old_set);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

int sigpending(sigset_t* set)
{
    int rc = syscall(SC_sigpending, set);
    __RETURN_WITH_ERRNO(rc, rc, -1);
}

const char* sys_siglist[NSIG] = {
    "Invalid signal number",
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trap",
    "Aborted",
    "Bus error",
    "FP exception",
    "Killed",
    "User signal 1",
    "Segmentation violation",
    "User signal 2",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
    "Stack fault",
    "Child exited",
    "Continued",
    "Stopped (signal)",
    "Stopped",
    "Stopped (tty input)",
    "Stopped (tty output)",
    "Urgent I/O condition)",
    "CPU limit exceeded",
    "File size limit exceeded",
    "Virtual timer expired",
    "Profiling timer expired",
    "Window changed",
    "I/O possible",
    "Power failure",
    "Bad system call",
};

int sigsetjmp(jmp_buf env, int savesigs)
{
    if (savesigs) {
        int rc = sigprocmask(0, nullptr, &env->saved_signal_mask);
        assert(rc == 0);
        env->did_save_signal_mask = true;
    } else {
        env->did_save_signal_mask = false;
    }
    return setjmp(env);
}
void siglongjmp(jmp_buf env, int val)
{
    if (env->did_save_signal_mask) {
        int rc = sigprocmask(SIG_SETMASK, &env->saved_signal_mask, nullptr);
        assert(rc == 0);
    }
    longjmp(env, val);
}

}
