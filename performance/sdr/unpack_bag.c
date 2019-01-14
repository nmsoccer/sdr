#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sdr/sdr.h>
#include "bag.h"

#define SDR_FILE "./bag.sdr"
#define MAX_BUFF_SIZE (1024*1024)
#define MAX_HANDLE_TIMES  (1000000)
#define SERIAL_FILE "./bag_info"

static int print_bag(bag_all_t *pbag);

int main(int argc , char **argv)
{
  sdr_data_res_t *pres = NULL;
  bag_all_t mybag;
  int i = 0;
  int ret = -1;
  struct timeval tv;
  char *buff = NULL;
  int len = -1;
  FILE *fp = NULL;

  //fp
  fp = fopen(SERIAL_FILE , "r");
  if(!fp)
  {
    printf("open %s failed!\n" , SERIAL_FILE);
  	return 0;
  }

  //load sdr
  pres = sdr_load_bin(SDR_FILE, NULL);
  if(!pres)
  {
    printf("sdr_load_bin of %s failed!\n" , SDR_FILE);
    return -1;
  }

  //buff
  buff = calloc(1 , MAX_BUFF_SIZE);
  if(!buff)
  {
    printf("malloc buff failed!\n");
    return -1;
  }

  //read buff
  ret = fread(buff , 1 , MAX_BUFF_SIZE , fp);
  if(ret >= MAX_BUFF_SIZE)
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
    len = sdr_unpack(pres, (char *)&mybag, buff, "bag_all", 0 , NULL);
    if(len <= 0)
    {
      printf("unpack failed! i:%d\n" , i);
      return -1;
    }
  }
  printf("unpack success! len%d\n" , len);
  gettimeofday(&tv , NULL);
  printf("end:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);

  //print
  //print_bag(&mybag);
  
  sdr_free_bin(pres);
  return 0;
}

static int print_bag(bag_all_t *pbag)
{
  item_info_t *pInfo = NULL;
  int i = 0;
  if(!pbag)
    return -1;

  printf("name:%s money:%ld diamond:%ld gold:%d hurt:%lld\n" , pbag->attr.name , pbag->attr.money ,
    pbag->attr.diamond , pbag->attr.gold , pbag->hurt);

  printf("expend_items type:%d count:%d\n" , pbag->expend_items.type , pbag->expend_items.count);
  for(i=0; i < pbag->expend_items.count; i++)
  {
    pInfo = &pbag->expend_items.list[i];
    printf(">[%d] res:%ld inst:%ld count:%d\n", pInfo->grid , pInfo->res_id , pInfo->instid , 
      pInfo->count);
  }

  return 0;
}
