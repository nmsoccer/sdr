#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sdr/sdr.h>
#include "bag.h"

#define SDR_FILE "./bag.sdr"
#define MAX_BUFF_SIZE (1024*1024)
#define REAL_ITEM_COUNT 128
#define SERIAL_FILE "./bag_info"

#define MAX_HANDLE_TIMES (1000000)

static int print_bag(bag_all_t *pbag);

int main(int argc , char **argv)
{
  sdr_data_res_t *pres = NULL;
  bag_all_t mybag;
  bag_all_t otherbag;
  int i = 0;
  int ret = -1;
  struct timeval tv;
  char *buff = NULL;
  int len = -1;
  FILE *fp = NULL;

  //fp
  fp = fopen(SERIAL_FILE , "w");
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

  srand(time(NULL));
  //basic attr
  mybag.attr.money = 1280;
  mybag.attr.diamond = 10;
  mybag.attr.gold = 16690;
  strncpy(mybag.attr.name , "cs_fuck_suomei" , sizeof(mybag.attr.name));

  //item_list
  mybag.expend_items.type = 1;
  mybag.expend_items.count = REAL_ITEM_COUNT;
  for(i=0; i<REAL_ITEM_COUNT; i++)
  {
    mybag.expend_items.list[i].res_id = rand()%10000;
    mybag.expend_items.list[i].count = rand()%100;
    mybag.expend_items.list[i].instid = 100000 + rand()%100000;    
    mybag.expend_items.list[i].grid = i;
  }

  //buff
  //print_bag(&mybag);
  buff = calloc(1 , MAX_BUFF_SIZE);
  if(!buff)
  {
    printf("malloc buff failed!\n");
    return -1;
  }

  //pack
  printf("bagall orinal len:%d\n" , sizeof(mybag));
  gettimeofday(&tv , NULL);
  printf("starts:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);


  for(i=0; i<MAX_HANDLE_TIMES; i++)
  {
    len = sdr_pack(pres, buff, (char *)&mybag, "bag_all", 0, 0 , NULL);
    if(len <= 0)
    {
      printf("pakced failed! i:%d\n" , i);
      return -1;
    }
  }
  printf("packed_len:%d\n" , len);

  gettimeofday(&tv , NULL);
  printf("end:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);
  

  //unpack
  /*
  printf("---------------\n\n");
  gettimeofday(&tv , NULL);
  printf("starts:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);
  
  len = sdr_unpack(pres, (char *)&otherbag , buff, "bag_all", 0 , NULL);
  printf("unpacked_len:%d\n" , len);

  gettimeofday(&tv , NULL);
  printf("end:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);
  print_bag(&otherbag);*/
  
  fwrite(buff , 1 , len , fp);
  fflush(fp);
  //free sdr
  sdr_free_bin(pres);
  return 0;
}

static int print_bag(bag_all_t *pbag)
{
  item_info_t *pInfo = NULL;

  int i = 0;
  if(!pbag)
    return -1;

  printf("name:%s money:%ld diamond:%ld gold:%d hurt:%ld\n" , pbag->attr.name , pbag->attr.money ,
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
