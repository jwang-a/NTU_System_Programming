#include<stdio.h>
#include<string.h>
#include<stdlib.h>
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

typedef struct identity{
  int bid;
  int key;
  int uniq;
}ID;

void unique(ID *player){
  for(int i = 0;i<4;i++){
    for(int j = 1+1;j<4;j++){
      if(player[i].bid==player[j].bid){
        player[i].uniq = 1;
        player[j].uniq = 1;
      }
    }
  }
  return;
}

int winner(ID *player){
  for(int i = 0;i<4;i++){
    if(player[i].uniq!=0){
      player[i].bid = -1;
      continue;
    }
    for(int j = i+1;j<4;j++){
      if(player[j].uniq!=0){
        player[j].bid = -1;
        continue;
      }
      if(player[i].bid>player[j].bid){
        player[j].bid = -1;
      }
      else{
        player[i].bid = -1;
      }
    }
  }
  for(int i = 0;i<4;i++){
    if(player[i].bid!=-1){
      for(int j = i+1;j<4;j++){
        player[j].bid = 0;
      }
      return i;
    }
    else{
      player[i].bid = 0;
    }
  }
  return -1;
}

int compare(const void *a, const void *b){
  ID *A=(ID*)a;
  ID *B=(ID*)b;
  if(A->bid<B->bid){
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]){
  int hostid;
  int cnt;
  int keytemp;
  int bidtemp;
  int which;
  char FIFOname[5][20];
  int fd[5];
  char hash_temp[8];
  int hash[4];
  int collide;
  char player_index[4][4] = {0};
  pid_t player_pid[4];
  int player_id[4];
  int win[4];
  int win_temp;
  int money[4];
  fd_set doset;
  struct timeval timeout;
  char FIFOmessage[2][1000];
  ID player[4];
  char index_temp[4];
  int stlc;
  if(argc!=2){
    fprintf(stderr,"usage: %s [host_id]\n",argv[0]);
    exit(1);
  }
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  hostid = atoi(argv[1]);
  sprintf(FIFOname[0],"host%d.FIFO",hostid);
  sprintf(FIFOname[1],"host%d_A.FIFO",hostid);
  sprintf(FIFOname[2],"host%d_B.FIFO",hostid);
  sprintf(FIFOname[3],"host%d_C.FIFO",hostid);
  sprintf(FIFOname[4],"host%d_D.FIFO",hostid);
  for(int i = 0;i<4;i++){
    player_index[i][0] = 'A'+i;
    player_index[i][1] = '\0';
  }
  while(1){
    FD_ZERO(&doset);
    FD_SET(0,&doset);
    if(select(FD_SETSIZE,&doset,NULL,NULL,&timeout)<0){
      ERR_EXIT("host select failed");
    }
    if(FD_ISSET(0,&doset)==0){
      continue;
    }
    scanf("%d%d%d%d",&player_id[0],&player_id[1],&player_id[2],&player_id[3]);
    if(-1==player_id[0]){
      break;
    }
    mkfifo(FIFOname[0],00600);
    mkfifo(FIFOname[1],00600);
    mkfifo(FIFOname[2],00600);
    mkfifo(FIFOname[3],00600);
    mkfifo(FIFOname[4],00600);
    for(int i = 0;i<4;i++){
      collide = 1;
      while(collide){
        collide = 0;
        hash[i] = rand()%65536;
        for(int j = 0;j<i;j++){
          if(hash[i]==hash[j]){
            collide = 1;
            break;
          }
        }
      }
    }
    for(int i = 0;i<4;i++){
      if((player_pid[i]=fork())<0){
        ERR_EXIT("host fork failed");
      }
      else if(player_pid[i]==0){
        sprintf(hash_temp,"%d",hash[i]);
        if(execl("player","./player",argv[1],player_index[i],hash_temp,(char*)0)<0){
          ERR_EXIT("host exec failed");
        }
      }
    }
    fd[0] = open(FIFOname[0],O_RDWR);
    for(int i = 1;i<=4;i++){
      fd[i] = open(FIFOname[i],O_RDWR);
    }
    for(int i = 0;i<4;i++){
      money[i] = 0;
      win[i] = 0;
      player[i].bid = 0;
    }
    for(int i = 0;i<10;i++){
      for(int j = 0;j<4;j++){
        money[j]+=1000-player[j].bid;
      }
      sprintf(FIFOmessage[0],"%d %d %d %d\n",money[0],money[1],money[2],money[3]);
      for(int j = 1;j<=4;j++){
        if(write(fd[j],FIFOmessage[0],strlen(FIFOmessage[0]))==-1){
          ERR_EXIT("host write failed");
        }
        fdatasync(fd[j]);
      }
      for(int j = 0;j<4;j++){
        player[j].key = -1;
      }
      cnt = 0;
      REVISE:;
      FD_ZERO(&doset);
      FD_SET(fd[0],&doset);
      if(select(FD_SETSIZE,&doset,NULL,NULL,&timeout)<0){
        ERR_EXIT("host select failed");
      }
      if(FD_ISSET(fd[0],&doset)==0){
        goto REVISE;
      }
      memset(FIFOmessage[1],0,sizeof(char)*1000);
      read(fd[0],FIFOmessage[1],1000);
      for(int j = 0;FIFOmessage[1][j]!='\0';j++){
        if(j==0 || FIFOmessage[1][j-1]=='\n'){
          sscanf(&FIFOmessage[1][j],"%s%d%d",index_temp,&(keytemp),&(bidtemp));
	  which = index_temp[0]-'A';
	  if(player[which].key==-1){
            cnt++;
	    player[which].key = keytemp;
	    player[which].bid = bidtemp;
	  }
        }
      }
      if(cnt!=4){
        goto REVISE;
      }
      for(int j = 0;j<4;j++){
        if(hash[j]!=player[j].key){
          fprintf(stderr,"intruder!\n");
          exit(1);
        }
        player[j].uniq = 0;
      }
      unique(player);
      win_temp = winner(player);
      if(win_temp!=-1){
        win[win_temp]+=1;
      }
    }
    for(int i = 0;i<4;i++){
      player[i].bid = win[i];
      player[i].key = i;
    }
    qsort(player,4,sizeof(ID),compare);
    player[player[0].key].uniq = 1;
    for(int i = 1;i<4;i++){
      if(player[i].bid==player[i-1].bid){
        player[player[i].key].uniq = player[player[i-1].key].uniq;
      }
      else{
        player[player[i].key].uniq = i+1;
      }
    }
    sprintf(FIFOmessage[0],"%d %d\n%d %d\n%d %d\n%d %d\n",player_id[0],player[0].uniq,player_id[1],player[1].uniq,player_id[2],player[2].uniq,player_id[3],player[3].uniq);
    write(STDOUT_FILENO,FIFOmessage[0],strlen(FIFOmessage[0]));
    for(int i = 0;i<4;i++){
      waitpid(player_pid[i],&stlc,0);
    }
    for(int i = 0;i<5;i++){
      close(fd[i]);
    }
  }
  for(int i = 0;i<5;i++){
    unlink(FIFOname[i]);
  }
  return 0;
}
