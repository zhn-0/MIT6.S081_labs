#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void func(int *p){
    int p1[2], base, t;
    pipe(p1);
    read(p[0], &base, 4);
    if(base==0)return;
    fprintf(1, "prime %d\n", base);
    if(fork()!=0){
        close(p1[0]);
        while(read(p[0], &t, 4)){
            if(t%base!=0)
                write(p1[1], (void*)&t, 4);
        }
        close(p1[1]);
    }else{
        close(p1[1]);
        func(p1);
        close(p1[0]);
    }
}

int main(int argc, char* argv[])
{
    int p[2];
    pipe(p);
    if(fork()!=0){
        close(p[0]);
        for(int i=2;i<=35;i++)
            write(p[1], (void*)&i, 4);
        close(p[1]);
    }else{
        close(p[1]);
        func(p);
        close(p[0]);
    }
    wait(0);
    exit(0);
}