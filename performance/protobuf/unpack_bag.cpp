#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include "bag.pb.h"
#include <sys/time.h>
using namespace std;

#define MAX_ITEM_COUNT (1024)
#define REAL_ITEM_COUNT 128
#define MAX_BUF_SIZE (1024*1024)
#define MAX_HANDLE_TIMES (1000000)

#define SERIAL_FILE "./bag_info"
static int print_bag(bag_all *pbag);

int main(int argc , char **argv)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  bag_all mybag;
  bag_all src_bag;
  char buff[MAX_BUF_SIZE] = {0};
  int i = 0;
  int size = 0;
  string str;
  struct timeval tv;

  //read
  fstream input(SERIAL_FILE, ios::in | ios::binary);
  input.seekg(0,ifstream::end);
  size=input.tellg();
  input.seekg(0);
  cout << "file size:" << size << endl;

  /*
  input.read(buff , size);
  cout << "try to unpack" << endl; 
  string str = buff;
  for(i=0; i<MAX_HANDLE_TIMES; i++)
  {
    if(!mybag.ParseFromString(str))
    {
      cerr << "Failed to parse my bag." << endl;
      return -1;
    }
  }*/  
  
  if (!src_bag.ParseFromIstream(&input)) {
    cerr << "Failed to parse bag." << endl;
    return -1;
  }
  else
    cout << "parse bag success!" << endl;
  if(!src_bag.SerializeToString(&str))
  {
      cout << "pack failed!" << endl; 
      return -1;
  }

  //real pack
  gettimeofday(&tv , NULL); 
  printf("start %ld:%ld\n" , tv.tv_sec , tv.tv_usec);

  for(i=0; i<MAX_HANDLE_TIMES; i++)
  {
    if(!mybag.ParseFromString(str))
    {
      cerr << "Failed to real parse my bag." << endl;
      return -1;
    }
  }

  gettimeofday(&tv , NULL);
  printf("end %ld:%ld\n" , tv.tv_sec , tv.tv_usec);

  //print_bag(&mybag);
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

  cout << "name:" << attr.name() << endl;
  cout << "money:" << attr.money() << endl;
  cout << "diamond:" << attr.diamond() << endl;
  cout << "exp:" << attr.exp() << endl;

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
