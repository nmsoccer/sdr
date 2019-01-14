#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sdr/sdr.h>
#include "dump.h"

#define MAX_BUFF_LEN (10*1024)
#define SDR_PROTO_FILE "./dump.sdr"
#define OUT_XML "./dump_out.xml"
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
    user_info_t src_user;
    user_info_t dst_user;
    int len = 0;
    int version = -1;
    int ret = -1;
    int i = 0;
    FILE *fp = fopen(OUT_XML , "w+");
    if(!fp)
    {
      printf("open %s failed!\n" , OUT_XML);
      return -1;
    }
    //init
    memset(&src_user , 0 , sizeof(src_user));
    memset(&dst_user , 0 , sizeof(dst_user));
    srand(time(NULL));


    src_user.age = 32;
    src_user.gold = 5000;
    src_user.money = 1289;
    src_user.sex = 1;
    src_user.name_len = strlen("cs_f**k_suomei");
    strncpy(src_user.user_name , "cs_f**k_suomei" , sizeof(src_user.user_name));
    src_user.height = 1.73;
    src_user.lat = 38.65777;
    src_user.lng = 104.08296;

    src_user.flags[0] = 177;
    src_user.flags[1] = 3;
    src_user.skill.skill_count = 2;
    src_user.skill.info_list[0].type = Q_SKILL;
    src_user.skill.info_list[0].data.qskill = 111;

    src_user.skill.info_list[1].type = E_SKILL;
    strncpy(src_user.skill.info_list[1].data.eskill , "wear" , sizeof(src_user.skill.info_list[1].data.eskill));

    for(i=0; i<MAX_ITEM_COUNT; i++)
    {
       src_user.item_list[i].resid = 4000+i;
       src_user.item_list[i].count = i*2;
       src_user.item_list[i].instid = 10000+i;
       src_user.item_list[i].grid = i;
    } 
    strncpy(src_user.desc , "hello world!" , sizeof(src_user.desc));
    strcpy(&src_user.desc[strlen(src_user.desc)+2] , "日了狗");

    printf("1) orignal==========================\n\n");
    print_user_info(&src_user);
    print_user_info(&dst_user);

    //sdr
    pres = sdr_load_bin(SDR_PROTO_FILE , NULL);
    if(!pres)
    {
    	printf("load %s failed!\n" , SDR_PROTO_FILE);
    	return -1;
    }

    printf("2) pack&unpacked==========================\n\n");
    //test pack
    ret = sdr_pack(pres , buff , (char *)&src_user , "user_info" , 3 , 0 , NULL);
    if(!ret)
    {
      printf("pack failed!\n");
      return -1;
    }
    printf("packed:%d\n" , ret);

    //test unpack
    ret = sdr_unpack(pres, (char *)&dst_user, buff, "user_info", 0 , NULL);
    if(!ret)
    {
      printf("unpack failed!\n");
      return -1;
    }
    printf("unpacked:%d\n" , ret);
    print_user_info(&src_user);
    print_user_info(&dst_user);


    //test dump
    ret = sdr_dump_struct(pres , "user_info" , (char *)&src_user , fp);
    printf("sdr_dump_struct ret:%d\n" , ret);
    

    //free
    sdr_free_bin(pres);
    return 0;
}
