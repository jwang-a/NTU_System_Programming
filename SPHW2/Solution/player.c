#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/select.h>
#include<signal.h>
#include<sys/wait.h>
#include<dirent.h>
#include<sys/time.h>
#include<fcntl.h>

#define ERR_EXIT(a) { perror(a); exit(1); }

int main(int argc, char *argv[]){
  fd_set doset;
  int fd[2];
  int flag;
  char FIFOname[2][20];
  char FIFOmessage[2][60];
  struct timeval timeout;
  if(argc!=4){
    fprintf(stderr,"usage: %s [host_id] [player_index] [random_key]\n",argv[0]);
    exit(1);
  }
  sprintf(FIFOname[0],"host%s.FIFO",argv[1]);
  sprintf(FIFOname[1],"host%s_%s.FIFO",argv[1],argv[2]);
  fd[0] = open(FIFOname[0],O_RDWR);
  fd[1] = open(FIFOname[1],O_RDWR);
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  for(int i = 0;i<10;i++){
    flag= 0;
    REVISE:;
    FD_ZERO(&doset);
    FD_SET(fd[1],&doset);
    select(FD_SETSIZE,&doset,NULL,NULL,&timeout);
    if(FD_ISSET(fd[1],&doset)==0)
      goto REVISE;
    read(fd[1],FIFOmessage[1],60);
    if(i%4==argv[2][0]-'A')
      flag = 1;
    sprintf(FIFOmessage[0],"%s %s %d\n",argv[2],argv[3],1000*flag);
    write(fd[0],FIFOmessage[0],strlen(FIFOmessage[0]));
    fdatasync(fd[0]);
  }
  return 0;
}
