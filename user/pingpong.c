#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int p1[2], p2[2];
    char c;
    pipe(p1);//p->c
    pipe(p2);//c->p
    if(fork()==0){
        close(p1[1]);
        close(p2[0]);
        read(p1[0], &c, 1);
        write(p2[1], &c, 1);
        printf("%d: received ping\n", getpid());
        close(p1[0]);
        close(p2[1]);
    }else{
        close(p1[0]);
        close(p2[1]);
        write(p1[1], (void*)'p', 1);
        wait(0);
        read(p2[0], &c, 1);
        printf("%d: received pong\n", getpid());
        close(p1[1]);
        close(p2[0]);
    }
    exit(0);
}