// SPDX-License-Identifier: GPL-3.0-only
#include "linux_child_lifetime.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <poll.h>
#include <thread>
#include <cerrno>
void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
void readBounded(int fd,void *data,std::size_t size){
    pollfd p{fd,POLLIN,0};require(::poll(&p,1,2000)>0,"Test child did not respond");
    require(::read(fd,data,size)==static_cast<ssize_t>(size),"Short test pipe read");
}
void exercise(bool parentExitsFirst){
    int announced[2],release[2],installed[2];
    require(::pipe(announced)==0 && ::pipe(release)==0 && ::pipe(installed)==0,"pipe");
    pid_t grandchild=-1;bool parentReaped=false;
    const auto parent=::fork();require(parent>=0,"fork parent");
    if(parent==0){
        const auto modifier=soundcurrent::terminateWithParent();
        const auto child=::fork();if(child<0)::_exit(120);
        if(child==0){
            if(parentExitsFirst){char go;if(::read(release[0],&go,1)!=1)::_exit(121);}
            modifier();
            const char ready='R';if(::write(installed[1],&ready,1)!=1)::_exit(122);
            for(;;)::pause();
        }
        if(::write(announced[1],&child,sizeof(child))!=sizeof(child))::_exit(123);
        if(parentExitsFirst)::_exit(0);
        for(;;)::pause();
    }
    try{
        readBounded(announced[0],&grandchild,sizeof(grandchild));require(grandchild>0,"invalid child PID");
        int status=0;
        if(parentExitsFirst){
            parentReaped=::waitpid(parent,&status,0)==parent;
            require(parentReaped && WIFEXITED(status) && WEXITSTATUS(status)==0,"early parent exit");
            const char go='G';require(::write(release[1],&go,1)==1,"release child");
        }else{
            char ready;readBounded(installed[0],&ready,1);require(ready=='R',"modifier not installed");
            require(::kill(parent,SIGKILL)==0,"terminate test parent");
            parentReaped=::waitpid(parent,&status,0)==parent;require(parentReaped,"reap test parent");
        }
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        pid_t done=0;
        do{done=::waitpid(grandchild,&status,WNOHANG);if(done!=0)break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}while(std::chrono::steady_clock::now()<deadline);
        require(done==grandchild,"orphan helper survived parent exit");grandchild=-1;
        require(parentExitsFirst?(WIFEXITED(status) && WEXITSTATUS(status)==127):(WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL),"wrong child termination");
    }catch(...){
        // Only PIDs obtained from our own forks/pipes are eligible for cleanup.
        int status;if(!parentReaped){::kill(parent,SIGKILL);::waitpid(parent,&status,0);}
        if(grandchild>0){::kill(grandchild,SIGKILL);::waitpid(grandchild,&status,0);}throw;
    }
    for(auto fd:{announced[0],announced[1],release[0],release[1],installed[0],installed[1]})::close(fd);
}
int main(){try{
    require(::prctl(PR_SET_CHILD_SUBREAPER,1)==0,"test subreaper unavailable");
    for(int i=0;i<20;++i){exercise(true);exercise(false);}
    std::cout<<"PASS: helper exits when parent dies before or after signal installation; 40 test-owned process trees, no audio capture\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
