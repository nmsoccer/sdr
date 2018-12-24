#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include "bag.pb.h"
#include <sys/time.h>
using namespace std;

#define MAX_ITEM_COUNT (1024)
#define REAL_ITEM_COUNT 128
#define MAX_HANDLE_TIMES (1000000)

#define SERIAL_FILE "./bag_info"
static int print_bag(bag_all *pbag);



int main(int argc , char **argv)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  item_info* pitem = NULL;
  int i = 0;
  int packed_len = 0;
  int ret = 0;
  struct timeval tv;

  srand(time(NULL));
  //bag
  bag_all mybag;

  //attr
  basic_attr *pattr = new basic_attr();
  pattr->set_money(78992);
  pattr->set_diamond(1001);
  pattr->set_exp(45536);
  pattr->set_gold(1998);  
  pattr->set_name("cs_fuck_suomei");

  //item_list
  item_list *pexpend_items = new item_list();
  pexpend_items->set_type(2001);  

  //construct
  mybag.set_allocated_attr(pattr);
  if(mybag.has_attr())
    cout << "has attr!" << endl;
  else
    cout << "no attr!" << endl;

  //item_list  
  mybag.set_allocated_expend_items(pexpend_items);
  if(mybag.has_expend_items())
    cout << "has expend items!" << endl;
  else
    cout << "no expend items!" << endl;

  for(i=0; i<REAL_ITEM_COUNT; i++)
  {
    pitem = pexpend_items->add_list();
    pitem->set_res_id(rand()%10000);
    pitem->set_count(rand()%100);
    pitem->set_instid(100000 + rand()%100000);
    pitem->set_grid(i);
  }  

  //print_bag(&mybag);
  //pack
  gettimeofday(&tv , NULL);
  printf("start %ld:%ld\n" , tv.tv_sec , tv.tv_usec);

  string str;
  for(i=0; i<MAX_HANDLE_TIMES; i++)
  {
    if(!mybag.SerializeToString(&str))
      cout << "pack failed!" << endl; 
  }

  gettimeofday(&tv , NULL);
  printf("end %ld:%ld\n" , tv.tv_sec , tv.tv_usec);

  //write to file
  packed_len = str.length();
  cout << "packed:" << packed_len << endl;
  fstream output(SERIAL_FILE, ios::out | ios::trunc | ios::binary);
  output.write(str.c_str() , packed_len); 

  /*
  if (!mybag.SerializeToOstream(&output)) {
    cerr << "Failed to write mybag." << endl;
  }
  else
    cout << "write output success!" << endl;
  */

  google::protobuf::ShutdownProtobufLibrary();  
  return 0;
}

static int print_bag(bag_all *pbag)
{
  int i = 0;
  if(!pbag)
    return -1;
  
  const basic_attr &attr = pbag->attr();
  const item_list &expend_items = pbag->expend_items(); 

  //basic
  cout << "name:" << attr.name() << endl;
  cout << "money:" << attr.money() << endl;
  cout << "diamond:" << attr.diamond() << endl;
  cout << "exp:" << attr.exp() << endl;

  //items
  cout << "item_list.type:" << expend_items.type() << endl;
  cout << "item_list.size:" << expend_items.list_size() << endl;
  for(i=0; i<expend_items.list_size(); i++)
  {
    const item_info &info = expend_items.list(i);
    printf(">[%d] res:%ld inst:%ld count:%d\n", info.grid() , info.res_id() , info.instid() ,
      info.count());
  }

  return 0;
}
