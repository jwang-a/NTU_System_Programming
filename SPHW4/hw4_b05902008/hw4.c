/*b05902008 王行健*/
#define _GNU_SOURCE
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<pthread.h>

#define TREE_SIZE 500
#define MAX_TREE_NUM 1000
#define MAX_THREAD_NUM 20

typedef struct tree{
  int feature;
  double threshold;
  int left,right;
}TREE;

int TREE_NUM;
int THREAD_NUM;
char train_file[100],test_file[100];
char buf[10000],*bufptr;
double train_dataset[26000][34];
double test_dataset[26000][34];
int train_cnt,test_cnt;
TREE forest[MAX_TREE_NUM][1100];
double correct;
double answer[26000];
pthread_t thread[MAX_TREE_NUM];
int index_[MAX_TREE_NUM];
int seed[MAX_TREE_NUM][TREE_SIZE];
char output[270000];

void TEST(int which){
  int predict = 0;
  int trav;
  for(int i = 0;i<TREE_NUM;i++){
    trav = 0;
    while(forest[i][trav].feature!=-1){
      (test_dataset[which][forest[i][trav].feature]<forest[i][trav].threshold)? (trav=forest[i][trav].left):(trav=forest[i][trav].right);
    }
    (forest[i][trav].threshold)? (predict++):(predict--);
  }
//printf("%d\n",predict);
  (predict>0)? (answer[which]=1):(answer[which]=0);
  return;
}

int CMP(const void *a,const void *b,void *iid){
  return(train_dataset[*((int*)a)][*((int*)iid)]>train_dataset[*((int*)b)][*((int*)iid)]);
}

void PLANT_TREE(int which,int size,int start,int *sample){
  if(size==1){
    forest[which][start].feature = -1;
    forest[which][start].threshold = train_dataset[sample[0]][33];
    return;
  }
  int o=0,z=0;
  for(int i = 0;i<size;i++){
    if(train_dataset[sample[i]][33])
      o = 1;
    else
      z = 1;
  }
  if(o+z!=2){
    forest[which][start].feature = -1;
    forest[which][start].threshold = train_dataset[sample[0]][33];
    return;
  }
  int best_feature,best_cut;
  double best_gini = 100,best_threshold;
  double down,up,gl,gr,gt;
  for(int i = 0;i<33;i++){
    down = size;
    up = 0;
    qsort_r(sample,size,sizeof(int),CMP,&i);
    for(int j = 0;j<size-1;j++){
      up+=train_dataset[sample[j]][33];
      down-=train_dataset[sample[j]][33];
      gl = 2*(up/(j+1)*(1-up/(j+1)));
      gr = 2*(down/(size-j-1)*(1-down/(size-j-1)));
      gt = gl+gr;
      if(gt<best_gini){
        best_gini = gt;
        best_cut = j;
        best_threshold = (train_dataset[sample[j]][i]);
        best_feature = i;
      }
    }
  }
  qsort_r(sample,size,sizeof(int),CMP,&best_feature);
  forest[which][start].feature = best_feature;
  forest[which][start].threshold = best_threshold;
  forest[which][start].left = start+1;
  forest[which][start].right = start+2*best_cut+2;
  PLANT_TREE(which,best_cut+1,forest[which][start].left,sample);
  PLANT_TREE(which,size-best_cut-1,forest[which][start].right,&sample[best_cut+1]);
  return;
}

void* TRAIN_INIT(void *arg){
  int which = *(int*)arg;
  int sample[TREE_SIZE];
  for(int i = 0;i<TREE_SIZE;i++)
    sample[i] = seed[which][i];
  PLANT_TREE(which,TREE_SIZE,0,sample);
  pthread_exit(NULL);
}

int main(int argc, char **argv){
  sscanf(argv[6],"%d",&TREE_NUM);
  sscanf(argv[8],"%d",&THREAD_NUM);
  for(int i = 0;i<TREE_NUM;i++)
    index_[i] = i;
////////////////GET_TRAIN_DATA//////////////////////
  strcpy(train_file,argv[2]);
  strcpy(&train_file[strlen(train_file)],"/training_data");
  FILE *train_fp = fopen(train_file,"r");
  while(fgets(buf,sizeof(buf),train_fp)!=NULL){
    bufptr = buf;
    for(int i = 0;i<34;i++){
      bufptr = strstr(bufptr," ");
      bufptr++;
      sscanf(bufptr,"%lf",&train_dataset[train_cnt][i]);
    }
    train_cnt++;
  }
  fclose(train_fp);
////////////////GET_TEST_DATA//////////////////////
  strcpy(test_file,argv[2]);
  strcpy(&test_file[strlen(test_file)],"/testing_data");
  FILE *test_fp = fopen(test_file,"r");
  while(fgets(buf,sizeof(buf),test_fp)!=NULL){
    bufptr = buf;
    for(int i = 0;i<33;i++){
      bufptr = strstr(bufptr," ");
      bufptr++;
      sscanf(bufptr,"%lf",&test_dataset[test_cnt][i]);
    }
    test_cnt++;
  }
  fclose(test_fp);
/////////////////////TRAIN/////////////////////////
  int ran = 0,cnt;
  while(ran<TREE_NUM){
    cnt = 0;
    for(int i = 0;i<THREAD_NUM;i++){
      for(int j = 0;j<TREE_SIZE;j++)
        seed[ran][j] = rand()%train_cnt;
      pthread_create(&thread[cnt],NULL,&TRAIN_INIT,&index_[ran]);
      ran++;
      cnt++;
      if(ran>=TREE_NUM)
        break;
    }
    for(int i = 0;i<cnt;i++)
      pthread_join(thread[i],NULL);
  }
/////////////////////TEST/////////////////////////
  for(int i = 0;i<test_cnt;i++)
    TEST(i);
  FILE *fp = fopen(argv[4], "w");
  fprintf(fp, "id,label\n");
  for(int i = 0;i<test_cnt;i++)
    fprintf(fp,"%d,%d\n",i,(int)answer[i]);
  fclose(fp);
  return 0;
}
