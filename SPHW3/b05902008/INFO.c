/*b05902008王行健*/
#include<stdio.h>
#include<signal.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

int main(int argc, char **argv){
  kill(getppid(),SIGUSR1);
  return 0;
}
