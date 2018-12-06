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

int main(){
  char filename[NAME_MAX];
  int length;
  read(STDIN_FILENO,filename,sizeof(filename));
  if(strcmp(filename,"NONE\0")!=0){
    puts("FILENAME MUST BE NONE FOR ME!!!");
    return 1;
  }
  sleep(10);
  fwrite("HAD A GOOD SLEEP ^_^\0",sizeof(char),20,stdout);
  return 0;
}
