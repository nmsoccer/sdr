/*
 * sdr.c
 *
 *  Created on: 2015-3-18
 *      Author: soullei
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

#include "sdr.h"

/************LOCAL FUNCTION*/
static int gen_struct_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp);
static int gen_union_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp);
static int gen_entry_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp);

static int pack_struct_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , FILE *log_fp);
static int pack_union_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , FILE *log_fp , int select_id);
static int unpack_struct_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , FILE *log_fp);
static int unpack_union_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , FILE *log_fp , int select_id);

static int get_sym_map_index(sdr_data_res_t *pres , char *sym_name);
static int get_base_type_size(char type);
static int print_info(char type , FILE *fp , char *fmt , ...);
static sdr_node_t *fetch_node_by_index(sdr_data_res_t *pres , int iIndex);
static sdr_node_t *fetch_node_entry_of_type(sdr_data_res_t *pres , char *type_name , char *member_name , int *perr_code);

/************GLOBAL FUNCTION*/
/*
 * 加载目标bin文件入内存
 */
sdr_data_res_t *sdr_load_bin(char *file_name , FILE *log_fp)
{
	int fd;
	sdr_data_res_t *pres = NULL;
	char *start;
	char buff[32] = {0};
	int max_node;
	int map_count;
	int size;
	int ret;

	/***Arg Check*/
	if(!file_name || strlen(file_name)<=0)
	{
		print_info(INFO_ERR , log_fp , "File name is empty!");
		return NULL;
	}

	/***初始化检查*/
	//1.打开目标文件
	fd = open(file_name , O_RDONLY , S_IRUSR);
	if(fd < 0)
	{
		print_info(INFO_ERR , log_fp , "Can not Open file %s" , file_name);
		return NULL;
	}

	//2.读入magic+maxnode
	ret = read(fd , buff , strlen(SDR_MAGIC_STR));
	if(ret < 0)
	{
		print_info(INFO_ERR , log_fp , "Read Magic Failed!");
		return NULL;
	}
	if(strcmp(buff , SDR_MAGIC_STR) != 0)
	{
		print_info(INFO_ERR , log_fp , "Not a regular sdr bin file!");
		return NULL;
	}

	ret = read(fd , &max_node , sizeof(max_node));
	if(ret < 0)
	{
		print_info(INFO_ERR , log_fp , "Read Max node Failed!");
		return NULL;
	}
	if(max_node <= 0)
	{
		print_info(INFO_ERR , log_fp , "max node <= 0");
		return NULL;
	}

	//3.读入sdr_nod_map
	memset(buff , 0 , strlen(buff));
	ret = read(fd , buff , sizeof(sdr_node_map_t));
	if(ret < 0)
	{
		print_info(INFO_ERR , log_fp , "Read sdr node map Failed!");
		return NULL;
	}
	map_count = ((sdr_node_map_t *)buff)->count;
	if(map_count <= 0)
	{
		print_info(INFO_ERR , log_fp , "map count <= 0");
		return NULL;
	}

	//4.文件读位置指向sdr_nod_map
	lseek(fd , 0-sizeof(sdr_node_map_t) , SEEK_CUR);

	/***Handle*/
	//1.分配内存
	size = sizeof(sdr_data_res_t) + sizeof(sdr_node_map_t) + map_count*sizeof(sdr_node_t) + sizeof(sym_table_t) + max_node*sizeof(sym_entry_t);
	pres = (sdr_data_res_t *)malloc(size);
	memset(pres , 0 , size);
	if(!pres)
	{
		print_info(INFO_ERR , log_fp , "malloc mem failed!");
		return NULL;
	}

	//2.读入
	start = (char *)pres + sizeof(sdr_data_res_t);
	size -= sizeof(sdr_data_res_t);
	ret = read(fd , start , size);
	if(ret < 0 || ret!=size)
	{
		print_info(INFO_ERR , log_fp , "read node map and sym table failed!");
		return NULL;
	}

	//3.设置pres
	strncpy(pres->magic , SDR_MAGIC_STR , strlen(SDR_MAGIC_STR));
	pres->max_node = max_node;
	pres->pnode_map = (sdr_node_map_t *)start;
	pres->psym_table = (sym_table_t *)(start + sizeof(sdr_node_map_t) + map_count*sizeof(sdr_node_t));

	print_info(INFO_NORMAL , log_fp , "max node:%d, map_count:%d , entry_count:%d" , max_node , pres->pnode_map->count , pres->psym_table->count);
	return pres;
}

/*
 * 释放加载入内存的bin文件
 */
int sdr_free_bin(sdr_data_res_t *pres)
{
	if(!pres)
		return -1;

	free(pres);
	return 0;
}

/*
 * 打印出bin文件内容为xml格式
 */
int sdr_bin2xml(sdr_data_res_t *pres , char *file_name , FILE *log_fp)
{
	FILE *fp;
	sdr_node_t *pnode;
	int i;
	int ret;
	time_t timep;
	struct tm *ptm;

	/***Arg Check*/
	if(!pres || !file_name || strlen(file_name)<=0)
		return -1;

	//打开文件
	fp = fopen(file_name, "w+");
	if(!fp)
	{
		print_info(INFO_ERR , log_fp , "Open  file %s Failed!" , file_name);
		return -1;
	}

	/***Handle*/
	//1.写入文件头
	fprintf(fp , "<?xml version=\"1.0\" encoding=\"utf8\" ?>\n");
	/*打印时间*/
	timep = time(NULL);
	ptm = localtime(&timep);
	fprintf(fp , "<!-- Created by sdrconv. @%d-%d-%d %d:%d:%d  -->\n\n" , ptm->tm_year+1900 , ptm->tm_mon+1 , ptm->tm_mday , ptm->tm_hour ,
			ptm->tm_min , ptm->tm_sec);

	//2.按序输出每个主节点的内容
	for(i=0; i<pres->pnode_map->count; i++)
	{
		pnode = (sdr_node_t *)&pres->pnode_map->node_list[i];

		//根据不同主节点类型
		switch(pnode->class)
		{
		case SDR_CLASS_START:	//start 和 entry 不解析
		case SDR_CLASS_ENTRY:
			break;

		case SDR_CLASS_MACRO:	//打印macro
			fprintf(fp , "<%s %s=\"%s\" %s=\"%s\" " , XML_LABEL_MACRO , XML_LABEL_NAME , pnode->node_name , XML_LABEL_VALUE , pnode->data.macro_value);
			if(strlen(pnode->node_desc) > 0)
				fprintf(fp , "%s=\"%s\" />\n" , XML_LABEL_DESC , pnode->node_desc);
			else
				fprintf(fp , "/>\n");

			break;
		case SDR_CLASS_STRUCT:	//打印struct
			ret = gen_struct_xml(pres, pnode , fp);
			if(ret < 0)
			{
				printf("generate struct %s failed!\n" , pnode->node_name);
				return -1;
			}
			break;
		case SDR_CLASS_UNION:	//打印union
			ret = gen_union_xml(pres , pnode , fp);
			if(ret < 0)
			{
				printf("generate union %s failed!\n" , pnode->node_name);
				return -1;
			}
			break;
		}

	}

	//3.close
	fflush(fp);
	fclose(fp);
	return 0;
}


/*
 * 打包in_buff数据到out_buff缓冲区
 * 只打包版本号小于version的数据
 * @type_name 类型名
 * out_buff前八个字节为两个整形 分别是version+length
 * return:>=0压缩后的数据
 * else:错误
 */
int sdr_pack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , int version , FILE *log_fp)
{
	sdr_node_t *pnode;
	sdr_buff_info_t sdr_in;
	sdr_buff_info_t sdr_out;
	int index;
	int ret;

	int len;
//	char type_name[MAX_NAME_LEN];

	/***Arg Check*/
	if(!pres || !pout_buff || !pin_buff || version<0 || !type_name)
		return -1;

	/***Init*/
	//1.
	memset(&sdr_in , 0 , sizeof(sdr_buff_info_t));
	memset(&sdr_out , 0 , sizeof(sdr_buff_info_t));
	sdr_out.src = &pout_buff[8];
	sdr_in.src = pin_buff;

	//1.5修改type_name：兼容type_name_t或者type_name
	/*
	strncpy(type_name , stype_name , MAX_NAME_LEN);

	len = strlen(stype_name);
	if(len >= 3)	//xx_t至少三个字节
	{
		if(stype_name[len-2]=='_' && stype_name[len-1]=='t')
		{
			type_name[len-2] = 0;	//将type_t还原成type
		}
	}*/

	//2.当前结构
	index = get_sym_map_index(pres , type_name);
	if(index <= 0)
	{
		print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! can not find type name '%s'" , type_name);
		return -1;
	}
	pnode = (sdr_node_t *)&pres->pnode_map->node_list[index];

	//大于当前版本
	if(pnode->version > version)
	{
		print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! type '%s' version=%d is larger than curr version.%d" , pnode->node_name , pnode->version , version);
		return -1;
	}

	/***Handle*/
	do
	{
		if(pnode->class == SDR_CLASS_STRUCT)
		{
			ret = pack_struct_node(pres , pnode , &sdr_out , &sdr_in , version , log_fp);
			break;
		}

		if(pnode->class == SDR_CLASS_UNION)
		{
			ret = pack_union_node(pres , pnode , &sdr_out , &sdr_in , version , log_fp , -1);
			break;
		}

		//非复合结构体
		print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! type '%s' is no struct or union" , type_name);
		ret = -1;
	}while(0);

	if(ret == 0)
	{
		printf("pack '%s' success! %d -> %d\n" , type_name , sdr_in.index , sdr_out.index);
		memset(pout_buff , 0 , 8);
		memcpy(pout_buff , &version , sizeof(int));
		memcpy(&pout_buff[4] , &sdr_out.index , sizeof(int));
		return sdr_out.index + 8;	//head(8) + 压缩后数据长度
	}
	else
		return ret;
}

/*
 * 解包in_buff数据到out_buff
 * 只解包版本号小于version的数据
 * @type_name 类型名
 * in_buff前八个字节为两个整形 分别是version+length
 * return:>=0解压缩后的数据
 * else:错误
 */
int sdr_unpack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , FILE *log_fp)
{
	sdr_node_t *pnode;
	sdr_buff_info_t sdr_in;
	sdr_buff_info_t sdr_out;
	int index;
	int ret;
	int version;

	/***Arg Check*/
	if(!pres || !pout_buff || !pin_buff)
		return -1;

	/***Init*/
	//1.
	memset(&sdr_in , 0 , sizeof(sdr_buff_info_t));
	memset(&sdr_out , 0 , sizeof(sdr_buff_info_t));
	sdr_out.src = pout_buff;
	sdr_in.src = &pin_buff[8];

	//2.version + length
	memcpy(&version , pin_buff , sizeof(int));
	memcpy(&sdr_in.length , &pin_buff[4] , sizeof(int));
	print_info(INFO_NORMAL , log_fp , "ready to unpack '%s', version:%d,length:%d" , type_name , version , sdr_in.length);
	if(sdr_in.length == 0)	//数据长度为0
	{
		ret = 0;
		goto _unpack_end;
	}
	//3.获取当前结构
	index = get_sym_map_index(pres , type_name);
	if(index <= 0)
	{
		print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! can not find type name '%s'" , type_name);
		return -1;
	}
	pnode = (sdr_node_t *)&pres->pnode_map->node_list[index];

	//大于待解压版本(说明待解压数据没有当前结构的数据啊)
	if(pnode->version > version)
	{
		print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! type '%s' curr version=%d is smaller than packed version.%d" , pnode->node_name , pnode->version , version);
		return -1;
	}

	/***Handle*/
	do
	{
		if(pnode->class == SDR_CLASS_STRUCT)
		{
			ret = unpack_struct_node(pres , pnode , &sdr_out , &sdr_in , version , log_fp);
			break;
		}

		if(pnode->class == SDR_CLASS_UNION)
		{
			ret = unpack_union_node(pres , pnode , &sdr_out , &sdr_in , version , log_fp , -1);
			break;
		}

		//非复合结构体
		print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! type '%s' is no struct or union" , type_name);
		ret = -1;
	}while(0);

_unpack_end:
	if(ret == 0)
	{
		print_info(INFO_NORMAL , log_fp , "unpack '%s' success! %d->%d", type_name , sdr_in.index , sdr_out.index);
		return sdr_out.index;
	}
	else
		return ret;
}


/*
 * 获得某成员相对父结构之偏移[打包前或者解包后之正常结构]只能是结构体
 * @type_name:结构体名
 * @member_name:成员名
 * @return
 * >=0:偏移量
 * -1:参数错误
 * -2:找不到结构
 * -3:类型错误非结构体
 * -4:找不到成员
 * -5:成员错误
*/
int sdr_member_offset(sdr_data_res_t *pres , char *type_name , char *member_name)
{
	sdr_node_t *pnode = NULL;
	int iValue = -1;

	/***Arg Check*/
	if(!pres || !type_name || !member_name)
		return -1;

	if(strlen(type_name)<=0 || strlen(member_name)<=0)
		return -1;

	/***Handle*/
	pnode = fetch_node_entry_of_type(pres , type_name , member_name , &iValue);
	if(!pnode)
		return iValue;
	else
		return pnode->data.entry_value.offset;
}

/*
 * 获得某结构体当前成员的下一个成员名
 * @type_name:结构体名
 * @curr_member:当前成员名;如果为空则next_member是查找第一个成员
 * @next_member:基于curr_member的下一个成员名
 * @len:缓冲区长度
 * @return:
 * >=0:下一个成员在该结构内之偏移
 * -1:参数错误
 * -2:没有type_name
 * -3:type_name类型错误:只能是结构体
 * -4:curr_member未找到
 * -5:curr_member成员错误
 * -6:len长度不够
 * -7:没有下一个成员
 * -8:下一个成员错误
 */
int sdr_next_member(sdr_data_res_t *pres , char *type_name , char *curr_member , char *next_member , int len)
{
	sdr_node_t *pmain_node = NULL;
	sdr_node_t *pnode = NULL;
	int iIndex = 0;
	int iValue = -1;

	/***Arg Check*/
	if(!pres || !type_name || !next_member || len<=0)
		return- 1;


	/***Handle*/
	//1.curr_member为空 则寻找第一个成员
	if(curr_member == NULL)
	{
		//1.1找主节点
		iIndex = get_sym_map_index(pres , type_name);
		pmain_node = fetch_node_by_index(pres , iIndex);
		if(!pmain_node)
			return -2;

		if(pmain_node->class != SDR_CLASS_STRUCT)
			return -3;

		//1.2.获得目标节点
		pnode = fetch_node_by_index(pres , pmain_node->data.struct_value.child_idx);
	}
	//2.curr_member不为空
	else
	{
		//1.获取curr_member的node
		pnode = fetch_node_entry_of_type(pres , type_name , curr_member , &iValue);
		if(pnode == NULL)
			return iValue;

		//2.获取目标节点
		pnode = fetch_node_by_index(pres , pnode->sibling_idx);
	}

	if(!pnode)
		return -7;

	//成员类型非ENTRY
	if(pnode->class != SDR_CLASS_ENTRY)
		return -8;

	//缓冲区长度不够
	if(len <= strlen(pnode->node_name))
		return -6;

	//拷贝
	strncpy(next_member , pnode->node_name , len);
	return pnode->data.entry_value.offset;
}


static int print_info(char type , FILE *fp , char *fmt , ...)
{
	va_list vp;
	char buff[1024] = {0};

	/***Arg Check*/
	if(!fmt || strlen(fmt)<=0)
		return -1;


	/***Handle*/
	va_start(vp , fmt);

	switch(type)
	{
	case INFO_NORMAL:
		break;
	case INFO_MAIN:
		strncpy(buff , "Main:" , sizeof(buff));
		break;
	case INFO_ERR:
//		strncpy(buff , "Error:" , sizeof(buff));
		break;
	}

	vsnprintf(&buff[strlen(buff)] , sizeof(buff)-strlen(buff) , fmt , vp);
	if(fp)
		fprintf(fp , "%s\n" , buff);
	printf("%s\n" , buff);
	return 0;
}



/*
 * 打印某个struct节点的头文件
 * @pnode:当前struct节点
 */
static int gen_struct_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp)
{
	int ret;
	sdr_node_t *pentry_node = NULL;
	char line_buff[SDR_LINE_LEN] = {0};

	/***Arg Check*/
	if(!pres || !pnode || !fp)
		return -1;
	if(pnode->class != SDR_CLASS_STRUCT)
		return -1;
	/***Handle*/
	//1.打印struct头
	snprintf(line_buff , sizeof(line_buff) , "<%s %s=\"%s\" " , XML_LABEL_STRUCT , XML_LABEL_NAME , pnode->node_name);

	//2.version
	if(pnode->version > 0)
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%d\" >\n" , XML_LABEL_VERSION , pnode->version);
	else
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , ">\n");

	fprintf(fp , "%s" , line_buff);

	//3.依次打印其成员
	if(pnode->data.struct_value.child_idx <= 0)
		pentry_node = NULL;
	else
		pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.struct_value.child_idx];

	while(pentry_node)
	{
		//打印该entry node 的信息
		ret = gen_entry_xml(pres , pentry_node , fp);
		if(ret < 0)
			return -1;

		//获取该entry_node的兄弟
		if(pentry_node->sibling_idx <= 0)
			break;

		pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->sibling_idx];
	}

	//3.打印struct尾
	fprintf(fp ,  "</%s>\n\n" , XML_LABEL_STRUCT);
	fflush(fp);
	return 0;
}

/*
 * 打印某个union节点的头文件
 *@pnode:当前union节点
 */
static int gen_union_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp)
{
	int ret;
	sdr_node_t *pentry_node = NULL;
	char line_buff[SDR_LINE_LEN] = {0};

	/***Arg Check*/
	if(!pres || !pnode || !fp)
		return -1;
	if(pnode->class != SDR_CLASS_UNION)
		return -1;
	/***Handle*/
	//1.打印union头
	snprintf(line_buff , sizeof(line_buff) , "<%s %s=\"%s\" " , XML_LABEL_UNION , XML_LABEL_NAME , pnode->node_name);

	//2.version
		if(pnode->version > 0)
			snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%d\" >\n" , XML_LABEL_VERSION , pnode->version);
		else
			snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , ">\n");

	fprintf(fp , "%s" , line_buff);
	//2.依次打印其成员
	if(pnode->data.union_value.child_idx <= 0)
		pentry_node = NULL;
	else
		pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.union_value.child_idx];

	while(pentry_node)
	{
		//打印该entry node 的信息
		ret = gen_entry_xml(pres , pentry_node , fp);
		if(ret < 0)
			return -1;

		//获取该entry_node的兄弟
		if(pentry_node->sibling_idx <= 0)
			break;

		pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->sibling_idx];
	}

	//3.打印union尾
	fprintf(fp , "</%s>\n\n" , XML_LABEL_UNION);
	fflush(fp);
	return 0;
}

/*
 * 打印某个entry节点的头文件
 * @pnode:当前entry节点
 */
static int gen_entry_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp)
{
	char *start = NULL;
	sdr_node_t *pnode_tmp;
	char line_buff[SDR_LINE_LEN] = {0};

	/***Arg Check*/
	if(!pres|| !pnode || !fp)
		return -1;

	if(pnode->class != SDR_CLASS_ENTRY)
		return -1;

	/***Handle*/
	strncpy(line_buff , "\t<entry " , sizeof(line_buff));

	//1.name
	snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%s\" " , XML_LABEL_NAME , pnode->node_name);

	//2.type name
	if(pnode->data.entry_value.entry_type==SDR_T_STRUCT || pnode->data.entry_value.entry_type==SDR_T_UNION)	//复合类型
	{
		pnode_tmp = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.entry_value.type_idx];
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%s\" " , XML_LABEL_TYPE , pnode_tmp->node_name);
	}
	else //基本类型
	{
		start = reverse_label_type(pnode->data.entry_value.entry_type);
		if(!start)
		{
			print_info(INFO_ERR , NULL , "Gen Entry Failed! Get Entry '%s' Type Failed\n" , pnode->node_name);
			return -1;
		}

		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%s\" " , XML_LABEL_TYPE , start);
	}

	//3.count;
	if(pnode->data.entry_value.count==0 || pnode->data.entry_value.count>1)
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%s\" " , XML_LABEL_COUNT , pnode->data.entry_value.count_name);

	//4.version
	if(pnode->version > 0)
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%d\" " , XML_LABEL_VERSION , pnode->version);

	//5.refer
	if(pnode->data.entry_value.refer_idx > 0)
	{
		pnode_tmp = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.entry_value.refer_idx];
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%s\" " , XML_LABEL_REFER , pnode_tmp->node_name);
	}

	//6.select
	if(pnode->data.entry_value.select_idx > 0)
	{
		pnode_tmp = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.entry_value.select_idx];
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%s\" " , XML_LABEL_SELECT , pnode_tmp->node_name);
	}

	//7.select_id
	if(strlen(pnode->data.entry_value.id_name) > 0)
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%s\" " , XML_LABEL_ID , pnode->data.entry_value.id_name);

	//8.desc
	if(strlen(pnode->node_desc) > 0)
		snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , "%s=\"%s\" " , XML_LABEL_DESC , pnode->node_desc);

	/***结尾*/
	snprintf(&line_buff[strlen(line_buff)] , SDR_LINE_LEN-strlen(line_buff) , " />");
	fprintf(fp , "%s\n" , line_buff);
	fflush(fp);
	return 0;
}


static int pack_struct_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , FILE *log_fp)
{
	sdr_node_t *pentry_node;
	sdr_node_t *ptmp_node;

	int in_start_index;	//当前结构体起始index

	int total_size;	//entry总大小
	unsigned int refer_count = 0;
	unsigned int selected_id = 0;

	int ret;
	int i;

	/***Arg Check*/
	if(!pres || !pnode || !pin || !pout || version<0)
		return -1;

	//不是struct
	if(pnode->class != SDR_CLASS_STRUCT)
		return -1;

	in_start_index = pin->index;
	/***Init*/
	if(pnode->data.struct_value.child_idx <= 0)	//没有子节点
		return 0;
	pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.struct_value.child_idx];

	/***Handle*/
	while(pentry_node)
	{
		//1.检查是否entry节点
		if(pentry_node->class != SDR_CLASS_ENTRY)
		{
			print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! '%s' is not an entry!" , pentry_node->node_name);
			return -1;
		}

		//2.获取当前节点的总大小
		total_size = pentry_node->size * pentry_node->data.entry_value.count;
		if(total_size <= 0)
			goto _next_sibling;

		//3.节点version>当前version,跳过
		if(pentry_node->version > version)
		{
			pin->index += total_size;
			goto _next_sibling;
		}

		/***将节点里成员打包*/
		//1.计算有效数据数目
		if(pentry_node->data.entry_value.refer_idx>0 && pentry_node->data.entry_value.count>1)	//refer 需要计算实际数据
		{
			//refer entry
			ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.refer_idx];

			//计算refer的偏移从而获取值
			memcpy(&refer_count , &pin->src[in_start_index+ptmp_node->data.entry_value.offset] , ptmp_node->size);
			if(refer_count > pentry_node->data.entry_value.count)
			{
				print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! '%s' max count:%d < refer count:%d!" , pentry_node->node_name ,
						pentry_node->data.entry_value.count , refer_count);
				return -1;
			}
		}
		else
			refer_count = pentry_node->data.entry_value.count;

		//2.依次打包每个有效成员
		for(i=0; i<refer_count; i++)
		{
			//普通类型，则直接计算即可
			if(pentry_node->data.entry_value.entry_type>=SDR_T_CHAR && pentry_node->data.entry_value.entry_type<=SDR_T_MAX)
			{
				memcpy(&pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size);
				pin->index += pentry_node->size;
				pout->index += pentry_node->size;
				continue;
			}

			//struct类型
			if(pentry_node->data.entry_value.entry_type == SDR_T_STRUCT)
			{
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.type_idx];
				ret = pack_struct_node(pres , ptmp_node , pout , pin , version , log_fp);
				if(ret < 0)
					return -1;

				continue;
			}

			//union类型
			if(pentry_node->data.entry_value.entry_type == SDR_T_UNION)
			{
				//获取selected_id
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.select_idx];
				//计算select的偏移从而获取值
				memcpy(&selected_id , &pin->src[in_start_index+ptmp_node->data.entry_value.offset] , ptmp_node->size);

				//打包union
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.type_idx];
				ret = pack_union_node(pres , ptmp_node , pout , pin , version , log_fp , selected_id);
				if(ret < 0)
					return -1;

				continue;
			}

			//unkown type
			print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! '%s' type unknown!" , pentry_node->node_name);
			return -1;
		}

		//3.跳过非有效成员
		pin->index += (pentry_node->data.entry_value.count - refer_count)*pentry_node->size;

_next_sibling:
		if(pentry_node->sibling_idx <= 0)	//没有兄弟节点
			break;
		pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->sibling_idx];
	}

	pin->index = in_start_index + pnode->size;	//指向下一个起始流
	return 0;
}
static int pack_union_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , FILE *log_fp , int select_id)
{
	sdr_node_t *pentry_node;
	sdr_node_t *ptmp_node;

	int in_start_index;	//当前union起始index
	int ret;
	int i;

	/***Arg Check*/
	if(!pres || !pnode || !pout || !pin || version<0 || select<0)
		return -1;

	//不是union
	if(pnode->class != SDR_CLASS_UNION)
		return -1;

	//保存当前起始节点和总大小
	in_start_index = pin->index;

	//大于当前版本则跳过不鸟
	if(pnode->version > version)
	{
//		print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! type '%s' version=%d is larger than curr version.%d" , pnode->node_name , pnode->version , version);
//		return -1;
		goto _end_pack;
	}

	/***Init*/
	if(pnode->data.union_value.child_idx <= 0)	//没有子节点
		return 0;
	pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.union_value.child_idx];

	/***Handle*/
	//1.获得目标id的entry
	while(pentry_node)
	{
		if(pentry_node->data.entry_value.select_id == select_id)
			break;

		if(pentry_node->sibling_idx <=0 )
			break;
		pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->sibling_idx];
	}
	if(pentry_node->data.entry_value.select_id != select_id)
	{
		print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! Union '%s' has no member id=%d" , pnode->node_name , select_id);
		return -1;
	}

	//2.打包entry
	if(pentry_node)
	{
		//1.检查是否entry节点
		if(pentry_node->class != SDR_CLASS_ENTRY)
		{
			print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! '%s' is not an entry!" , pentry_node->node_name);
			return -1;
		}

		//3.节点version>当前version,跳过
		if(pentry_node->version > version)
		{
			goto _end_pack;
		}

		/***将该entry打包*/
		//1.依次打包该entry每个有效成员（这里没有refer，因为union成员里不会有refer存在）
		for(i=0; i<pentry_node->data.entry_value.count; i++)
		{
			//普通类型，则直接计算即可
			if(pentry_node->data.entry_value.entry_type>=SDR_T_CHAR && pentry_node->data.entry_value.entry_type<=SDR_T_MAX)
			{
				memcpy(&pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size);
				pin->index += pentry_node->size;
				pout->index += pentry_node->size;
				continue;
			}

			//struct类型
			if(pentry_node->data.entry_value.entry_type == SDR_T_STRUCT)
			{
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.type_idx];
				ret = pack_struct_node(pres , ptmp_node , pout , pin , version , log_fp);
				if(ret < 0)
					return -1;

				continue;
			}

			//union类型不能直接嵌套
			//unkown type
			print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! '%s' type unknown or is union!" , pentry_node->node_name);
			return -1;
		}

	}

_end_pack:
	pin->index = in_start_index + pnode->size;	//将输入置于下一个将解析的数据流开始
	return 0;
}

static int unpack_struct_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , FILE *log_fp)
{
	sdr_node_t *pentry_node;
	sdr_node_t *ptmp_node;

	int out_start_index;	//当前结构体起始index

	int total_size;	//entry总大小
	unsigned int refer_count = 0;
	unsigned int selected_id = 0;

	int ret;
	int i;

	/***Arg Check*/
	if(!pres || !pnode || !pin || !pout || version<0)
		return -1;

	//不是struct
	if(pnode->class != SDR_CLASS_STRUCT)
		return -1;

	out_start_index = pout->index;
	/***Init*/
	if(pnode->data.struct_value.child_idx <= 0)	//没有子节点
		return 0;
	pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.struct_value.child_idx];

	/***Handle*/
	while(pentry_node)
	{
		//1.检查是否entry节点
		if(pentry_node->class != SDR_CLASS_ENTRY)
		{
			print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! '%s' is not an entry!" , pentry_node->node_name);
			return -1;
		}

		//2.获取当前节点的总大小
		total_size = pentry_node->size * pentry_node->data.entry_value.count;
		if(total_size <= 0)
			goto _next_sibling;

		//3.节点version>待解压version,跳过
		if(pentry_node->version > version)
		{
			pout->index += total_size;
			goto _next_sibling;
		}

		/***将节点里成员解包*/
		//1.计算有效数据数目
		if(pentry_node->data.entry_value.refer_idx>0 && pentry_node->data.entry_value.count>1)	//refer 需要计算实际数据
		{
			//refer entry
			ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.refer_idx];

			//计算refer的偏移从而获取值(注意是偏移需要根据out_put来计算，原因是in_put已经被压缩，而节点里记录的偏移是依据无压缩结构计算的)
			memcpy(&refer_count , &pout->src[out_start_index+ptmp_node->data.entry_value.offset] , ptmp_node->size);
			if(refer_count > (unsigned int)pentry_node->data.entry_value.count)
			{
				print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! '%s' max count:%d < packed refer count:%d!" , pentry_node->node_name ,
						pentry_node->data.entry_value.count , refer_count);
				return -1;
			}
		}
		else
			refer_count = pentry_node->data.entry_value.count;

		//2.依次解压每个有效成员
		for(i=0; i<refer_count; i++)
		{
			//普通类型，则直接计算即可
			if(pentry_node->data.entry_value.entry_type>=SDR_T_CHAR && pentry_node->data.entry_value.entry_type<=SDR_T_MAX)
			{
				memcpy(&pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size);
				pin->index += pentry_node->size;
				if(pin->index > pin->length)	//已经超过了数据流长度
				{
					print_info(INFO_ERR , log_fp , "Error:Unpack '%s' Failed! overflow input lenth:%d\n" , pnode->node_name , pin->length);
					return -1;
				}
				pout->index += pentry_node->size;
				continue;
			}

			//struct类型
			if(pentry_node->data.entry_value.entry_type == SDR_T_STRUCT)
			{
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.type_idx];
				ret = unpack_struct_node(pres , ptmp_node , pout , pin , version , log_fp);
				if(ret < 0)
					return -1;

				continue;
			}

			//union类型
			if(pentry_node->data.entry_value.entry_type == SDR_T_UNION)
			{
				//获取selected_id
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.select_idx];
				//计算select的偏移从而获取值
				memcpy(&selected_id , &pout->src[out_start_index+ptmp_node->data.entry_value.offset] , ptmp_node->size);

				//解压union
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.type_idx];
				ret = unpack_union_node(pres , ptmp_node , pout , pin , version , log_fp , selected_id);
				if(ret < 0)
					return -1;

				continue;
			}

			//unkown type
			print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! '%s' type unknown!" , pentry_node->node_name);
			return -1;
		}

		//3.跳过非有效成员
		pout->index += (pentry_node->data.entry_value.count - refer_count)*pentry_node->size;

_next_sibling:
		if(pentry_node->sibling_idx <= 0)	//没有兄弟节点
			break;
		pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->sibling_idx];
	}

	pout->index = out_start_index + pnode->size;	//指向下一个起始流
	return 0;
}

static int unpack_union_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , FILE *log_fp , int select_id)
{
	sdr_node_t *pentry_node;
	sdr_node_t *ptmp_node;

	int out_start_index;	//目标union起始index
	int ret;
	int i;

	/***Arg Check*/
	if(!pres || !pnode || !pin || !pout || version<0 || select_id<0)
		return -1;

	//不是union
	if(pnode->class != SDR_CLASS_UNION)
		return -1;

	//保存当前起始节点和总大小
	out_start_index = pout->index;

	//大于待解压版本则跳过不鸟
	if(pnode->version > version)
	{
//		print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! type '%s' version=%d is larger than curr version.%d" , pnode->node_name , pnode->version , version);
//		return -1;
		goto _end_pack;
	}

	/***Init*/
	if(pnode->data.union_value.child_idx <= 0)	//没有子节点
		return 0;
	pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pnode->data.union_value.child_idx];

	/***Handle*/
	//1.获得目标id的entry
	while(pentry_node)
	{
		if(pentry_node->data.entry_value.select_id == select_id)
			break;

		if(pentry_node->sibling_idx <=0 )
			break;
		pentry_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->sibling_idx];
	}
	if(pentry_node->data.entry_value.select_id != select_id)
	{
		print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! Union '%s' has no member id=%d" , pnode->node_name , select_id);
		return -1;
	}

	//2.解压entry
	if(pentry_node)
	{
		//1.检查是否entry节点
		if(pentry_node->class != SDR_CLASS_ENTRY)
		{
			print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! '%s' is not an entry!" , pentry_node->node_name);
			return -1;
		}

		//3.节点version>待解压version,跳过
		if(pentry_node->version > version)
		{
			goto _end_pack;
		}

		/***将该entry解压*/
		//1.依次解压该entry每个有效成员（这里没有refer，因为union成员里不会有refer存在）
		for(i=0; i<pentry_node->data.entry_value.count; i++)
		{
			//普通类型，则直接计算即可
			if(pentry_node->data.entry_value.entry_type>=SDR_T_CHAR && pentry_node->data.entry_value.entry_type<=SDR_T_MAX)
			{
				memcpy(&pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size);
				pin->index += pentry_node->size;
				if(pin->index > pin->length)	//已经超过了数据流长度
				{
					print_info(INFO_ERR , log_fp , "Error:Unpack '%s' Failed! overflow input lenth:%d\n" , pnode->node_name , pin->length);
					return -1;
				}
				pout->index += pentry_node->size;
				continue;
			}

			//struct类型
			if(pentry_node->data.entry_value.entry_type == SDR_T_STRUCT)
			{
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.type_idx];
				ret = unpack_struct_node(pres , ptmp_node , pout , pin , version , log_fp);
				if(ret < 0)
					return -1;

				continue;
			}

			//union类型不能直接嵌套
			//unkown type
			print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! '%s' type unknown or is union!" , pentry_node->node_name);
			return -1;
		}

	}

_end_pack:
	pout->index = out_start_index + pnode->size;	//将输入置于下一个将解压的数据流开始
	return 0;
}

/*
 * 获取一个名字在node_map中的序号
 * <0:error
 * else:序号
 */
static int get_sym_map_index(sdr_data_res_t *pres , char *sym_name)
{
	int i;
	int pos;

	/***Arg Check*/
	if(!pres || !sym_name || strlen(sym_name)<=0)
		return -1;

	//先查看目标处
	pos = BKDRHash(sym_name) % pres->max_node;
	if(strcmp(pres->psym_table->entry_list[pos].sym_name , sym_name) == 0)
	{
		return pres->psym_table->entry_list[pos].index;
	}

	//搜索所有
	for(i=0; i<pres->max_node; i++)
	{
		if(strcmp(pres->psym_table->entry_list[i].sym_name , sym_name) == 0)
		{
			return pres->psym_table->entry_list[i].index;
		}
	}

	return -1;
}

/*
 * 获取基本类型长度
 * @return
 * > 0：基本类型长度
 * <0: error
 */
static int get_base_type_size(char type)
{

	int size = -1;

	if(type<SDR_T_CHAR || type>SDR_T_MAX)
		return -1;

	switch(type)
	{
	case SDR_T_CHAR:
	case SDR_T_UCHAR:
		size = sizeof(char);
		break;
	case SDR_T_SHORT:
	case SDR_T_USHORT:
		size = sizeof(short);
		break;
	case SDR_T_INT:
	case SDR_T_UINT:
		size = sizeof(int);
		break;
	case SDR_T_LONG:
	case SDR_T_ULONG:
		size = sizeof(long);
		break;
	case SDR_T_FLOAT:
		size = sizeof(float);
		break;
	case SDR_T_DOUBLE:
		size = sizeof(double);
		break;
	case SDR_T_LONGLONG:
		size = sizeof(long long);
		break;
	default:
		size = -1;
		break;
	}

	return size;
}

/*
 * 根据偏移获取对应的node
 *@return:
 *NULL:FAILED
 *ELSE:SUCCESS
 */
static sdr_node_t *fetch_node_by_index(sdr_data_res_t *pres , int iIndex)
{
	sdr_node_t *pnode = NULL;

	/***Arg Check*/
	if(!pres || iIndex<=0 || iIndex>=pres->pnode_map->count)
		return NULL;

	pnode = &pres->pnode_map->node_list[iIndex];
	if(pnode->my_idx != iIndex)
		return NULL;

	return pnode;
}

/*
 * 获取某结构之成员node
 * @type_name:类型名[仅限于结构]
 * @member_name[成员名]
 * @err_code:错误码
 * 0:成功
 * -1:参数错误
 * -2:找不到结构
 * -3:类型错误非结构体
 * -4:找不到成员
 * -5:成员错误
 *@return:
 *NULL:FAILED
 *ELSE:SUCCESS
 */
static sdr_node_t *fetch_node_entry_of_type(sdr_data_res_t *pres , char *type_name , char *member_name , int *perr_code)
{
	sdr_node_t *pmain_node = NULL;
	sdr_node_t *pnode = NULL;
	int iIndex = -1;

	/***Arg Check*/
	if(!perr_code)
		return NULL;

	if(!pres || !type_name || !member_name)
	{
		*perr_code = -1;
		return NULL;
	}

	if(strlen(type_name)<=0 || strlen(member_name)<=0)
	{
		*perr_code = -1;
		return NULL;
	}
	/***Handle*/
	//1.获取主节点
	iIndex = get_sym_map_index(pres , type_name);
	pmain_node = fetch_node_by_index(pres , iIndex);
	if(!pmain_node)
	{
		*perr_code = -2;
		return NULL;
	}

	if(pmain_node->class != SDR_CLASS_STRUCT)
	{
		*perr_code = -3;
		return NULL;
	}

	//2.查找
	pnode = fetch_node_by_index(pres , pmain_node->data.struct_value.child_idx);
	while(1)
	{
		//指针判定
		if(pnode == NULL)
		{
			*perr_code = -4;
			break;
		}

		//是否ENTRY
		if(pnode->class != SDR_CLASS_ENTRY)
		{
			*perr_code = -5;
			break;
		}

		//找到
		if(strcmp(pnode->node_name , member_name) == 0)
		{
			*perr_code = 0;
			return pnode;
		}
		//没找到则继续查找下一成员
		pnode = fetch_node_by_index(pres , pnode->sibling_idx);
	}

	return NULL;
}


/*
 * 将基本sdr类型转换为对应的字符串
 * NULL:错误
 * ELSE:类型字符指针
 */
char *reverse_label_type(char sdr_type)
{
	static char str_type[SDR_NAME_LEN] = {0};

	/***Arg Check*/
	if(sdr_type<SDR_T_CHAR || sdr_type>SDR_T_MAX)
		return NULL;

	/***Handle*/
	memset(str_type , 0 , sizeof(str_type));
	switch (sdr_type)
	{
		case SDR_T_CHAR:
			strncpy(str_type , "char" , sizeof(str_type));
			break;
		case SDR_T_UCHAR:
			strncpy(str_type , "unsigned char" , sizeof(str_type));
			break;
		case SDR_T_SHORT:
			strncpy(str_type , "short" , sizeof(str_type));
			break;
		case SDR_T_USHORT:
			strncpy(str_type , "unsigned short" , sizeof(str_type));
			break;
		case SDR_T_INT:
			strncpy(str_type , "int" , sizeof(str_type));
			break;
		case SDR_T_UINT:
			strncpy(str_type , "unsigned int" , sizeof(str_type));
			break;
		case SDR_T_LONG:
			strncpy(str_type , "long" , sizeof(str_type));
			break;
		case SDR_T_ULONG:
			strncpy(str_type , "unsigned long" , sizeof(str_type));
			break;
		case SDR_T_FLOAT:
			strncpy(str_type , "float" , sizeof(str_type));
			break;
		case SDR_T_DOUBLE:
			strncpy(str_type , "double" , sizeof(str_type));
			break;
		case SDR_T_LONGLONG:
			strncpy(str_type , "long long" , sizeof(str_type));
			break;
		default:
			return NULL;
			break;
	}

//	printf("type is:%s\n" , str_type);
	return str_type;
}

// BKDR Hash Function
unsigned int BKDRHash(char *str)
{
    unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;

    while (*str)
    {
        hash = hash * seed + (*str++);
    }

    return (hash & 0x7FFFFFFF);
}
