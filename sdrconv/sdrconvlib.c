/*
 * sdrconvlib.c
 *
 *  Created on: 2015-3-17
 *      Author: soullei
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include "sdr.h"
#include "sdrconv.h"

extern int errno;

/*
 * Hash Map
 * 用来扩大max_node(node_map 和 sym_map共用)
 */
static int hash_size_map[] = { //7 , 13 , 19 , 27 , 37 ,
		61, 113 , 211 , 379 , 509 , 683 , 911 , //<1K
		1217 , 1627 , 2179 , 2909 , 3881 , 6907 , 9209, //<10K
		12281 , 16381 , 21841 , 29123 , 38833 , 51787 , 69061 , 92083, //<100K
		122777,163729,218357,291143,388211,517619,690163,999983, //<1M
		1226959 , 1635947 , 2181271 , 2908361 , 3877817 , 5170427,6893911,9191891, //<10M
		12255871 , 16341163,21788233,29050993,38734667,51646229,68861641,91815541, //<100M
};


static sdr_node_t *sdr_parse_entry(sdr_conv_env_t *penv , sdr_node_t *pparent);
static int fetch_attr_value(sdr_conv_env_t *penv , char *src , char *attr_name , char *attr_value);
static int converse_label_type(char *label_type , int *size);
static char *get_macro_value(sdr_conv_env_t *penv , char *macro_name);
static sdr_node_t *entry_of_struct(sdr_conv_env_t *penv , sdr_node_t *pparent , char *entry_name);
static sdr_node_t *entry_of_union(sdr_conv_env_t *penv , sdr_node_t *pparent , int selcted_id);
static packed_sym_table_t *pack_sym_table(sdr_conv_env_t *penv);

static int gen_struct_h(sdr_conv_env_t *penv , sdr_node_t *pnode , FILE *fp);
static int gen_union_h(sdr_conv_env_t *penv , sdr_node_t *pnode , FILE *fp);
static int gen_entry_h(sdr_conv_env_t *penv , sdr_node_t *pnode , FILE *fp);
static int extend_max_node(int max_node , sdr_conv_env_t *penv);

/*
 * 分析注释信息
 * penv->working_line所存储的为待分析的第一行数据
 * return:
 * 0:success
 * -1:error
 */
int sdr_parse_comment(sdr_conv_env_t *penv)
{
	int ret;
	char place;
	/***Arg Check*/
	if(!penv)
		return -1;

	//检查起始<!--
	place = penv->working_line[4];
	penv->working_line[4] = 0;
	if(strcmp(penv->working_line , "<!--") != 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Comment Format");
		return -1;
	}
	//处理<!--之后的内容
	penv->working_line[4] = place;
	copy_str(penv->working_line , &penv->working_line[4]);

	/***Handle*/
	//目标搜寻 -->
	do
	{
		//首先处理当前行
		ret = forward_to_char(penv->working_line , '-');

		//匹配'-'
		if(ret == 0)
		{
			//如果找到-->跳出循环
			if(penv->working_line[1]=='-' && penv->working_line[2]=='>')
			{
				break;
			}
			//没有找到则在该行继续找
			copy_str(penv->working_line , &penv->working_line[1]);
			continue;
		}

		//出错或者没有目标，则读取下一行继续找
		ret = read_one_line(penv);
		if(ret < 0)	//没有能够正常完成注释解析
		{
			sdr_print_info(penv , SDR_INFO_ERR , "Failed to Parse Comment");
			return -1;
		}

	}while(1);

	return 0;
}

/*
 * 分析macro
 * penv->working_line所存储的为待分析的第一行数据
 * return:
 * NULL:ERROR
 * Else:当前分析的节点指针
 */
sdr_node_t *sdr_parse_macro(sdr_conv_env_t *penv)
{
	sdr_node_t *pnode = NULL;
	char place;
	int index;
	char attr_value[SDR_DESC_LEN] = {0};
	int ret;
	char *start;

	/***Arg Check*/
	if(!penv)
		return NULL;

	//检查<macro
	if(penv->working_line[0] != XML_LEFT_BRACKET)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Macro Format");
		return NULL;
	}
	index = 1 + strlen(XML_LABEL_MACRO);	//strlen("<macro")
	place = penv->working_line[index];
	penv->working_line[index] = 0;
	if(strcmp(&penv->working_line[1] , XML_LABEL_MACRO) != 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Macro Format");
		return NULL;
	}

	penv->working_line[index] = place;
	copy_str(penv->working_line , &penv->working_line[index]);
	/***Handle*/
	//1.获取Node
	pnode = get_node(penv);
	if(!pnode)
		return NULL;

	pnode->class = SDR_CLASS_MACRO;
	//2.获取属性标签
	//name
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_NAME , pnode->node_name);
	if(ret < 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Failed to Get Attr '%s'" , XML_LABEL_NAME);
		return NULL;
	}

	//value
	memset(attr_value , 0 , sizeof(attr_value));
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_VALUE , attr_value);
	if(ret < 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Failed to Get Attr '%s'" , XML_LABEL_VALUE);
		return NULL;
	}
	strncpy(pnode->data.macro_value , attr_value , MACRO_VALUE_LEN);

	//desc
	fetch_attr_value(penv , penv->working_line , XML_LABEL_DESC , pnode->node_desc);

	/***检查括弧是否封闭 />*/
	start = strrchr(penv->working_line , '>');
	if(!start)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "No '%c' at the end of line" , XML_RIGHT_BRACKET);
		return NULL;
	}
	start--;
	if(start[0] != '/')
	{
		sdr_print_info(penv , SDR_INFO_ERR , "No Closed of Macro at the end of line" , XML_RIGHT_BRACKET);
		return NULL;
	}
	return pnode;
}

/*
 * 分析struct
 * penv->working_line所存储的为待分析的第一行数据
 * return:
 * NULL:ERROR
 * Else:当前分析的节点指针
 */
sdr_node_t *sdr_parse_struct(sdr_conv_env_t *penv)
{
	sdr_node_t *pnode = NULL;
	sdr_node_t *pnode_entry = NULL;
	sdr_node_t *pnode_now_entry = NULL;
	char place;
	int index;
	char attr_value[SDR_DESC_LEN] = {0};
	int ret;
	char *start;
	int size = 0;

	/***Arg Check*/
	if(!penv)
		return NULL;

	//检查<struct
	if(penv->working_line[0] != XML_LEFT_BRACKET)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Struct Format");
		return NULL;
	}
	index = 1 + strlen(XML_LABEL_STRUCT);	//strlen("<struct")
	place = penv->working_line[index];
	penv->working_line[index] = 0;
	if(strcmp(&penv->working_line[1] , XML_LABEL_STRUCT) != 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Struct Format");
		return NULL;
	}

	penv->working_line[index] = place;
	copy_str(penv->working_line , &penv->working_line[index]);
	/***Handle*/
	//1.获取Node
	pnode = get_node(penv);
	if(!pnode)
		return NULL;
	pnode->class = SDR_CLASS_STRUCT;
	//2.获取属性标签
	//name
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_NAME , pnode->node_name);
	if(ret < 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Failed to Get Attr '%s'" , XML_LABEL_NAME);
		return NULL;
	}

	//version
	memset(attr_value , 0 , sizeof(attr_value));
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_VERSION , attr_value);
	if(ret == 0)
	{
		pnode->version = atoi(attr_value);
	}

	//desc
	fetch_attr_value(penv , penv->working_line , XML_LABEL_DESC , pnode->node_desc);

	/***检查本行括弧是否封闭 >*/
	start = strrchr(penv->working_line , '>');
	if(!start)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "No '%c' at the end of line" , XML_RIGHT_BRACKET);
		return NULL;
	}

	/***依次解析entry，直到遇见</struct>*/
	do
	{
		//读入新行
		ret = read_one_line(penv);
		if(ret < 0)	//没有能够正常完成注释解析
		{
			sdr_print_info(penv , SDR_INFO_ERR , "No Closed %s" , XML_LABEL_STRUCT);
			return NULL;
		}

		//只解析<entry>或者</struct>
		ret = forward_to_char(penv->working_line , '<');
		if(ret < 0)
			continue;

		//</struct>
		if(penv->working_line[1] == '/')
		{
			penv->working_line[strlen("</struct>")] = 0;
			if(strcmp(penv->working_line , "</struct>") != 0)
			{
				sdr_print_info(penv , SDR_INFO_ERR , "No Closed %s" , XML_LABEL_STRUCT);
				return NULL;
			}
			else	//正常结束
				break;
		}


		//<entry ... />
		if(penv->working_line[1] == 'e')
		{
			pnode_entry = sdr_parse_entry(penv , pnode);
			if(!pnode_entry)	//解析entry失败
			{
				return NULL;
			}

			//解析成功，连接成entry串
			if(pnode_now_entry == NULL)	//当前是第一个entry
			{
				pnode_now_entry = pnode_entry;
				pnode->data.struct_value.child_idx = pnode_now_entry->my_idx;
				pnode_entry->data.entry_value.offset = 0;	//偏移0字节
			}
			else	//其他的entry
			{
				pnode_entry->data.entry_value.offset = pnode_now_entry->data.entry_value.offset +
																				 pnode_now_entry->size*pnode_now_entry->data.entry_value.count;	//当前节点偏移=兄节点偏移+兄节点长度
				pnode_now_entry->sibling_idx = pnode_entry->my_idx;
				pnode_now_entry = pnode_entry;
			}

			sdr_print_info(penv , SDR_INFO_NORMAL , "Entry Name:%s,Index:%d , Version:%d,Size:%d,Type:%d,Count:%d,Offset:%d,Refer:%d,Select:%d,Desc:%s" ,
								pnode_entry->node_name , pnode_entry->my_idx ,	pnode_entry->version ,
								pnode_entry->size , pnode_entry->data.entry_value.entry_type , pnode_entry->data.entry_value.count,pnode_entry->data.entry_value.offset ,
								pnode_entry->data.entry_value.refer_idx , pnode_entry->data.entry_value.select_idx , pnode_entry->node_desc);
			pnode->size += (pnode_entry->size*pnode_entry->data.entry_value.count);	//struct大小
		}

		//other
		continue;

	}while(1);

	/***Return*/
	return pnode;
}


/*
 * 分析union
 * penv->working_line所存储的为待分析的第一行数据
 * return:
 * NULL:ERROR
 * Else:当前分析的节点指针
 */
sdr_node_t *sdr_parse_union(sdr_conv_env_t *penv)
{
	sdr_node_t *pnode = NULL;
	sdr_node_t *pnode_entry = NULL;
	sdr_node_t *pnode_now_entry = NULL;
	char place;
	int index;
	char attr_value[SDR_DESC_LEN] = {0};
	int ret;
	char *start;
	int size = 0;

	/***Arg Check*/
	if(!penv)
		return NULL;

	//检查<struct
	if(penv->working_line[0] != XML_LEFT_BRACKET)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Union Format");
		return NULL;
	}
	index = 1 + strlen(XML_LABEL_UNION);	//strlen("<union")
	place = penv->working_line[index];
	penv->working_line[index] = 0;
	if(strcmp(&penv->working_line[1] , XML_LABEL_UNION) != 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Union Format");
		return NULL;
	}

	penv->working_line[index] = place;
	copy_str(penv->working_line , &penv->working_line[index]);
	/***Handle*/
	//1.获取Node
	pnode = get_node(penv);
	if(!pnode)
		return NULL;
	pnode->class = SDR_CLASS_UNION;
	//2.获取属性标签
	//name
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_NAME , pnode->node_name);
	if(ret < 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Failed to Get Attr '%s'" , XML_LABEL_NAME);
		return NULL;
	}

	//version
	memset(attr_value , 0 , sizeof(attr_value));
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_VERSION , attr_value);
	if(ret == 0)
	{
		pnode->version = atoi(attr_value);
	}

	//desc
	fetch_attr_value(penv , penv->working_line , XML_LABEL_DESC , pnode->node_desc);

	/***检查本行括弧是否封闭 >*/
	start = strrchr(penv->working_line , '>');
	if(!start)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "No '%c' at the end of line" , XML_RIGHT_BRACKET);
		return NULL;
	}

	/***依次解析entry，直到遇见</union>*/
	do
	{
		//读入新行
		ret = read_one_line(penv);
		if(ret < 0)	//没有能够正常完成注释解析
		{
			sdr_print_info(penv , SDR_INFO_ERR , "No Closed %s" , XML_LABEL_UNION);
			return NULL;
		}

		//只解析<entry>或者</union>
		ret = forward_to_char(penv->working_line , '<');
		if(ret < 0)
			continue;

		//</union>
		if(penv->working_line[1] == '/')
		{
			penv->working_line[strlen("</union>")] = 0;
			if(strcmp(penv->working_line , "</union>") != 0)
			{
				sdr_print_info(penv , SDR_INFO_ERR , "No Closed %s" , XML_LABEL_UNION);
				return NULL;
			}
			else	//正常结束
				break;
		}


		//<entry ... />
		if(penv->working_line[1] == 'e')
		{
			pnode_entry = sdr_parse_entry(penv , pnode);
			if(!pnode_entry)	//解析entry失败
			{
				return NULL;
			}

			//解析成功，连接成entry串
			if(pnode_now_entry == NULL)	//当前是第一个entry
			{
				pnode_now_entry = pnode_entry;
				pnode->data.union_value.child_idx = pnode_now_entry->my_idx;
			}
			else	//其他的entry
			{
				//offset 都为0
				pnode_now_entry->sibling_idx = pnode_entry->my_idx;
				pnode_now_entry = pnode_entry;
			}

			sdr_print_info(penv , SDR_INFO_NORMAL , "Entry Name:%s,Index:%d , Version:%d,Size:%d,Type:%d,Count:%d,Offset:%d,ID:%d,Desc:%s" , pnode_entry->node_name , pnode_entry->my_idx ,
								pnode_entry->version ,pnode_entry->size , pnode_entry->data.entry_value.entry_type , pnode_entry->data.entry_value.count,
								pnode_entry->data.entry_value.offset ,	pnode_entry->data.entry_value.select_id , pnode_entry->node_desc);

			//union size 以最大成员为准
			if(pnode_entry->size*pnode_entry->data.entry_value.count > pnode->size)
				pnode->size = pnode_entry->size * pnode_entry->data.entry_value.count;
		}

		//other
		continue;

	}while(1);

	/***Return*/
	return pnode;
}


/*
 * 生成.h头文件
 * @return:
 * 0:success
 * 1:failed
 */
int sdr_gen_h(sdr_conv_env_t *penv)
{
	sdr_node_t *pnode;
	FILE *fp = NULL;
	char file_name[SDR_NAME_LEN];
	char buff[SDR_LINE_LEN] = {0};
	char *start;
	int i;
	int ret;

	time_t timep;
	struct tm *ptm;


	/***Arg Check*/
	if(!penv)
		return -1;

	/***Handle*/
	//1.生成输出文件名
	for(i=0; i<strlen(penv->input_name); i++)
	{
		if(penv->input_name[i] == '.')
			break;
		buff[i] = penv->input_name[i];
	}
	strncpy(file_name , buff , sizeof(file_name));
	strcat(buff , ".h");

	//2.打开文件名
	fp = fopen(buff , "w+");
	if(!fp)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Open header file %s Failed!" , buff);
		return -1;
	}

	/*打印时间*/
	timep = time(NULL);
	ptm = localtime(&timep);
	fprintf(fp , "/*\n*Created by sdrconv\n--%d-%d-%d %d:%d:%d\nContact:https://github.com/nmsoccer\n*/\n\n" ,
	        ptm->tm_year+1900 , ptm->tm_mon+1 , ptm->tm_mday , ptm->tm_hour ,
			ptm->tm_min , ptm->tm_sec);

	//3.写入文件头
	fprintf(fp , "#ifndef _%s_h\n#define _%s_h\n\n" , file_name , file_name);
    fprintf(fp , "#ifdef __cplusplus\nextern \"C {\"\n#endif\n\n");
    

	//4.按序输出每个主节点的内容
	for(i=0; i<penv->pnode_map->count; i++)
	{
		pnode = (sdr_node_t *)&penv->pnode_map->node_list[i];

		//根据不同主节点类型
		switch(pnode->class)
		{
		case SDR_CLASS_START:	//start 和 entry 不解析
		case SDR_CLASS_ENTRY:
			break;

		case SDR_CLASS_MACRO:	//打印macro
			fprintf(fp , "#define %s %s" , pnode->node_name , pnode->data.macro_value);
			if(strlen(pnode->node_desc) > 0)
				fprintf(fp , "//%s\n" , pnode->node_desc);
			else
				fprintf(fp , "\n");

			break;
		case SDR_CLASS_STRUCT:	//打印struct
			ret = gen_struct_h(penv , pnode , fp);
			if(ret < 0)
			{
				printf("generate struct %s failed!\n" , pnode->node_name);
				return -1;
			}
			break;
		case SDR_CLASS_UNION:	//打印union
			ret = gen_union_h(penv , pnode , fp);
			if(ret < 0)
			{
				printf("generate union %s failed!\n" , pnode->node_name);
				return -1;
			}
			break;
		}

	}


	//.结束字串
	fprintf(fp , "#ifdef __cplusplus\n}\n#endif\n\n");
	fprintf(fp , "#endif\n");
	fflush(fp);
	fclose(fp);
	return 0;
}


/*
 * 产生bin二进制文件
 */
int sdr_gen_bin(sdr_conv_env_t *penv)
{
	sdr_data_res_t *pres;
	packed_sym_table_t *ppacked_sym_table = NULL;
	int size;
	char *pstart;
	int ret;
	unsigned short check_sum = 0;
	int result = 0;

	/***Arg Check*/
	if(!penv)
		return -1;
	pres = &penv->sdr_res;

	/***Handle*/
	//1.写入magic
	pstart = pres->magic;
	ret = write(penv->out_fd , pstart , sizeof(pres->magic));
	if(ret <= 0)
	{
		printf("output %s failed!\n" , penv->output_name);
		return -1;
	}
	pstart += sizeof(pres->magic);

	//2.写入max_node
	ret = write(penv->out_fd , pstart , sizeof(int));
	if(ret <= 0)
	{
		printf("output %s failed!\n" , penv->output_name);
		return -1;
	}

	//3.写入node_map
	pstart = (char *)pres->pnode_map;
	size = sizeof(sdr_node_map_t) + pres->pnode_map->count*sizeof(sdr_node_t);
	ret = write(penv->out_fd , pstart , size);
	if(ret <= 0)
	{
		printf("output %s failed!\n" , penv->output_name);
		return -1;
	}

	//4.写入sym_table
	ppacked_sym_table = pack_sym_table(penv);
	if(!ppacked_sym_table)
	{
		printf("pack sym table failed!\n");
		return -1;
	}
	dump_sym_table(pres->psym_table , pres->max_node);
	dump_packed_sym_table(ppacked_sym_table);

	//free(ppacked_sym_table);
	/*
	pstart = (char *)pres->psym_table;
	size = sizeof(sym_table_t) + pres->max_node*sizeof(sym_entry_t);
	ret = write(penv->out_fd , pstart , size);
	if(ret <= 0)
	{
		printf("output %s failed!\n" , penv->output_name);
		return -1;
	}*/
	pstart = (char *)ppacked_sym_table;
	size = sizeof(packed_sym_table_t) + ppacked_sym_table->my_list_size*sizeof(packed_sym_entry_t);
	ret = write(penv->out_fd , pstart , size);
	if(ret != size)
	{
		printf("output %s failed! when writing packed_sym_table\n" , penv->output_name);
		return -1;
	}
	free(ppacked_sym_table);

	//5.计算checksum 校验和
	check_sum = check_sum_sdr(pres , &result);
	if(result != 0)
	{
		printf("output %s failed! when calc checksum! sum:%d result:%d\n" , penv->output_name , check_sum , result);
		return -1;
	}
	ret = write(penv->out_fd , &check_sum , sizeof(check_sum));
	if(ret < 0)
	{
		printf("output %s failed! when writing checksum result\n" , penv->output_name);
		return -1;
	}

	return 0;
}

int sdr_reverse_bin(sdr_conv_env_t *penv)
{
	sdr_data_res_t *pres = NULL;
	int i;
	int ret;
	/***Arg Check*/
	if(!penv)
		return -1;

	//Modify Output Name
	if(strlen(penv->output_name) <= 0)
	{
		snprintf(penv->output_name , sizeof(penv->output_name) , "%s.xml" , penv->input_name);
		penv->output_name[sizeof(penv->output_name)-1] = 0;
	}

	/***Handle*/
	//load
	pres = sdr_load_bin(penv->input_name , NULL);
	if(!pres)
		return -1;
	sdr_print_info(penv , SDR_INFO_MAIN , "load %s success!" , penv->input_name);

	//reverse
	ret = sdr_bin2xml(pres , penv->output_name , NULL);
	if(ret == 0)
		sdr_print_info(penv , SDR_INFO_MAIN , "reverse %s success!" , penv->input_name);

	sdr_free_bin(pres);
	return 0;
}


/*
 * 搜索src,跳过所遇见的所有miss，直到遇见第一个非miss的字符，并将之后的字串覆盖src
 * return:
 * 0:成功
 * -1:错误
 * -2:全部是miss字符
 */
int forward_miss_char(char *src , char miss)
{
	char buff[SDR_LINE_LEN] = {0};
	int i;

	/***Arg Check*/
	if(!src || strlen(src)<=0)
		return -1;

	/***Handle*/
	for(i=0; i<strlen(src); i++)
	{
		if(src[i] != miss)
			break;
	}

	//全部是miss字符
	if(i >= strlen(src))
		return -2;

	//首字符就不是miss不需要替换
	if(i == 0)
		return 0;

	//将i之后的字符替换到src
	strncpy(buff , &src[i] , SDR_LINE_LEN);
	memset(src , 0 , strlen(src));
	strncpy(src , buff , SDR_LINE_LEN);
	return 0;
}

/*
 * 搜索src,跳过所有遇见的字符，直到遇见第一个target字符,并将之后的字串覆盖src
 * return:
 * 0:成功
 * -1:错误
 * -2:没有target字符
 */
int forward_to_char(char *src , char target)
{
	char buff[SDR_LINE_LEN] = {0};
	int i;

	/***Arg Check*/
	if(!src || strlen(src)<=0)
		return -1;

	/***Handle*/
	for(i=0; i<strlen(src); i++)
	{
		if(src[i] == target)
			break;
	}

	//没有target
	if(i >= strlen(src))
		return -2;

	//首字符就是target不需要替换
	if(i == 0)
		return 0;

	//将i之后的字符替换到src
	strncpy(buff , &src[i] , SDR_LINE_LEN);
	memset(src , 0 , strlen(src));
	strncpy(src , buff , SDR_LINE_LEN);
	return 0;
}

/*
 * 从缓存中获取一个sdr_node.注意参数已经全部初始化，不要再memset 0了
 * return:
 * NULL:错误
 * else:成功
 */
sdr_node_t *get_node(sdr_conv_env_t *penv)
{
	sdr_node_t *pnode;
	int ret = -1;

	/***Arg Check*/
	if(!penv)
		return NULL;

	if(penv->pnode_map->count >= penv->max_node)
	{
		printf("memory error! max Node Count:%d is not enough!\n"  , penv->max_node);
		printf("Please use 'sdrconv -s number -I inputfile' to specify max node\n");
		return NULL;
		/*
		ret = extend_max_node(penv->max_node , penv);
		if(ret < 0)
		{
			printf("<%s> extend max node failed!\n" , __FUNCTION__);
			return NULL;
		}*/
	}

	pnode = (sdr_node_t *)&penv->pnode_map->node_list[penv->pnode_map->count];
	memset(pnode , 0 , sizeof(sdr_node_t));
	pnode->my_idx = penv->pnode_map->count;
	//pnode->sibling_idx = -1;	//这里要注意

	penv->pnode_map->count++;
	return pnode;
}

/*
 * 读取输入文件一行数据，并存储于sdr_conv_env_t.src_line中
 * 同时内置行计数1
 * 0：成功
 * -1：失败
 * -2：END
 */
int read_one_line(sdr_conv_env_t *penv)
{
	char buff[SDR_LINE_LEN] = {0};
	/***Arg Check*/
	if(!penv)
		return -1;


	if(fgets(buff , sizeof(buff) , penv->in_fp) != NULL)
	{
		memset(penv->src_line , 0 , sizeof(penv->src_line));
		memset(penv->working_line , 0 , strlen(penv->working_line));
		strncpy(penv->src_line , buff , SDR_LINE_LEN);
		strncpy(penv->working_line , buff , SDR_LINE_LEN);
		penv->curr_line++;
		return 0;
	}
	else	//EOF
	{
		return -2;
	}
}

/*
 * 将src所指的字符串通过缓存拷贝到dest
 * return:
 * 0:成功
 * -1:错误
 */
int copy_str(char *dest , char *src)
{
	char buff[SDR_LINE_LEN] = {0};

	/***Arg Check*/
	if(!dest || !src)
		return -1;

	if(strlen(src) == 0)
	{
		dest[0] = 0;
		return 0;
	}

	strncpy(buff , src , strlen(src));
	memset(dest , 0 , strlen(dest));
	strncpy(dest , buff , strlen(buff));
	return 0;
}

int sdr_print_info(sdr_conv_env_t *penv , char type , ...)
{
	char *fmt;
	va_list ap;
	char buff[2048] = {0};

	/***Arg Check*/
	if(!penv)
		return -1;

	/***Get Va list*/
	va_start(ap , type);	//ap初始化为type之后的下一个参数地址,即fmt
	fmt = va_arg(ap , char *);	//返回ap当前所指地址 fmt，同时将ap地址增加sizeof(char *) 指向下一个参数地址(不定参数)

	switch(type)
	{
	case SDR_INFO_NORMAL:
		if(penv->debug_info == 1)
		{
			vsnprintf(buff , sizeof(buff) , fmt , ap);
		}
		else
			return 0;
		break;
	case SDR_INFO_MAIN:
		vsnprintf(buff , sizeof(buff) , fmt , ap);
		break;
	case SDR_INFO_ERR:
		snprintf(buff , sizeof(buff) , ">>>>Error:[Line]:%d,[Refer]:%s----[REASON]:" , penv->curr_line , penv->src_line);
		vsnprintf(&buff[strlen(buff)] , sizeof(buff)-strlen(buff) , fmt , ap);
		break;
	}

	printf("%s\n" , buff);
	return 0;
}

/*
 * 将一个主节点名字存储在sym_table哈希表中
 * @sym_name:主节点名
 * @index:节点index
 * @return:
 * 0:成功
 * -1:失败
 * -2:已有重复
 */
int insert_sym_table(sdr_conv_env_t *penv , char *sym_name , int index)
{
	int i;
	int pos;
	int ret = -1;
	sym_entry_t *pentry = NULL;
	sym_entry_t *ptmp = NULL;

	/***Arg Check*/
	if(!penv || !sym_name || strlen(sym_name)<=0 || index<=0)
		return -1;

	if(penv->psym_table->count >= penv->max_node)
	{
		printf("memory error! max Node Count:%d is not enough!\n"  , penv->max_node);
		printf("Please use 'sdrconv -s number -I inputfile' to specify max node\n");
		return -1;
		/*
		sdr_print_info(penv , SDR_INFO_MAIN , "<%s> Symtable is Full!" , __FUNCTION__);
		ret = extend_max_node(penv->max_node , penv);
		if(ret < 0)
		{
			sdr_print_info(penv , SDR_INFO_ERR , "<%s> failed! extend max node failed!" , __FUNCTION__);
			return -1;
		}*/
	}

	/***Handle*/
	//1.查找是否重名
	//先查看目标处是否未被使用
	pos = BKDRHash(sym_name) % penv->max_node;
	if(strcmp(penv->psym_table->entry_list[pos].sym_name , sym_name) == 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "<%s> Dup Define of %s" , __FUNCTION__ , sym_name);
		return -2;
	}

	//搜索所有
	pentry = penv->psym_table->entry_list[pos].next;
	while(pentry)
	{
		if(strcmp(pentry->sym_name , sym_name) == 0)
		{
			sdr_print_info(penv , SDR_INFO_ERR , "<%s> Dup Define of %s" , __FUNCTION__ , sym_name);
			return -2;
		}
		pentry = pentry->next;
	}
	/*
	for(i=0; i<penv->max_node; i++)
	{
		if(strcmp(penv->psym_table->entry_list[i].sym_name , sym_name) == 0)
		{
			sdr_print_info(penv , SDR_INFO_ERR , "Dup Define of %s" , sym_name);
			return -2;
		}
	}*/

	//2.插入
	if(penv->psym_table->entry_list[pos].index <= 0)
	{
		penv->psym_table->entry_list[pos].index = index;
		strncpy(penv->psym_table->entry_list[pos].sym_name , sym_name , SDR_NAME_LEN);
		penv->psym_table->entry_list[pos].next = NULL;
		penv->psym_table->count++;
		return 0;
	}

	//2.1已被占用则找到拉链尾
	pentry = &penv->psym_table->entry_list[pos];
	while(1)
	{
		if(pentry->next == NULL) //pentry is tail of linked-list
			break;

		pentry = pentry->next;
	}

	//2.2new entry
	ptmp = (sym_entry_t *)calloc(1 , sizeof(sym_entry_t));
	if(!ptmp)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "<%s> Fail calloc of %s. err:%s" , __FUNCTION__ , sym_name , strerror(errno));
		return -1;
	}
	ptmp->index = index;
	strncpy(ptmp->sym_name , sym_name , SDR_NAME_LEN);

	//2.3 chained
	pentry->next = ptmp;
	penv->psym_table->count++;

	/*
	//2.1已被占用则从0顺序查找一个
	for(i=0; i<penv->max_node; i++)
	{
		if(penv->psym_table->entry_list[i].index <= 0)
		{
			penv->psym_table->entry_list[i].index = index;
			strncpy(penv->psym_table->entry_list[i].sym_name , sym_name , SDR_NAME_LEN);
			penv->psym_table->count++;
			return 0;
		}
	}
	return -1;
	*/
	return 0;
}

/*
 * 获取一个名字在node_map中的序号
 * <0:error
 * else:序号
 */
int fetch_sym_map_index(sdr_conv_env_t *penv , char *sym_name)
{
	int i;
	int pos;
	sym_entry_t *pentry = NULL;

	/***Arg Check*/
	if(!penv || !sym_name || strlen(sym_name)<=0)
		return -1;

	//先查看目标处
	pos = BKDRHash(sym_name) % penv->max_node;
	if(strcmp(penv->psym_table->entry_list[pos].sym_name , sym_name) == 0)
	{
		return penv->psym_table->entry_list[pos].index;
	}

	//查找拉链
	pentry = penv->psym_table->entry_list[pos].next;
	while(pentry)
	{
		if(strcmp(pentry->sym_name , sym_name) == 0)
			return pentry->index;

		pentry = pentry->next;
	}
	/*
	for(i=0; i<penv->max_node; i++)
	{
		if(strcmp(penv->psym_table->entry_list[i].sym_name , sym_name) == 0)
		{
			return penv->psym_table->entry_list[i].index;
		}
	}*/

	return -1;
}

/*
 * 根据当前值，获得下一个hash size
 */
int get_next_hash_size(int curr_value)
{
	int i = 0;

	for(i=0; i<(sizeof(hash_size_map)/sizeof(int)); i++)
	{
		if(hash_size_map[i] > curr_value)
			return hash_size_map[i];
	}

	//wtf so many entries?
	printf("<%s> failed! can not find bigger than %d\n" , __FUNCTION__ , curr_value);
	return -1;
}

/*********************STATIC FUNCTION*/
/*
 * 分析entry
 * penv->working_line所存储的为待分析的第一行数据
 * @pparent:父节点
 * return:
 * NULL:ERROR
 * Else:当前分析的节点指针
 */
static sdr_node_t *sdr_parse_entry(sdr_conv_env_t *penv , sdr_node_t *pparent)
{
	sdr_node_t *pnode = NULL;
	sdr_node_t *pnode_tmp = NULL;
	char place;
	int index;
	char attr_value[SDR_DESC_LEN] = {0};
	int ret;
	char *start;

	/***Arg Check*/
	if(!penv)
		return NULL;

	//检查<entry
	if(penv->working_line[0] != XML_LEFT_BRACKET)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Entry Format");
		return NULL;
	}
	index = 1 + strlen(XML_LABEL_ENTRY);	//strlen("<macro")
	place = penv->working_line[index];
	penv->working_line[index] = 0;
	if(strcmp(&penv->working_line[1] , XML_LABEL_ENTRY) != 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Wrong Entry Format");
		return NULL;
	}

	penv->working_line[index] = place;
	copy_str(penv->working_line , &penv->working_line[index]);
	/***Handle*/
	//1.获取Node
	pnode = get_node(penv);
	if(!pnode)
		return NULL;

	pnode->class = SDR_CLASS_ENTRY;
	//2.获取属性标签
	//name
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_NAME , pnode->node_name);
	if(ret < 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Failed to Get Attr '%s'" , XML_LABEL_NAME);
		return NULL;
	}

	//type
	memset(attr_value , 0 , sizeof(attr_value));
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_TYPE , attr_value);
	if(ret < 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Failed to Get Attr '%s'" , XML_LABEL_TYPE);
		return NULL;
	}
	pnode->data.entry_value.entry_type = converse_label_type(attr_value , &pnode->size);
	if(pnode->data.entry_value.entry_type == SDR_T_COMPOS)	//不是基本类型，还需要进一步分析
	{
		//先找到类型index
		index = fetch_sym_map_index(penv , attr_value);
		if(index <= 0)
		{
			sdr_print_info(penv , SDR_INFO_ERR , "Type '%s' Unknown" , attr_value);
			return NULL;
		}

		//获得类型节点
		pnode_tmp = (sdr_node_t *)&penv->pnode_map->node_list[index];
		if(pnode_tmp->my_idx <= 0 || (pnode_tmp->class!=SDR_CLASS_STRUCT && pnode_tmp->class!=SDR_CLASS_UNION))
		{
			sdr_print_info(penv , SDR_INFO_ERR , "Type '%s' Info Wrong" , attr_value);
			return NULL;
		}

		//进行赋值
		if(pnode_tmp->class == SDR_CLASS_STRUCT)
			pnode->data.entry_value.entry_type = SDR_T_STRUCT;
		else
			pnode->data.entry_value.entry_type = SDR_T_UNION;

		pnode->size = pnode_tmp->size;
		pnode->data.entry_value.type_idx = pnode_tmp->my_idx;

		//union不能直接嵌套，因为需要指定其他成员select
		if(pnode->data.entry_value.entry_type==SDR_T_UNION && pparent->class==SDR_CLASS_UNION)
		{
			sdr_print_info(penv , SDR_INFO_ERR , "Type 'Union' Can not be Nested Directly!");
			return NULL;
		}
	}


	//count
	memset(attr_value , 0 , sizeof(attr_value));
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_COUNT , attr_value);
	if(ret < 0)	//没有这个属性
		pnode->data.entry_value.count = 1;
	else
	{
		start = get_macro_value(penv , attr_value);
		if(!start)	//这个MACRO值没有找到
		{
			sdr_print_info(penv , SDR_INFO_ERR , "No Define Macro '%s'" , attr_value);
			return NULL;
		}
		pnode->data.entry_value.count = atoi(start);
		strncpy(pnode->data.entry_value.count_name , attr_value , sizeof(pnode->data.entry_value.count_name));
	}

	//refer 只有结构体才有 而且只能为基本非浮点类型
	if(pparent->class == SDR_CLASS_STRUCT)
	{
		memset(attr_value , 0 , sizeof(attr_value));
		ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_REFER , attr_value);
		if(ret == 0)
		{
			//1.找到refer的成员指针
			pnode_tmp = entry_of_struct(penv , pparent , attr_value);
			if(!pnode_tmp)
			{
				sdr_print_info(penv , SDR_INFO_ERR , "No Refer Member '%s' of struct %s Can be Found!" , attr_value , pparent->node_name);
				return NULL;
			}

			//1.5检查类型(基本非浮点类型)
			if(pnode_tmp->data.entry_value.entry_type<SDR_T_CHAR || pnode_tmp->data.entry_value.entry_type>=SDR_T_FLOAT)
			{
				sdr_print_info(penv , SDR_INFO_ERR , "Refer Type Must be Basic None Float Type!");
				return NULL;
			}

			//2.设置refer的index
			pnode->data.entry_value.refer_idx = pnode_tmp->my_idx;
		}
	}

	//select 只有父节点为结构体；子entry为union才会存在
	if(pparent->class == SDR_CLASS_STRUCT && pnode->data.entry_value.entry_type == SDR_T_UNION)
	{
		memset(attr_value , 0 , sizeof(attr_value));
		ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_SELECT , attr_value);
		if(ret == 0)
		{
			//1.找到select的成员指针
			pnode_tmp = entry_of_struct(penv , pparent , attr_value);
			if(!pnode_tmp)
			{
				sdr_print_info(penv , SDR_INFO_ERR , "No Select Member '%s' of struct %s Can be Found!" , attr_value , pparent->node_name);
				return NULL;
			}

			//1.5检查类型(基本非浮点类型)
			if(pnode_tmp->data.entry_value.entry_type<SDR_T_CHAR || pnode_tmp->data.entry_value.entry_type>=SDR_T_FLOAT)
			{
				sdr_print_info(penv , SDR_INFO_ERR , "Select Type Must be Basic None Float Type!");
				return NULL;
			}

			//2.设置refer的index
			pnode->data.entry_value.select_idx = pnode_tmp->my_idx;
		}
		else
		{
			sdr_print_info(penv , SDR_INFO_ERR , "Entry Type is Union ,Must Specify Attr '%s'" , XML_LABEL_SELECT);
			return NULL;
		}
	}

	//id 只有union才有
	if(pparent->class == SDR_CLASS_UNION)
	{
		memset(attr_value , 0 , sizeof(attr_value));
		ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_ID , attr_value);
		if(ret < 0)
		{
			sdr_print_info(penv , SDR_INFO_ERR , "Entry %s of Union %s Must Specify Attr '%s'" , pnode->node_name , pparent->node_name , XML_LABEL_ID);
			return NULL;
		}

		start = get_macro_value(penv , attr_value);
		if(!start)	//这个MACRO值没有找到
		{
			sdr_print_info(penv , SDR_INFO_ERR , "No Define Macro '%s'" , attr_value);
			return NULL;
		}
		pnode->data.entry_value.select_id = atoi(start);
		strncpy(pnode->data.entry_value.id_name , attr_value , SDR_NAME_LEN);
	}

	//version
	memset(attr_value , 0 , sizeof(attr_value));
	ret = fetch_attr_value(penv , penv->working_line , XML_LABEL_VERSION , attr_value);
	if(ret == 0)
	{
		pnode->version = atoi(attr_value);
	}
	//desc
	fetch_attr_value(penv , penv->working_line , XML_LABEL_DESC , pnode->node_desc);

	/***检查括弧是否封闭 />*/
	start = strrchr(penv->working_line , '>');
	if(!start)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "No '%c' at the end of line" , XML_RIGHT_BRACKET);
		return NULL;
	}
	start--;
	if(start[0] != '/')
	{
		sdr_print_info(penv , SDR_INFO_ERR , "No Closed of Entry at the end of line" , XML_RIGHT_BRACKET);
		return NULL;
	}
	return pnode;
}

/*
 * 在src中提取attr_name的值，并存储于attr_value中
 * 属性格式attr_name="attr_value"
 * @src:源字串
 * @attr_name:属性名称
 * @attr_value:返回的属性值
 * @return:
 * 0:成功
 * -1:失败
 */
static int fetch_attr_value(sdr_conv_env_t *penv , char *src , char *attr_name , char *attr_value)
{
	char replace;
	int pos;
	int ret;

	char *start;
	char *end;

	char buff[SDR_LINE_LEN] = {0};
	char format_buff[SDR_LINE_LEN] = {0};
	/***Arg Check*/
	if(!src || !attr_name || !attr_value)
		return -1;
	if(strlen(src)<=0 || strlen(attr_name)<=0)
		return -1;

	strncpy(buff , src , SDR_LINE_LEN);
	/***Handle*/
	do
	{
		//寻找'attr_name='模板
		snprintf(format_buff , sizeof(format_buff) , "%s=" , attr_name);
		start = strstr(buff , format_buff);
		if(!start)
			return -1;

		copy_str(buff , start+strlen(format_buff));

		//获取属性值
		//匹配起始"
		start = strchr(buff , '"');
		if(!start)//没找到起始 "
		{
			sdr_print_info(penv , SDR_INFO_ERR , "No Attr Value");
			return -1;
		}

		//匹配结束"
		end = strchr(&start[1] , '"');
		if(!end)//没找到结束 "
		{
			sdr_print_info(penv , SDR_INFO_ERR , "No Attr Value");
			return -1;
		}
		end[0] = 0;

		//拷贝
		strncpy(attr_value , &start[1] , SDR_NAME_LEN);
		break;
	}while(1);

	return 0;
}

/*
 * 将type类型名翻译为对应的宏,并返回基本类型的大小
 * -1:错误
 * >=SDR_T_CHAR 基本类型
 * ELSE:复合类型
 */
static int converse_label_type(char *label_type , int *size)
{
	int type;
	/***Arg Check*/
	if(!label_type || strlen(label_type)<=0)
		return -1;

	/***Handle*/
	do
	{
		if(strcmp(label_type , XML_LABEL_CHAR) == 0)
		{
			type = SDR_T_CHAR;
			*size = sizeof(char);
			break;
		}
		if(strcmp(label_type , XML_LABEL_UCHAR) == 0)
		{
			type = SDR_T_UCHAR;
			*size = sizeof(char);
			break;
		}
		if(strcmp(label_type , XML_LABEL_SHORT) == 0)
		{
			type = SDR_T_SHORT;
			*size = sizeof(short);
			break;
		}
		if(strcmp(label_type , XML_LABEL_USHORT) == 0)
		{
			type = SDR_T_USHORT;
			*size = sizeof(short);
			break;
		}
		if(strcmp(label_type , XML_LABEL_INT) == 0)
		{
			type = SDR_T_INT;
			*size = sizeof(int);
			break;
		}
		if(strcmp(label_type , XML_LABEL_UINT) == 0)
		{
			type = SDR_T_UINT;
			*size = sizeof(int);
			break;
		}
		if(strcmp(label_type , XML_LABEL_LONG) == 0)
		{
			type = SDR_T_LONG;
			*size = sizeof(long);
			break;
		}
		if(strcmp(label_type , XML_LABEL_ULONG) == 0)
		{
			type = SDR_T_ULONG;
			*size = sizeof(long);
			break;
		}
		if(strcmp(label_type , XML_LABEL_FLOAT) == 0)
		{
			type = SDR_T_FLOAT;
			*size = sizeof(float);
			break;
		}
		if(strcmp(label_type , XML_LABEL_DOUBLE) == 0)
		{
			type = SDR_T_DOUBLE;
			*size = sizeof(double);
			break;
		}
		if(strcmp(label_type , XML_LABEL_LONGLONG) == 0)
	   {
			type = SDR_T_LONGLONG;
			*size = sizeof(long long);
			break;
		}

		*size = 0;
		type = SDR_T_COMPOS;
	}while(0);

	return type;
}



/*
 * 获取MACRO的字符串值
 * 如果MACRO以'-'或者[0,9]开始，则是直接赋值不用查找macro对应的值
 */
static char *get_macro_value(sdr_conv_env_t *penv , char *macro_name)
{
	sdr_node_t *pmacro_node;
	int macro_index;
	static char macro_value[MACRO_VALUE_LEN] = {0};

	if(!macro_name || strlen(macro_name)<=0)
		return NULL;

	memset(macro_value , 0 , sizeof(macro_value));
	/***转换*/
	if(macro_name[0]=='-' || (macro_name[0]>='0' && macro_name[0]<='9')) //如果以负号或者数字开头，则直接拷贝，不用找宏定义
		strncpy(macro_value , macro_name , sizeof(macro_value));
	else	//查找宏定义
	{
		macro_index = fetch_sym_map_index(penv , macro_name);
		if(macro_index < 0)
			return NULL;

		//得到相应的NODE
		pmacro_node = (sdr_node_t *)&penv->pnode_map->node_list[macro_index];
		if(pmacro_node->class != SDR_CLASS_MACRO)
			return NULL;

		strncpy(macro_value , pmacro_node->data.macro_value , sizeof(macro_value));
//		snprintf(macro_value , sizeof(macro_value) , "%d" , 1986);
	}
	return macro_value;
}



/*
 * 根据成员名来获取某结构体的成员entry
 * @return:
 * NULL:Failed
 * Else:entry 节点指针
 */
static sdr_node_t *entry_of_struct(sdr_conv_env_t *penv , sdr_node_t *pparent , char *entry_name)
{
	sdr_node_t *pnode = NULL;

	/***Arg Check*/
	if(!penv || !pparent || !entry_name || strlen(entry_name)<=0)
		return NULL;

	if(pparent->class != SDR_CLASS_STRUCT)
		return NULL;
	if(pparent->data.struct_value.child_idx <= 0)
		return NULL;

	pnode = (sdr_node_t *)&penv->pnode_map->node_list[pparent->data.struct_value.child_idx];
	/***Handle*/
	while(pnode)
	{
		//名字匹配
		if(strcmp(pnode->node_name , entry_name) == 0)
			return pnode;

		//没有兄弟节点
		if(pnode->sibling_idx <= 0)
			break;

		//查找兄弟节点
		pnode = (sdr_node_t *)&penv->pnode_map->node_list[pnode->sibling_idx];
	}

	return NULL;
}


/*
 * 根据成员ID来获取某union的成员entry
 * @return:
 * NULL:Failed
 * Else:entry 节点指针
 */
static sdr_node_t *entry_of_union(sdr_conv_env_t *penv , sdr_node_t *pparent , int select_id)
{
	sdr_node_t *pnode = NULL;

	/***Arg Check*/
	if(!penv || !pparent || select_id<0)
		return NULL;

	if(pparent->class != SDR_CLASS_UNION)
		return NULL;
	if(pparent->data.union_value.child_idx <= 0)
		return NULL;

	pnode = (sdr_node_t *)&penv->pnode_map->node_list[pparent->data.union_value.child_idx];
	/***Handle*/
	while(pnode)
	{
		//id匹配
		if(pnode->data.entry_value.select_id == select_id)
			return pnode;

		//没有兄弟节点
		if(pnode->sibling_idx <= 0)
			break;

		//查找兄弟节点
		pnode = (sdr_node_t *)&penv->pnode_map->node_list[pnode->sibling_idx];
	}

	return NULL;
}

/*
 * 打印某个struct节点的头文件
 * @pnode:当前struct节点
 */
static int gen_struct_h(sdr_conv_env_t *penv , sdr_node_t *pnode , FILE *fp)
{
	int ret;
	sdr_node_t *pentry_node = NULL;

	/***Arg Check*/
	if(!penv || !pnode || !fp)
		return -1;
	if(pnode->class != SDR_CLASS_STRUCT)
		return -1;
	/***Handle*/
	//1.打印struct头
	fprintf(fp , "struct _%s\n{\n" , pnode->node_name);

	//2.依次打印其成员
	if(pnode->data.struct_value.child_idx <= 0)
		pentry_node = NULL;
	else
		pentry_node = (sdr_node_t *)&penv->pnode_map->node_list[pnode->data.struct_value.child_idx];

	while(pentry_node)
	{
		//打印该entry node 的信息
		ret = gen_entry_h(penv , pentry_node , fp);
		if(ret < 0)
			return -1;

		//获取该entry_node的兄弟
		if(pentry_node->sibling_idx <= 0)
			break;

		pentry_node = (sdr_node_t *)&penv->pnode_map->node_list[pentry_node->sibling_idx];
	}

	//3.打印struct尾
	fprintf(fp , "}__attribute__((packed));\n");
	fprintf(fp , "typedef struct _%s %s_t;\n\n" , pnode->node_name , pnode->node_name);
	fflush(fp);
	return 0;
}

/*
 * 打印某个union节点的头文件
 *@pnode:当前union节点
 */
static int gen_union_h(sdr_conv_env_t *penv , sdr_node_t *pnode , FILE *fp)
{
	int ret;
	sdr_node_t *pentry_node = NULL;

	/***Arg Check*/
	if(!penv || !pnode || !fp)
		return -1;
	if(pnode->class != SDR_CLASS_UNION)
		return -1;
	/***Handle*/
	//1.打印union头
	fprintf(fp , "union _%s\n{\n" , pnode->node_name);

	//2.依次打印其成员
	if(pnode->data.union_value.child_idx <= 0)
		pentry_node = NULL;
	else
		pentry_node = (sdr_node_t *)&penv->pnode_map->node_list[pnode->data.union_value.child_idx];

	while(pentry_node)
	{
		//打印该entry node 的信息
		ret = gen_entry_h(penv , pentry_node , fp);
		if(ret < 0)
			return -1;

		//获取该entry_node的兄弟
		if(pentry_node->sibling_idx <= 0)
			break;

		pentry_node = (sdr_node_t *)&penv->pnode_map->node_list[pentry_node->sibling_idx];
	}

	//3.打印union尾
	fprintf(fp , "}__attribute__((packed));\n");
	fprintf(fp , "typedef union _%s %s_t;\n\n" , pnode->node_name , pnode->node_name);
	fflush(fp);
	return 0;
}

/*
 * 打印某个entry节点的头文件
 * @pnode:当前entry节点
 */
static int gen_entry_h(sdr_conv_env_t *penv , sdr_node_t *pnode , FILE *fp)
{
	char *start = NULL;
	sdr_node_t *ptype_node;
//	char type_name[MAX_NAME_LEN] ={0};

	/***Arg Check*/
	if(!penv || !pnode || !fp)
		return -1;

	if(pnode->class != SDR_CLASS_ENTRY)
		return -1;

	/***Handle*/
	fprintf(fp , "    "); //four space
	//1.type name
	if(pnode->data.entry_value.entry_type==SDR_T_STRUCT || pnode->data.entry_value.entry_type==SDR_T_UNION)	//复合类型
	{
		ptype_node = (sdr_node_t *)&penv->pnode_map->node_list[pnode->data.entry_value.type_idx];
		fprintf(fp , "%s_t %s" , ptype_node->node_name , pnode->node_name);
	}
	else //基本类型
	{
//		memset(type_name , 0 , sizeof(type_name));
		start = reverse_label_type(pnode->data.entry_value.entry_type , NULL);
		if(!start)
		{
			printf("Gen Entry Failed! Get Entry '%s' Type Failed\n" , pnode->node_name);
			return -1;
		}
//		strncpy(type_name , start , sizeof(type_name));
		fprintf(fp , "%s %s" , start , pnode->node_name);
	}

	//2.[count];
	if(pnode->data.entry_value.count==0 || pnode->data.entry_value.count>1)
		fprintf(fp , "[%s]" , pnode->data.entry_value.count_name);
	fprintf(fp , ";");

	//3.version[desc]
	if(pnode->version>0 || strlen(pnode->node_desc)>0 || strlen(pnode->data.entry_value.id_name))
	{
		fprintf(fp , "//");
	}
	if(pnode->version > 0)
		fprintf(fp , "Ver.%d" , pnode->version);
	fprintf(fp , " %s  %s\n" , pnode->node_desc , pnode->data.entry_value.id_name);
	fflush(fp);
	return 0;
}

#if 0
/*
 * 被废弃
 * 扩展node上限
 * 同时需要重构node_map和sym_table,它们都受到max_node约束
 * return 0 success; -1 failed
 * 由于在解析过程中该函数会重新开辟sym&node地址，所以之前分配且在使用的node地址就会被丢弃
 * 无法完成解析过程。
 */
static int extend_max_node(int max_node , sdr_conv_env_t *penv)
{
	int next_size = -1;
	sdr_node_map_t *pmap = NULL;
	sym_table_t *psym = NULL;
	int i = 0;

	/***Arg Check*/
	if(!penv)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "<%s> failed! arg NULL" , __FUNCTION__);
		return -1;
	}

	/***Handle*/
	//1. get next
	next_size = get_next_hash_size(max_node);
	if(next_size < 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "<%s> failed! get next_size failed! max_node:%d" , __FUNCTION__ , max_node);
		return -1;
	}

	//2.malloc new memory
	pmap = (sdr_node_map_t *)calloc(1 , sizeof(sdr_node_map_t) + next_size*sizeof(sdr_node_t));
	if(!pmap)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "<%s> failed! malloc new node_map fail, err:%s" , __FUNCTION__ , strerror(errno));
		return -1;
	}

	psym = (sym_table_t *)calloc(1 , sizeof(sym_table_t) + next_size*sizeof(sym_entry_t));
	if(!psym)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "<%s> failed! malloc new sym_table fail, err:%s" , __FUNCTION__ , strerror(errno));
		free(pmap);
		return -1;
	}

	//3.copy node_map
	pmap->count = penv->pnode_map->count;
	memcpy(&pmap->node_list[0] , &penv->pnode_map->node_list[0] , pmap->count*sizeof(sdr_node_t));

	//4.copy sym table
	psym->count = penv->psym_table->count;
		//原有指针链直接copy即可，不用释放
	memcpy(&psym->entry_list[0] , &penv->psym_table->entry_list[0] , max_node * sizeof(sym_entry_t));

	sdr_print_info(penv , SDR_INFO_MAIN , "<%s> success! old node:%d old map:%lx old sym:%lx , new node:%d , new map:%lx new sym:%lx" ,
			__FUNCTION__ , max_node , penv->pnode_map , penv->psym_table , next_size , pmap , psym);

	//5.free old
	free(penv->pnode_map);
	free(penv->psym_table);

	//6.reset
	penv->max_node = next_size;
	penv->pnode_map = pmap;
	penv->psym_table = psym;

	penv->sdr_res.max_node = next_size;
	penv->sdr_res.pnode_map = pmap;
	penv->sdr_res.psym_table = psym;

	return 0;
}
#endif

/*
 * 将离散的符号表压缩成紧凑表，并返回对应的紧表指针
 * 使用之后注意free
 */
static packed_sym_table_t *pack_sym_table(sdr_conv_env_t *penv)
{
	sym_table_t *psym_table = NULL;
	packed_sym_table_t *ppacked_sym_table = NULL;
	sym_entry_t *pentry = NULL;
	int i = 0;
	int checked = 0;
	if(!penv)
	{
        printf("%s failed! penv NULL." , __FUNCTION__);
        return NULL;
	}

	/***Init*/
	psym_table = penv->psym_table;

	//malloc mem
	ppacked_sym_table = calloc(1 , sizeof(packed_sym_table_t) + psym_table->count*sizeof(packed_sym_entry_t));
	if(!ppacked_sym_table)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "<%s> failed! calloc packed_sym_table failed! err:%s" , __FUNCTION__ , strerror(errno));
		return NULL;
	}

	ppacked_sym_table->sym_list_size = penv->max_node;
	//for each sym_entry
	for(i=0; i<penv->max_node && checked<psym_table->count; i++)
	{
		if(psym_table->entry_list[i].index<=0)
			continue;

		//set packed
		ppacked_sym_table->entry_list[checked].pos = i;
		ppacked_sym_table->entry_list[checked].index = psym_table->entry_list[i].index;
		memcpy(ppacked_sym_table->entry_list[checked].sym_name , psym_table->entry_list[i].sym_name , SDR_NAME_LEN);
		checked++;

		//search chain list
		pentry = psym_table->entry_list[i].next;
		while(pentry)
		{
			ppacked_sym_table->entry_list[checked].pos = i;
			ppacked_sym_table->entry_list[checked].index = pentry->index;
			memcpy(ppacked_sym_table->entry_list[checked].sym_name , pentry->sym_name , SDR_NAME_LEN);
			//update
			checked++;
			pentry = pentry->next;
		}

	}

	ppacked_sym_table->my_list_size = checked;
	return ppacked_sym_table;
}
