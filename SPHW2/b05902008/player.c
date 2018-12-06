/*b05902008 王行健*/
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

void interpret(char *message, int *money,int *win, int round){
  int cur_money[4];
  sscanf(message,"%d%d%d%d",&cur_money[0],&cur_money[1],&cur_money[2],&cur_money[3]);
  for(int i = 0;i<4;i++){
    if(cur_money[i]!=money[i]+1000){
      win[i]++;
    }
    money[i] = cur_money[i];
  }
  return;
}

int main(int argc, char *argv[]){
  int fd[2];
  int flag;
  int money[4] = {0};
  int win[4] = {0};
  char FIFOname[2][20];
  char FIFOmessage[2][60];
  if(argc!=4){
    fprintf(stderr,"usage: %s [host_id] [player_index] [random_key]\n",argv[0]);
    exit(1);
  }
  sprintf(FIFOname[0],"host%s.FIFO",argv[1]);
  sprintf(FIFOname[1],"host%s_%s.FIFO",argv[1],argv[2]);
  fd[0] = open(FIFOname[0],O_RDWR);
  fd[1] = open(FIFOname[1],O_RDWR);
  for(int i = 0;i<10;i++){
    flag= 0;
    read(fd[1],FIFOmessage[1],60);
    interpret(FIFOmessage[1],money,win,i);
    if(i%4==argv[2][0]-'A')
      flag = 1;
    sprintf(FIFOmessage[0],"%s %s %d\n",argv[2],argv[3],1000*flag);
    write(fd[0],FIFOmessage[0],strlen(FIFOmessage[0]));
    fdatasync(fd[0]);
  }
  return 0;
}
