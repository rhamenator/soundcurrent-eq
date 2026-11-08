// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifdef __linux__
#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>
namespace soundcurrent {
// Construct in the parent before fork. The modifier uses only system calls;
// the parent may already have exited before PR_SET_PDEATHSIG is installed.
inline auto terminateWithParent() {
    const auto expectedParent=::getpid();
    return [expectedParent] {
        if(::prctl(PR_SET_PDEATHSIG,SIGTERM)!=0 || ::getppid()!=expectedParent)
            ::_exit(127);
    };
}
}
#endif
