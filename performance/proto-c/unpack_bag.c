#include <stdio.h>
#include <stdlib.h>
#include "bag.pb-c.h"

#define MAX_BUF_SIZE (1024*1024)
#define MAX_HANDLE_TIMES (1000000)
#define SERIAL_FILE "./bag_info"

static int print_bag(BagAll *pbag);


int main(int argc , char **argv)
{
  BagAll *pbag = NULL;
  int ret = 0;
  int total_len = 0;
  char *buff = calloc(1 , MAX_BUF_SIZE);
  struct timeval tv;
  int i = 0;
  FILE *fp = NULL;

  if(!buff)
  {
    printf("malloc buff failed!\n");
    return -1;
  }
  
  fp = fopen(SERIAL_FILE , "r");
  if(!fp)
  {
    printf("open %s failed!\n" , SERIAL_FILE);
  	return 0;
  }

  //read buff
  ret = fread(buff , 1 , MAX_BUF_SIZE , fp);
  if(ret >= MAX_BUF_SIZE)
  {
    printf("overflow!\n");
    return -1;
  }
  printf("read %d bytes!\n" , ret);

  //unpack
  gettimeofday(&tv , NULL);
  printf("starts:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);

  for(i=0; i<MAX_HANDLE_TIMES; i++)
  {
    pbag = bag_all__unpack(NULL, ret, buff);
    if(!pbag)
    {
      printf("unpack failed!\n");
      return -1;
    }

    //print_bag(pbag);
    bag_all__free_unpacked(pbag , NULL);
  }
  printf("unpack success!\n");
  

  gettimeofday(&tv , NULL);
  printf("end:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);
  return 0;
}

static int print_bag(BagAll *pbag)
{
  ItemInfo *pInfo = NULL;
  int i = 0;
  if(!pbag)
    return -1;

  printf("name:%s money:%ld diamond:%ld gold:%d\n" , pbag->attr->name , pbag->attr->money , 
    pbag->attr->diamond , pbag->attr->gold);

  printf("expend_items type:%d count:%d\n" , pbag->expend_items->type , pbag->expend_items->n_list);
  for(i=0; i < pbag->expend_items->n_list; i++)
  {
    pInfo = pbag->expend_items->list[i];
    printf(">[%d] res:%ld inst:%ld count:%d\n", pInfo->grid , pInfo->res_id , pInfo->instid , 
      pInfo->count);
  }

  return 0;
}
