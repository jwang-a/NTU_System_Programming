#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<errno.h>
#include<sys/time.h>
#include<dirent.h>
#include<signal.h>

#define ERR_EXIT(a) { perror(a); exit(1); }

typedef struct player_rank{
  int id;
  int score;
  int place;
}RANK;

int fillplays(int playernum, int *table){
  int cnt = 0;
  for(int i = 1;i<playernum-2;i++){
    for(int j = i+1;j<playernum-1;j++){
      for(int k = j+1;k<playernum;k++){
        for(int l = k+1;l<=playernum;l++){
          table[5*cnt] = i;
          table[5*cnt+1] = j;
          table[5*cnt+2] = k;
          table[5*cnt+3] = l;
          table[5*cnt+4] = 0;
          cnt++;
        }
      }
    }
  }
  return cnt;
}

void end_auction(int *busyplayer, int *busyhost, int *score, int *fd, int *cnt){
  char RESULT[1000];
  int player[4], rank[4];
  read(*fd,RESULT,1000);
  sscanf(RESULT,"%d%d%d%d%d%d%d%d",&player[0],&rank[0],&player[1],&rank[1],&player[2],&rank[2],&player[3],&rank[3]);
  for(int j = 0;j<4;j++){
    busyplayer[player[j]-1] = 0;
    score[player[j]-1]+=4-rank[j];
  }
  *busyhost = 0;
  (*cnt)++;
  return;
}

int compare(const void *a, const void *b){
  RANK A = *(RANK*)a;
  RANK B = *(RANK*)b;
  if(A.score<B.score){
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]){
  int hostnum;
  int playernum;
  int plays = 1,played = 0;
  pid_t host_pid[12];
  int player_id[20];
  int score[20];
  int matches[4845][5];
  int engaged[20];
  int busyplayer[20] = {0};
  int busyhost[12] = {0};
  int waitlist;
  int fd[12][4];
  int cnt;
  char host_id_temp[4];
  char pipemessage[20];
  int stlc;
  fd_set doset;
  struct timeval timeout;
  RANK result[20];
  if(argc!=3){
    fprintf(stderr,"usage: %s [host_num] [player_num]\n", argv[0]);
    exit(1);
  }
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  hostnum = atoi(argv[1]);
  playernum = atoi(argv[2]);
  if(hostnum<1 || hostnum>12){
    fprintf(stderr,"constraint: 1 <= host_num <= 12 not satisfied\n");
    exit(1);
  }
  if(playernum<4 || playernum>20){
    fprintf(stderr,"constaint: 4 <= player_num <= 20 not satisfied\n");
    exit(1);
  }
  for(int i = 0;i<playernum;i++){
    player_id[i] = i;
    score[i] = 0;
    engaged[i] = 0;
  }
  plays = playernum*(playernum-1)*(playernum-2)*(playernum-3)/4/3/2;
  if(fillplays(playernum,&matches[0][0])!=plays){
    fprintf(stderr,"number over matches wrong\n");
    exit(1);
  }
  for(int i = 0;i<hostnum;i++){
    if(pipe(&fd[i][0])==-1){
      ERR_EXIT("bidding_system pipe failed");
    }
    if(pipe(&fd[i][2])==-1){
      ERR_EXIT("bidding_system pipe failed");
    }
  }
  for(int i = 0;i<hostnum;i++){
    host_pid[i]=fork();
    if(host_pid[i]==0){
      close(fd[i][1]);
      close(fd[i][2]);
      for(int j = 0;j<hostnum;j++){
	if(j==i){
	  continue;
        }
	for(int k = 0;k<4;k++){
          close(fd[j][k]);
        }
      }
      dup2(fd[i][0], STDIN_FILENO);
      dup2(fd[i][3],STDOUT_FILENO);
      sprintf(host_id_temp,"%d",i+1);
      if(execl("host","./host",host_id_temp,(char*)0)==-1){
        ERR_EXIT("bidding_system exec failed");
      }
    }
    else{
      close(fd[i][0]);
      close(fd[i][3]);
    }
  }
  cnt = 0;
  waitlist = -1;
  while(plays!=cnt){
    FD_ZERO(&doset);
    for(int i = 0;i<hostnum;i++){
      FD_SET(fd[i][2],&doset);
    }
    if(select(FD_SETSIZE,&doset,NULL,NULL,&timeout)<0){
      ERR_EXIT("bidding_system select failed");
    }
    for(int i = 0;i<hostnum;i++){
      if(FD_ISSET(fd[i][2],&doset)){
        end_auction(busyplayer,&busyhost[i],score,&fd[i][2],&cnt);
      }
    }
    if(waitlist==-1){
      for(int i = 0;i<plays;i++){
        if(matches[i][4]==0 && busyplayer[matches[i][0]-1]==0 && busyplayer[matches[i][1]-1]==0 && busyplayer[matches[i][2]-1]==0 && busyplayer[matches[i][3]-1]==0){
          waitlist = i;
          break;
        }
      }
    }
    if(waitlist==-1){
      continue;
    }
    for(int i = 0;i<hostnum;i++){
      if(busyhost[i]==0){
        sprintf(pipemessage,"%d %d %d %d\n",matches[waitlist][0],matches[waitlist][1],matches[waitlist][2],matches[waitlist][3]);
        matches[waitlist][4] = 1;
        for(int j = 0;j<4;j++){
	  busyplayer[matches[waitlist][j]-1] = 1;
	}
	write(fd[i][1],pipemessage,strlen(pipemessage));
	waitlist = -1;
	busyhost[i] = 1;
	break;
      }
    }
  }
  for(int i = 0;i<hostnum;i++){
    sprintf(pipemessage,"-1 -1 -1 -1\n");
    write(fd[i][1],pipemessage,strlen(pipemessage));
    waitpid(host_pid[i],&stlc,0);
  }
  for(int i = 0;i<playernum;i++){
    result[i].id = i+1;
    result[i].score = score[i];
  }
  qsort(result,playernum,sizeof(RANK),compare);
  result[result[0].id-1].place = 1;
  for(int i = 1;i<playernum;i++){
    if(result[i].score==result[i-1].score){
      result[result[i].id-1].place = result[result[i-1].id-1].place;
    }
    else{
      result[result[i].id-1].place = i+1;
    }
  }
  for(int i = 0;i<playernum;i++){
    printf("%d %d\n",i+1,result[i].place);
  }
  return 0;
}
