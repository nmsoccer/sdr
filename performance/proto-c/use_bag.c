#include <stdio.h>
#include <stdlib.h>
#include "bag.pb-c.h"

#define MAX_ITEM_COUNT (1024)
#define REAL_ITEM_COUNT 128
#define MAX_HANDLE_TIMES (1000000)

#define SERIAL_FILE "./bag_info"
static int print_bag(BagAll *pbag);


int main(int argc , char **argv)
{
  BagAll zone_bag = BAG_ALL__INIT;
  BasicAttr basic_attr = BASIC_ATTR__INIT;
  ItemList item_list = ITEM_LIST__INIT;
  int i = 0;
  int packed_len = 0;
  long curr_time;
  char *buff = NULL;
  size_t ret = 0;
  int len = 0;
  FILE *fp = NULL;
  struct timeval tv;
  
  fp = fopen(SERIAL_FILE , "w");
  if(!fp)
  {
	  printf("open %s failed!\n" , SERIAL_FILE);
	  return 0;
  }

  //
  srand(time(NULL));


  //basic attr
  basic_attr.money = 1280;
  basic_attr.diamond = 10;
  basic_attr.gold = 16690;
  basic_attr.name = "cs_fuck_suomei";
  zone_bag.attr = &basic_attr;

  //item_list
  item_list.type = 1;
  item_list.list = calloc(REAL_ITEM_COUNT , sizeof(ItemInfo *));
  item_list.n_list = REAL_ITEM_COUNT;
  for(i=0; i<REAL_ITEM_COUNT; i++)
  {
    item_list.list[i] = calloc(1 , sizeof(ItemInfo));
    item_info__init(item_list.list[i]);
    item_list.list[i]->res_id = rand()%10000;
    item_list.list[i]->count = rand()%100;
    item_list.list[i]->instid = 100000 + rand()%100000;    
    item_list.list[i]->grid = i;
  }
  zone_bag.expend_items = &item_list;

  //src len
  //print_bag(&zone_bag);
  printf("bagall orinal len:%d\n" , sizeof(BagAll)+sizeof(BasicAttr)+sizeof(ItemList)+(REAL_ITEM_COUNT*sizeof(ItemInfo)));  
  
  //pack
  gettimeofday(&tv , NULL);
  printf("starts:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);
    
  packed_len = bag_all__get_packed_size(&zone_bag);
  buff = calloc(1 , packed_len);

  for(i=0; i<MAX_HANDLE_TIMES; i++)
  {
    ret = bag_all__pack(&zone_bag, buff);
    if(ret < 0)
    {
      printf("pack failed! i:%d\n" , i);
      return -1;
    }
  }
  printf("packed_len:%d and ret:%d\n" , packed_len , ret);
  
  gettimeofday(&tv , NULL);  
  printf("end:%ld:%ld\n" , tv.tv_sec , tv.tv_usec);

  //write to stdout
  fwrite(buff , packed_len , 1 , fp);
  fflush(fp);
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

