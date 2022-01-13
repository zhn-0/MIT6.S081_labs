#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[])
{
    char buf[512], *p;
    p=buf;
    for(int i=0;i<argc-1;i++){
        argv[i]=argv[i+1];
    }
    argv[argc-1]=buf;
    while(read(0, p, 1)>0){
        if(*p=='\n' || *p=='\0'){
            *p='\0';
            if(fork()==0){
                exec(argv[0], argv);
            }else wait(0);
            p=buf;
        }else p++;
    }

    exit(0);
}