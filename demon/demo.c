#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sdr/sdr.h>
#include "demo.h"

#define MAX_BUFF_LEN (10*1024)
#define SDR_PROTO_FILE "./demo.sdr"
static int print_user_info(user_info_t *puser)
{
	int i = 0;
	if(!puser)
		return -1;

	printf("-------------------------------\n");
	printf("sex:%d name:%s age:%d height:%f money:%lld gold:%ld lat:%lf lng:%lf\n" , puser->sex , puser->user_name , puser->age , puser->height, puser->money , puser->gold , puser->lat , puser->lng);

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
    //init
	memset(&src_user , 0 , sizeof(src_user));
	memset(&dst_user , 0 , sizeof(dst_user));

    src_user.age = 32;
    src_user.gold = 5000;
    src_user.money = 1289;
    src_user.sex = 1;
    src_user.name_len = strlen("cs_f**k_suomei");
    strncpy(src_user.user_name , "cs_f**k_suomei" , sizeof(src_user.user_name));
    src_user.height = 1.73;
    src_user.lat = 38.65777;
    src_user.lng = 104.08296;

    src_user.skill.skill_count = 2;
    src_user.skill.info_list[0].type = Q_SKILL;
    src_user.skill.info_list[0].data.qskill = 111;

    src_user.skill.info_list[1].type = E_SKILL;
    strncpy(src_user.skill.info_list[1].data.eskill , "wear" , sizeof(src_user.skill.info_list[1].data.eskill));

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

    while(version<3)
    {
    	version++;
    	printf("\n\nversion [%d] ==========================\n" , version);
        memset(buff , 0 , sizeof(buff));
    	memset(&dst_user , 0 , sizeof(dst_user));

    	//pack
    	len = sdr_pack(pres , buff , (char *)&src_user , "user_info" , version , NULL);
    	if(len < 0)
    	{
    		printf("sdr_pack failed!\n");
    		continue;
    	}
    	printf("sdr_pack len:%d\n" , len);

    	//unpack
    	len = sdr_unpack(pres , (char *)&dst_user , buff , "user_info" , NULL);
    	if(len < 0)
    	{
    		printf("sdr_unpack failed!\n");
    		return -1;
    	}
    	printf("sdr_unpack len:%d\n" , len);
    	print_user_info(&dst_user);
    }

    //free
    sdr_free_bin(pres);
    return 0;
}
