/*
*Created by sdrconv
*@2017-2-19 14:3:28
*/

#ifndef _demon_h
#define _demon_h

#define VERSION 1
#define MAX_NAME_LEN 32
struct _user_info
{
	int sex;// 性别  
	char count;   
	char user_name[MAX_NAME_LEN];// 姓名  
	int age;//Ver.2 年龄  
}__attribute__((packed));
typedef struct _user_info user_info_t;

#define Q_SKILL 1
#define W_SKILL 2
#define E_SKILL 3
#define R_SKILL 4
union _skill_data
{
	char qskill;//   Q_SKILL
	int wskill;//   W_SKILL
	char eskill[12];//   E_SKILL
	user_info_t rskill;//   R_SKILL
}__attribute__((packed));
typedef union _skill_data skill_data_t;

struct _skill_info
{
	char type;   
	skill_data_t data;   
}__attribute__((packed));
typedef struct _skill_info skill_info_t;

struct _skill_list
{
	char skill_count;   
	int useless;//Ver.10   
	char name[MAX_NAME_LEN];// 姓名  
	skill_info_t info_list[5];   
}__attribute__((packed));
typedef struct _skill_list skill_list_t;

#endif
