#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sdr/sdr.h>
#include "dump.h"

#define MAX_BUFF_LEN (10*1024)
#define SDR_PROTO_FILE "./dump.sdr"
#define BIN_FILE "./dump_out.xml.dump.bin"
#define OUT_XML "./import_out.xml"
static int print_user_info(user_info_t *puser)
{
	int i = 0;
	if(!puser)
		return -1;

	printf("-------------------------------\n");
        printf("sex:%d name:%s age:%d money:%lld gold:%ld height:%f , lat:%lf , lng:%lf\n" , puser->sex , puser->user_name , puser->age ,
                        puser->money , puser->gold , puser->height , puser->lat , puser->lng);

        printf("flags:\n");
        for(i=0; i<sizeof(puser->flags); i++)
          printf("'%d'" , puser->flags[i]);
        printf("\n");        

	for(i=0; i<puser->skill.skill_count; i++)
	{
		printf("skill<%d> type:%d\n" , i , puser->skill.info_list[i].type);
		switch(puser->skill.info_list[i].type)
		{
		case Q_SKILL:
			printf("qskill:%d\n" , puser->skill.info_list[i].data.qskill);
			break;
		case W_SKILL:
			printf("wskill:%d\n" , puser->skill.info_list[i].data.wskill);
			break;
		case E_SKILL:
			printf("eskill:%s\n" , puser->skill.info_list[i].data.eskill);
			break;
		case R_SKILL:
			printf("rskill:%d\n" , puser->skill.info_list[i].data.rskill);
			break;
		default:
			printf("ha?\n");
			break;
		}
	}

	return 0;
}

int main(int argc , char **argv)
{
    sdr_data_res_t *pres;
    char buff[MAX_BUFF_LEN] = {0};
    user_info_t dst_user;
    int len = 0;
    int version = -1;
    int ret = -1;
    int i = 0;
    FILE *fp_bin = NULL;
    FILE *fp = fopen(OUT_XML , "w+");
    if(!fp)
    {
      printf("open %s failed!\n" , OUT_XML);
      return -1;
    }

    fp_bin = fopen(BIN_FILE , "r");
    if(!fp_bin)
    {
      printf("open %s failed!\n" , BIN_FILE);
      return -1;
    } 


    //init
    memset(&dst_user , 0 , sizeof(dst_user));

    //sdr
    pres = sdr_load_bin(SDR_PROTO_FILE , NULL);
    if(!pres)
    {
    	printf("load %s failed!\n" , SDR_PROTO_FILE);
    	return -1;
    }

    //read bin file
    ret = fread(buff , 1 , MAX_BUFF_LEN , fp_bin);
    if(ret < 0)
    {
        printf("read %s failed!\n" , BIN_FILE);
        return -1;
    }
    printf("read %s success! bytes:%d\n" , BIN_FILE , ret);

    //unpack
    len = sdr_unpack(pres , (char *)&dst_user , buff , "user_info" , NULL);
    if(len < 0)
    {
        printf("sdr_unpack failed!\n");
        return -1;
    }
    printf("sdr_unpack len:%d\n" , len);
    print_user_info(&dst_user);

    // dump
    ret = sdr_dump_struct(pres , "user_info" , (char *)&dst_user , fp);
    printf("sdr_dump_struct ret:%d\n" , ret);
    

    //free
    sdr_free_bin(pres);
    return 0;
}
