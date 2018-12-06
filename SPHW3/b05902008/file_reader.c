/*b05902008王行健*/
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<signal.h>
#include<dirent.h>
#include<limits.h>
#include<ctype.h>
#include <time.h>
#include <sys/mman.h>
char filename[NAME_MAX];

typedef struct {
    char c_time_string[100];
} TimeInfo;

void END(char name[]){
    int fd,i;
    time_t current_time;
    char c_time_string[100],buf[100];
    TimeInfo *p_map;
    const char  *file ="time_log";
    fd = open(file, O_RDWR | O_TRUNC | O_CREAT, 0777); 
    if(fd<0){
        perror("open");
        exit(-1);
    }
    lseek(fd,sizeof(TimeInfo),SEEK_SET);
    write(fd,"",1);
    p_map = (TimeInfo*) mmap(0, sizeof(TimeInfo), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    current_time = time(NULL);
    strcpy(buf, ctime(&current_time));
    buf[strlen(buf)-1] = '\0';
    sprintf(c_time_string,"Last Exit CGI: %s, CGIname:%s, Filename:%s\n",buf,name,filename);
    memcpy(p_map->c_time_string, &c_time_string , sizeof(c_time_string));
    munmap(p_map, sizeof(TimeInfo));
}

int main(int argc, char **argv){
  int length;
  char buffer[10000];
  read(STDIN_FILENO,filename,sizeof(filename));
  length = strlen(filename);
  for(int i = 0;i<length;i++){
    if(!isdigit(filename[i])&&!isalpha(filename[i])&&filename[i]!='_'){
      sprintf(buffer,"400 Bad Request\nONLY 'a'~'z' 'A'~'Z' '0'~'9' '_' ALLOWED IN NAME");
      length = strlen(buffer);
      fwrite(buffer,sizeof(char),length,stderr);
      END(argv[0]);
      return 2;
    }
  }
  if(access(filename,F_OK)==-1){
    sprintf(buffer,"404 Not Found\n%s DOES NOT EXIST",filename);
    length = strlen(buffer);
    fwrite(buffer,sizeof(char),length,stderr);
    END(argv[0]);
    return 1;
  }
  FILE *fp = fopen(filename,"r");
  do{
    length = fread(buffer,sizeof(char),1000,fp);
    fwrite(buffer,sizeof(char),length,stdout);
  }while(length==1000);
  END(argv[0]);
  return 0;
}
