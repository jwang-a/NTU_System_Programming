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
#include<time.h>

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


int strategy(int *money, int *win, int me, int rounds_left){
  int notice_num = 0;
  int notice[3];
  int max = 0;
  int exist[3];
  int decide;
  for(int i = 0;i<4;i++){
    if(i==me){
      continue;
    }
    if(rounds_left+win[i]>=win[me]){
      notice[notice_num] = i;
      notice_num++;
      if(money[i]>max){
        max = money[i];
      }
    }
  }
  if(notice_num==0){
    return(money[me]);
  }
  if(money[me]>max){
    return(max+1);
  }
  if(money[me]>3400)
    decide = 3400;
  else
    decide = money[me];
  srand(clock());
  return(decide-rand()%5);
}


int main(int argc, char *argv[]){
  int fd[2];
  int flag;
  int money[4] = {0};
  int win[4] = {0};
  int me;
  char FIFOname[2][20];
  char FIFOmessage[2][60];
  if(argc!=4){
    fprintf(stderr,"usage: %s [host_id] [player_index] [random_key]\n",argv[0]);
    exit(1);
  }
  me = argv[2][0]-'A';
  sprintf(FIFOname[0],"host%s.FIFO",argv[1]);
  sprintf(FIFOname[1],"host%s_%s.FIFO",argv[1],argv[2]);
  fd[0] = open(FIFOname[0],O_RDWR);
  fd[1] = open(FIFOname[1],O_RDWR);
  for(int i = 0;i<10;i++){
    flag= 0;
    read(fd[1],FIFOmessage[1],60);
    interpret(FIFOmessage[1],money,win,i);
    flag = strategy(money,win,me,10-i);
    sprintf(FIFOmessage[0],"%s %s %d\n",argv[2],argv[3],flag);
    write(fd[0],FIFOmessage[0],strlen(FIFOmessage[0]));
    fdatasync(fd[0]);
  }
  return 0;
}
