/*
 * use_demon.c
 *
 *  Created on: 2015-3-24
 *      Author: Administrator
 */

#include <stdio.h>
#include <stdlib.h>

#include "demo.h"
#include "sdr.h"

#define TARGET_BIN "demo.bin"

int main(int argc , char **argv)
{
	sdr_data_res_t *pres;
	char buff[1024] = {0};

	user_info_t user_info;
	skill_list_t skill_list;
	skill_list_t skill_list_unpacked;


	//1.
	pres = sdr_load_bin(TARGET_BIN , NULL);
	if(!pres)
	{
		printf("load %s failed!\n" , TARGET_BIN);
		return -1;
	}

//	printf("size is:%d\n" , sizeof(skill_list_t));

	//2.pack
	memset(&skill_list , 0 , sizeof(skill_list_t));
	strncpy(skill_list.name , "shut!" , MAX_NAME_LEN);
	skill_list.useless = 12;
	skill_list.skill_count = 2;
	skill_list.info_list[0].type = R_SKILL;
	skill_list.info_list[0].data.rskill.count = 5;
	skill_list.info_list[1].type = Q_SKILL;
	skill_list.info_list[1].data.qskill = 3;
	printf("qskill is:%d , useless:%d , skillcount:%d\n" , skill_list.info_list[1].data.qskill , skill_list.useless , skill_list.skill_count);
	if(sdr_pack(pres , buff , &skill_list , "skill_list" , 2 , NULL) < 0)
		printf("pack failed!\n");

	//3.unpack
	printf("-------------------------------\n");
	memset(&skill_list_unpacked , 0 , sizeof(skill_list_t));
	skill_list_unpacked.useless = 17;
	if(sdr_unpack(pres , &skill_list_unpacked , buff , "skill_list" , NULL) < 0)
		printf("unpack failed!\n");
	else
		printf("qskill is:%d , useless:%d , skillcount:%d,"
				"name:%s\n" , skill_list_unpacked.info_list[1].data.qskill , skill_list_unpacked.useless , skill_list_unpacked.skill_count , skill_list_unpacked.name);

	//3.free
	sdr_free_bin(pres);

	return 0;
}
