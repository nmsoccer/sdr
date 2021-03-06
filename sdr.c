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
#include <errno.h>
#include <ctype.h>
#include "sdr.h"

extern int errno;
static int sdr_dump_max_version = 0;
/************LOCAL FUNCTION*/
static int gen_struct_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp);
static int gen_union_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp);
static int gen_entry_xml(sdr_data_res_t *pres , sdr_node_t *pnode , FILE *fp);

static int pack_struct_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , char net_byte , FILE *log_fp);
static int pack_union_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , char net_byte , FILE *log_fp , int select_id);
static int unpack_struct_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , char net_byte , FILE *log_fp);
static int unpack_union_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , char net_byte , FILE *log_fp , int select_id);
static int pack_hton(char type , char *out , char *in , int size , FILE *fp);
static int unpack_ntoh(char type , char *out , char *in , int size , FILE *fp);

static int get_sym_map_index(sdr_data_res_t *pres , char *sym_name);
static int get_base_type_size(char type);
static int print_info(char type , FILE *fp , char *fmt , ...);
static sdr_node_t *fetch_node_by_index(sdr_data_res_t *pres , int iIndex);
static sdr_node_t *fetch_node_by_type(sdr_data_res_t *pres , char *type_name);
static sdr_node_t *fetch_node_entry_of_type(sdr_data_res_t *pres , char *type_name , char *member_name , int *perr_code);
static int unpack_to_sym_table(packed_sym_table_t *ppacked , sym_table_t *psym);

static int dump_struct_info(sdr_data_res_t *pres , char *name , char *type_name , char *prefix , sdr_node_t *pmain_node , char *data , FILE *fp);
static int dump_union_info(sdr_data_res_t *pres , char *name , char *type_name , char *prefix , sdr_node_t *pmain_node , char *data , int my_select , FILE *fp);
static int dump_basic_info(sdr_data_res_t *pres , char *name , char *type_name , char *prefix , sdr_node_t *pmain_node , char *data , FILE *fp);
static unsigned short check_sum(unsigned char *array , int len);

/************GLOBAL FUNCTION*/
/*
 * 加载目标bin文件入内存
 */
#define SDR_LOAD_BIN_FAIL  \
	do \
    {  \
		close(fd); \
		sdr_free_bin(pres); \
		return NULL; \
	}while(0)

API sdr_data_res_t *sdr_load_bin(char *file_name , FILE *log_fp)
{
	int fd;
	sdr_data_res_t *pres = NULL;
	sdr_node_map_t *pmap = NULL;
	sym_table_t *psym = NULL;
	packed_sym_table_t *ppacked_sym = NULL;
	char *pstart = NULL;
	char buff[32] = {0};
	int max_node = 0;
	int map_count = 0;
	int sym_list_size = 0;
	int my_list_size = 0;
	int size;
	int ret;
	unsigned short file_sum = 0;
	unsigned short mem_sum = 0;

	/***Arg Check*/
	if(!file_name || strlen(file_name)<=0)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! File name is empty!" , __FUNCTION__);
		return NULL;
	}

	/***初始化检查*/
	//1.打开目标文件
	fd = open(file_name , O_RDONLY , S_IRUSR);
	if(fd < 0)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! Can not Open file %s" , __FUNCTION__ , file_name);
		return NULL;
	}

	//2.读入magic+maxnode+malloc
	ret = read(fd , buff , strlen(SDR_MAGIC_STR));
	if(ret < 0)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! Read Magic Failed!" , __FUNCTION__);
		SDR_LOAD_BIN_FAIL;
	}
	if(strcmp(buff , SDR_MAGIC_STR) != 0)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! Not a regular sdr bin file!" , __FUNCTION__);
		SDR_LOAD_BIN_FAIL;
	}

	ret = read(fd , &max_node , sizeof(max_node));
	if(ret < 0)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! Read Max node Failed!" , __FUNCTION__);
		SDR_LOAD_BIN_FAIL;
	}
	if(max_node <= 0)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! max node <= 0" , __FUNCTION__);
		SDR_LOAD_BIN_FAIL;
	}

		//2.1malloc
	pres = (sdr_data_res_t *)calloc(1 , sizeof(sdr_data_res_t));
	if(!pres)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! calloc sdr_data_res_t failed! err:%s" , __FUNCTION__ , strerror(errno));
		SDR_LOAD_BIN_FAIL;
	}
	pres->max_node = max_node;
	strncpy(pres->magic , SDR_MAGIC_STR , sizeof(pres->magic));

	//3.读入sdr_nod_map 并分配内存
	memset(buff , 0 , strlen(buff));
	ret = read(fd , buff , sizeof(sdr_node_map_t));
	if(ret != sizeof(sdr_node_map_t))
	{
		print_info(INFO_ERR , log_fp , "Read sdr node map Failed!");
		SDR_LOAD_BIN_FAIL;
	}
	map_count = ((sdr_node_map_t *)buff)->count;
	if(map_count <= 0)
	{
		print_info(INFO_ERR , log_fp , "map count <= 0");
		SDR_LOAD_BIN_FAIL;
	}

		//3.1malloc
	pmap = (sdr_node_map_t *)calloc(1 , (sizeof(sdr_node_map_t) + map_count*sizeof(sdr_node_t)));
	if(!pmap)
	{
		print_info(INFO_ERR , log_fp , "<%s> fail! calloc sdr_node_map failed!" , __FUNCTION__);
		SDR_LOAD_BIN_FAIL;
	}
	pmap->count = map_count;
	pres->pnode_map = pmap;

		//3.2read node list
	size = map_count * sizeof(sdr_node_t);
	ret = read(fd , &pmap->node_list[0] ,  size);
	if(ret != size)
	{
		print_info(INFO_ERR , log_fp , "<%s>fail! read node_entry failed! target:%d read:%d" , __FUNCTION__ , size , ret);
		SDR_LOAD_BIN_FAIL;
	}

	//4. 读取packed_sym_table 并转成 sym_table
	memset(buff , 0 , sizeof(buff));
	ret = read(fd , buff , sizeof(packed_sym_table_t));
	if(ret != sizeof(packed_sym_table_t))
	{
		print_info(INFO_ERR , log_fp , "<%s> fail! read packed_sym_table  failed! target:%d read:%d" , __FUNCTION__ , sizeof(packed_sym_table_t) ,
				ret);
		SDR_LOAD_BIN_FAIL;
	}
	sym_list_size = ((packed_sym_table_t *)buff)->sym_list_size;
	my_list_size = ((packed_sym_table_t *)buff)->my_list_size;
	if(sym_list_size==0 || my_list_size==0)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! packed_sym data illegal! sym_list_size:%d my_list_size:%d" , __FUNCTION__ , sym_list_size ,
				my_list_size);
		SDR_LOAD_BIN_FAIL;
	}

		//4.1malloc
	psym = (sym_table_t *)calloc(1 , sizeof(sym_table_t) + sym_list_size*sizeof(sym_entry_t));
	if(!psym)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! calloc sym_table failed! err:%s" , __FUNCTION__ , strerror(errno));
		SDR_LOAD_BIN_FAIL;
	}
	psym->count = my_list_size;
	pres->psym_table = psym;

	ppacked_sym = (packed_sym_table_t *)calloc(1 , sizeof(packed_sym_table_t) + my_list_size*sizeof(packed_sym_entry_t));
	if(!ppacked_sym)
	{
		print_info(INFO_ERR , log_fp , "<%s> failed! calloc packed_sym_table failed! err:%s" , __FUNCTION__ , strerror(errno));
		SDR_LOAD_BIN_FAIL;
	}
	ppacked_sym->sym_list_size = sym_list_size;
	ppacked_sym->my_list_size = my_list_size;

		//4.2read packed_sym
	size = my_list_size * sizeof(packed_sym_entry_t);
	ret = read(fd , &ppacked_sym->entry_list[0] ,  size);
	if(ret != size)
	{
		print_info(INFO_ERR , log_fp , "<%s>fail! read packed_sym_entry failed! target:%d read:%d" , __FUNCTION__ , size , ret);
		SDR_LOAD_BIN_FAIL;
	}

		//4.3 convert packed_sym_table --> sym_table
	ret = unpack_to_sym_table(ppacked_sym , psym);
	if(ret < 0)
	{
		print_info(INFO_ERR , log_fp , "<%s>failed! unpack to sym_table failed!" , __FUNCTION__);
		free(ppacked_sym);
		SDR_LOAD_BIN_FAIL;
	}

		//4.4print and release
	dump_packed_sym_table(ppacked_sym);
	dump_sym_table(psym , sym_list_size);
	free(ppacked_sym);


	//4.文件读位置指向sdr_nod_map
	//lseek(fd , 0-sizeof(sdr_node_map_t) , SEEK_CUR);

	/***Handle*/
	/*
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
	pres->psym_table = (sym_table_t *)(start + sizeof(sdr_node_map_t) + map_count*sizeof(sdr_node_t));*/

	//5.check sum value
	//5.1 read
	size = sizeof(unsigned short);
	ret = read(fd , &file_sum ,  size);
	if(ret != size)
	{
		print_info(INFO_ERR , log_fp , "<%s>fail! read checksum value failed! target:%d read:%d" , __FUNCTION__ , size , ret);
		SDR_LOAD_BIN_FAIL;
	}

	//5.2 calc mem
	mem_sum = check_sum_sdr(pres , &ret);
	if(ret < 0)
	{
		print_info(INFO_ERR , log_fp , "<%s>fail! calc checksum value failed! ret:%d" , __FUNCTION__ , ret);
		SDR_LOAD_BIN_FAIL;
	}
	if(mem_sum != file_sum)
	{
		print_info(INFO_ERR , log_fp , "<%s>fail! checksum value failed! FileSum:%d MemSum:%d" , __FUNCTION__ , file_sum , mem_sum);
		SDR_LOAD_BIN_FAIL;
	}

	print_info(INFO_NORMAL , log_fp , "%s max node:%d, map_count:%d , entry_size:%d entry_count:%d checksum:%d" , __FUNCTION__ , max_node , pres->pnode_map->count , sym_list_size , pres->psym_table->count ,
			mem_sum);
	return pres;
}

/*
 * 释放加载入内存的sdr文件
 */
API int sdr_free_bin(sdr_data_res_t *pres)
{
	sym_table_t *psym = NULL;
	sym_entry_t *pentry = NULL;
	sym_entry_t *ptmp = NULL;
	int i = 0;

	if(!pres)
		return -1;

	//order by order
	if(pres->pnode_map)
		free(pres->pnode_map);

	if(pres->psym_table)
	{
		psym = pres->psym_table;

		//free each chain-list
		for(i=0; i<pres->max_node; i++)
		{
			if(psym->entry_list[i].index <= 0)
				continue;

			pentry = psym->entry_list[i].next; //curr free
			while(pentry)
			{
				ptmp = pentry->next;
				free(pentry);
				pentry = ptmp;
			}
		}

		//free sym_table
		free(pres->psym_table);
	}
	free(pres);
	return 0;
}

/*
 * 打印出bin文件内容为xml格式
 */
NO_API int sdr_bin2xml(sdr_data_res_t *pres , char *file_name , FILE *log_fp)
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
 * net_byte:转换成网络序(0:直接打包 1:转成网络序)
 *   若打开网络字节转换 浮点型将透传字节，不进行转换
 *   若可以保证打解包双方大小端一致可以关闭网络转换以提高性能，同时兼容浮点型
 * return:>=0压缩后的数据
 * else:错误
 */
API int sdr_pack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , int version , char net_byte , FILE *log_fp)
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
			ret = pack_struct_node(pres , pnode , &sdr_out , &sdr_in , version , net_byte , log_fp);
			break;
		}

		if(pnode->class == SDR_CLASS_UNION)
		{
			ret = pack_union_node(pres , pnode , &sdr_out , &sdr_in , version , net_byte , log_fp , -1);
			break;
		}

		//非复合结构体
		print_info(INFO_ERR , log_fp , "Error:sdr pack Failed! type '%s' is no struct or union" , type_name);
		ret = -1;
	}while(0);

	if(ret == 0)
	{
#if SDR_PRINT_DEBUG
		printf("pack '%s' success! %d -> %d\n" , type_name , sdr_in.index , sdr_out.index);
#endif
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
 * net_byte:从网络序解包(0:直接解包 1:从网络序解包)
 * return:>=0解压缩后的数据
 * else:错误
 */
API int sdr_unpack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , char net_byte , FILE *log_fp)
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
	//print_info(INFO_NORMAL , log_fp , "ready to unpack '%s', version:%d,length:%d" , type_name , version , sdr_in.length);
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
			ret = unpack_struct_node(pres , pnode , &sdr_out , &sdr_in , version , net_byte , log_fp);
			break;
		}

		if(pnode->class == SDR_CLASS_UNION)
		{
			ret = unpack_union_node(pres , pnode , &sdr_out , &sdr_in , version , net_byte , log_fp , -1);
			break;
		}

		//非复合结构体
		print_info(INFO_ERR , log_fp , "Error:sdr unpack Failed! type '%s' is no struct or union" , type_name);
		ret = -1;
	}while(0);

_unpack_end:
	if(ret == 0)
	{
#if SDR_PRINT_DEBUG
		print_info(INFO_NORMAL , log_fp , "unpack '%s' success! %d->%d", type_name , sdr_in.index , sdr_out.index);
#endif
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
API int sdr_member_offset(sdr_data_res_t *pres , char *type_name , char *member_name)
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
API int sdr_next_member(sdr_data_res_t *pres , char *type_name , char *curr_member , char *next_member , int len)
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

/*
 * 打印结构体的数据信息
 * @type_name:xml里定义的结构体名
 * @结构体数据的起始地址
 * @打印到的目标文件
 * @return:
 * 0 : success
 * -1:failed
 */
API int sdr_dump_struct(sdr_data_res_t *pres , char *type_name , char *struct_data , FILE *fp)
{
	sdr_node_t *pmain_node;
	time_t timep;
	struct tm *ptm;

	/***Arg Check*/
	if(!pres || !type_name || !struct_data)
	{
		print_info(INFO_ERR , fp , "<%s> failed! some arg nil!" , __FUNCTION__);
		return -1;
	}

	pmain_node = fetch_node_by_type(pres , type_name);
	if(!pmain_node)
	{
		print_info(INFO_ERR , fp , "<%s> failed! type %s not found!" , __FUNCTION__ , type_name);
		return -1;
	}

	if(pmain_node->class != SDR_CLASS_STRUCT)
	{
		print_info(INFO_ERR , fp , "<%s> failed! %s not a struct!" , __FUNCTION__ , type_name);
		return -1;
	}

	/***Print Head*/
	timep = time(NULL);
	ptm = localtime(&timep);
	print_info(INFO_NORMAL , fp , "<?xml version=\"1.0\" encoding=\"utf8\" ?>");
	print_info(INFO_NORMAL , fp , "<!-- Created by sdr on %d-%02d-%02d %02d:%02d:%02d  -->" , ptm->tm_year+1900 , ptm->tm_mon+1 , ptm->tm_mday , ptm->tm_hour ,
			ptm->tm_min , ptm->tm_sec);
	print_info(INFO_NORMAL , fp , "<!-- @https://github.com/nmsoccer/sdr --> \n");

	/***Hanlde*/
	sdr_dump_max_version = 0;
	dump_struct_info(pres , "" , type_name , "" , pmain_node , struct_data , fp);

	/***Print Version*/
	print_info(INFO_NORMAL , fp , "\n<version name=\"dump_max\" value=\"%d\" />" , sdr_dump_max_version);
	sdr_dump_max_version = 0;

	if(fp)
		fflush(fp);
	return 0;
}


static int print_info(char type , FILE *fp , char *fmt , ...)
{
	va_list vp;
	char buff[4096] = {0};

	/***Arg Check*/
	if(!fmt || strlen(fmt)<=0)
		return -1;


	/***Handle*/
	va_start(vp , fmt);

	switch(type)
	{
	case INFO_NORMAL:
	case INFO_DISABLE:
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
	{
		if(type != INFO_DISABLE)
			fprintf(fp , "%s\n" , buff);
		else
			fprintf(fp , "%s" , buff);
	}
	else
	{
		if(type != INFO_DISABLE)
			printf("%s\n" , buff);
		else
			printf("%s" , buff);
	}
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
		start = reverse_label_type(pnode->data.entry_value.entry_type , NULL);
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


static int pack_struct_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , char net_byte , FILE *log_fp)
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
				if(net_byte == 0) //直传
					memcpy(&pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size);
				else  //网络字节转换
				{
					if (pack_hton(pentry_node->data.entry_value.entry_type , &pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size , log_fp) < 0)
					{
						print_info(INFO_ERR , log_fp , "<%s> failed! pack_hton meets an error! name:%s type:%s size:%s" , __FUNCTION__ ,
								pentry_node->node_name , pentry_node->data.entry_value.entry_type , pentry_node->size);
						return -1;
					}
				}
				pin->index += pentry_node->size;
				pout->index += pentry_node->size;
				continue;
			}

			//struct类型
			if(pentry_node->data.entry_value.entry_type == SDR_T_STRUCT)
			{
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.type_idx];
				ret = pack_struct_node(pres , ptmp_node , pout , pin , version , net_byte , log_fp);
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
				ret = pack_union_node(pres , ptmp_node , pout , pin , version , net_byte , log_fp , selected_id);
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
static int pack_union_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version , char net_byte , FILE *log_fp , int select_id)
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
				if(net_byte == 0) //直传
					memcpy(&pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size);
				else  //网络字节转换
				{
					if (pack_hton(pentry_node->data.entry_value.entry_type , &pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size , log_fp) < 0)
					{
						print_info(INFO_ERR , log_fp , "<%s> failed! pack_hton meets an error! name:%s type:%s size:%s" , __FUNCTION__ ,
								pentry_node->node_name , pentry_node->data.entry_value.entry_type , pentry_node->size);
						return -1;
					}
				}
				pin->index += pentry_node->size;
				pout->index += pentry_node->size;
				continue;
			}

			//struct类型
			if(pentry_node->data.entry_value.entry_type == SDR_T_STRUCT)
			{
				ptmp_node = (sdr_node_t *)&pres->pnode_map->node_list[pentry_node->data.entry_value.type_idx];
				ret = pack_struct_node(pres , ptmp_node , pout , pin , version , net_byte , log_fp);
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

//打包时将成员打为网络序
//float & double 透传
// arg no check
//return: -1 failed 0 success
static int pack_hton(char type , char *out , char *in , int size , FILE *fp)
{
	//char
	if(type==SDR_T_CHAR || type==SDR_T_UCHAR)
	{
		*out = *in;
		return 0;
	}

	//float & double
	if(type==SDR_T_FLOAT || type==SDR_T_DOUBLE)
	{
		memcpy(out , in , size);
		return 0;
	}

	//short
	if(type==SDR_T_SHORT || type==SDR_T_USHORT)
	{
		*((unsigned short *)out) = htons(*((unsigned short *)in));
		return 0;
	}

	//int
	if(type==SDR_T_INT || type==SDR_T_UINT)
	{
		*((unsigned int *)out) = htonl(*((unsigned int *)in));
		return 0;
	}

	//long
	if(type==SDR_T_LONG || SDR_T_ULONG)
	{
		if(size == 4) //32bit
		{
			*((unsigned int *)out) = htonl(*((unsigned int *)in));
			return 0;
		}
		else if(size == 8) //64bit
		{
			*((unsigned int*)(out+4)) = htonl(*((unsigned int *)in));
			*((unsigned int*)out) = htonl(*((unsigned int *)(in+4)));
			return 0;
		}
		else
		{
			print_info(INFO_ERR , fp , "<%s> failed! long size %d illegal!" , __FUNCTION__ , size);
			return -1;
		}
	}

	//long long
	if(type==SDR_T_LONGLONG)
	{
		*((unsigned int*)(out+4)) = htonl(*((unsigned int *)in));
		*((unsigned int*)out) = htonl(*((unsigned int *)(in+4)));
		return 0;
	}

	//err:
	print_info(INFO_ERR , fp , "<%s> failed! type:%d size %d illegal!" , __FUNCTION__ , type , size);
	return -1;
}

//解包时将网络序解为主机
// arg no check
//return: -1 failed 0 success
static int unpack_ntoh(char type , char *out , char *in , int size , FILE *fp)
{
	long long host;
	long long net;
	//char
	if(type==SDR_T_CHAR || type==SDR_T_UCHAR)
	{
		*out = *in;
		return 0;
	}

	//float & double
	if(type==SDR_T_FLOAT || type==SDR_T_DOUBLE)
	{
		memcpy(out , in , size);
		return 0;
	}

	//short
	if(type==SDR_T_SHORT || type==SDR_T_USHORT)
	{
		*((unsigned short *)out) = ntohs(*((unsigned short *)in));
		return 0;
	}

	//int
	if(type==SDR_T_INT || type==SDR_T_UINT)
	{
		*((unsigned int *)out) = ntohl(*((unsigned int *)in));
		return 0;
	}

	//long
	if(type==SDR_T_LONG || SDR_T_ULONG)
	{
		if(size == 4) //32bit
		{
			*((unsigned int *)out) = ntohl(*((unsigned int *)in));
			return 0;
		}
		else if(size == 8) //64bit
		{
			*((unsigned int*)(out+4)) = ntohl(*((unsigned int *)in));
			*((unsigned int*)out) = ntohl(*((unsigned int *)(in+4)));
			return 0;
		}
		else
		{
			print_info(INFO_ERR , fp , "<%s> failed! long size %d illegal!" , __FUNCTION__ , size);
			return -1;
		}
	}

	//long long
	if(type==SDR_T_LONGLONG)
	{
		*((unsigned int*)(out+4)) = htonl(*((unsigned int *)in));
		*((unsigned int*)out) = htonl(*((unsigned int *)(in+4)));
		return 0;
	}

	//err:
	print_info(INFO_ERR , fp , "<%s> failed! type:%d size %d illegal!" , __FUNCTION__ , type , size);
	return -1;
}

static int unpack_struct_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version ,
		char net_byte , FILE *log_fp)
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
				if(net_byte == 0)//直传
					memcpy(&pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size);
				else  //网络字节转换
				{
					if (unpack_ntoh(pentry_node->data.entry_value.entry_type , &pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size , log_fp) < 0)
					{
						print_info(INFO_ERR , log_fp , "<%s> failed! unpack_ntoh meets an error! name:%s type:%s size:%s" , __FUNCTION__ ,
								pentry_node->node_name , pentry_node->data.entry_value.entry_type , pentry_node->size);
						return -1;
					}
				}
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
				ret = unpack_struct_node(pres , ptmp_node , pout , pin , version , net_byte , log_fp);
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
				ret = unpack_union_node(pres , ptmp_node , pout , pin , version , net_byte , log_fp , selected_id);
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

static int unpack_union_node(sdr_data_res_t *pres , sdr_node_t *pnode , sdr_buff_info_t *pout , sdr_buff_info_t *pin , int version ,
		char net_byte , FILE *log_fp , int select_id)
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
				if(net_byte == 0) //直传
					memcpy(&pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size);
				else  //网络字节转换
				{
					if (unpack_ntoh(pentry_node->data.entry_value.entry_type , &pout->src[pout->index] , &pin->src[pin->index] , pentry_node->size , log_fp) < 0)
					{
						print_info(INFO_ERR , log_fp , "<%s> failed! unpack_ntoh meets an error! name:%s type:%s size:%s" , __FUNCTION__ ,
								pentry_node->node_name , pentry_node->data.entry_value.entry_type , pentry_node->size);
						return -1;
					}
				}
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
				ret = unpack_struct_node(pres , ptmp_node , pout , pin , version , net_byte , log_fp);
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
	sym_entry_t *pentry = NULL;

	/***Arg Check*/
	if(!pres || !sym_name || strlen(sym_name)<=0)
		return -1;

	//先查看目标处
	pos = BKDRHash(sym_name) % pres->max_node;
	if(strcmp(pres->psym_table->entry_list[pos].sym_name , sym_name) == 0)
	{
		return pres->psym_table->entry_list[pos].index;
	}

	//search chain-list
	pentry = pres->psym_table->entry_list[pos].next;
	while(pentry)
	{
		if(strcmp(pentry->sym_name , sym_name) == 0)
			return pentry->index;

		pentry = pentry->next;
	}
	/*
	//搜索所有
	for(i=0; i<pres->max_node; i++)
	{
		if(strcmp(pres->psym_table->entry_list[i].sym_name , sym_name) == 0)
		{
			return pres->psym_table->entry_list[i].index;
		}
	}*/

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
 * 根据类型名获得对应的起始node
 * @return:
 * NULL: Failed
 * ELSE : Success
 */
static sdr_node_t *fetch_node_by_type(sdr_data_res_t *pres , char *type_name)
{
	int index = -1;
	if(!pres || !type_name)
		return NULL;

	index = get_sym_map_index(pres , type_name);
	if(index < 0)
		return NULL;

	return fetch_node_by_index(pres , index);
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
 * 将packed_sym_table展开到sym_table里
 * return: succes 0; failed -1
 */
static int unpack_to_sym_table(packed_sym_table_t *ppacked , sym_table_t *psym)
{
	int pos = -1;
	int i = 0;
	sym_entry_t *pentry = NULL;
	sym_entry_t *ptmp = NULL;

	/***Arg Check*/
	if(!ppacked || !psym)
		return -1;

	for(i=0; i<ppacked->my_list_size; i++)
	{
		pos = ppacked->entry_list[i].pos;
		if(pos<0 || pos>=ppacked->sym_list_size)
		{
			printf("<%s> failed! pos illegal! pos:%d sym_list_size:%d sym:%s" , __FUNCTION__ , pos , ppacked->sym_list_size ,
					ppacked->entry_list[i].sym_name);
			return -1;
		}

		//first entry
		if(psym->entry_list[pos].index <= 0)
		{
			memcpy(psym->entry_list[pos].sym_name , ppacked->entry_list[i].sym_name , SDR_NAME_LEN);
			psym->entry_list[pos].index = ppacked->entry_list[i].index;
			continue;
		}

		//search tail of chain-list
		pentry = &psym->entry_list[pos];
		while(1)
		{
			if(pentry->next == NULL)
				break;

			pentry = pentry->next;
		}

		//caloc entry
		ptmp = (sym_entry_t *)calloc(1 , sizeof(sym_entry_t));
		if(!ptmp)
		{
			printf("<%s> failed! malloc sym_entry:%s failed! err:%s\n" , __FUNCTION__ , ppacked->entry_list[i].sym_name ,
					strerror(errno));
			return -1;
		}

		memcpy(ptmp->sym_name , ppacked->entry_list[i].sym_name , SDR_NAME_LEN);
		ptmp->index = ppacked->entry_list[i].index;
		pentry->next = ptmp;
	}

	psym->count = ppacked->my_list_size;
	return 0;
}


/*
 * 将基本sdr类型转换为对应的字符串
 * @format_buff : 如果不为空则返回对应printf的限定符号[注意缓冲区足够长]
 * NULL:错误
 * ELSE:类型字符指针
 */
char *reverse_label_type(char sdr_type , char *format_buff)
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
			if(format_buff)
				strcpy(format_buff , "%d");
			break;
		case SDR_T_UCHAR:
			strncpy(str_type , "unsigned char" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%u");
			break;
		case SDR_T_SHORT:
			strncpy(str_type , "short" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%d");
			break;
		case SDR_T_USHORT:
			strncpy(str_type , "unsigned short" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%u");
			break;
		case SDR_T_INT:
			strncpy(str_type , "int" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%d");
			break;
		case SDR_T_UINT:
			strncpy(str_type , "unsigned int" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%u");
			break;
		case SDR_T_LONG:
			strncpy(str_type , "long" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%ld");
			break;
		case SDR_T_ULONG:
			strncpy(str_type , "unsigned long" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%lu");
			break;
		case SDR_T_FLOAT:
			strncpy(str_type , "float" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%f");
			break;
		case SDR_T_DOUBLE:
			strncpy(str_type , "double" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%lf");
			break;
		case SDR_T_LONGLONG:
			strncpy(str_type , "long long" , sizeof(str_type));
			if(format_buff)
				strcpy(format_buff , "%lld");
			break;
		default:
			return NULL;
			break;
	}

//	printf("type is:%s\n" , str_type);
	return str_type;
}


/*
 * check sum
 */
static unsigned short check_sum(unsigned char *array , int len)
{
	unsigned int sum = 0;
	unsigned short value = 0;
	int i = 0;

	//calc each two bytes
	while(1)
	{
		if(i+2>len)
			break;

		sum = sum + ((array[i]<<8) + array[i+1]);
		i += 2;
	}

	//last byte
	if(i<len)
	{
		sum = sum + (array[len-1]<<8);
	}

	//low 16bit + high 16bit < 0xffff
	do
	{
		sum = (sum & 0x0000ffff) + (sum >> 16);
	}while(sum > 0xffff);

	//~
	value = sum & 0x0000ffff;
	value =  ~value;
	return value;
}



// BKDR Hash Function
NO_API unsigned int BKDRHash(char *str)
{
    unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;

    while (*str)
    {
        hash = hash * seed + (*str++);
    }

    return (hash & 0x7FFFFFFF);
}

//dump table
NO_API int dump_sym_table(sym_table_t *psym_table , int max_size)
{

#if  !SDR_PRINT_DEBUG
	return 0;
#endif

	int i = 0;
	int checked = 0;
	sym_entry_t *pentry = NULL;
	printf("<%s> max_size:%d\n" , __FUNCTION__ , max_size);
	for(i=0; i<max_size&&checked<psym_table->count; i++)
	{
		if(psym_table->entry_list[i].index <= 0)
			continue;

		//print
		printf("<%d>:{%d , %s , 0x%lx}\n" , i , psym_table->entry_list[i].index , psym_table->entry_list[i].sym_name ,
				psym_table->entry_list[i].next);
		checked++;

		//print chain
		pentry = psym_table->entry_list[i].next;
		while(pentry)
		{
			printf("<%d>:{%d , %s , 0x%lx}\n" , i , pentry->index , pentry->sym_name ,pentry->next);
			checked++;
			pentry = pentry->next;
		}
	}

	return 0;
}

//dump table
NO_API int dump_packed_sym_table(packed_sym_table_t *ppacked_sym_table)
{

#if  !SDR_PRINT_DEBUG
	return 0;
#endif

	int i = 0;
	if(!ppacked_sym_table)
		return -1;

	printf("<%s> sym_list_size:%d my_list_size:%d\n" , __FUNCTION__ , ppacked_sym_table->sym_list_size , ppacked_sym_table->my_list_size);
	for(i=0; i<ppacked_sym_table->my_list_size; i++)
	{
		//print
		printf("<%d>[%d]{%d:%s}\n" , i , ppacked_sym_table->entry_list[i].pos , ppacked_sym_table->entry_list[i].index ,
				ppacked_sym_table->entry_list[i].sym_name);
	}

	return 0;
}

/*check sum
*计算sdr_data_res_t的总和校验值
*max_node和每个sdr_node_t.class
*@result: <0 error 0:success
*@return: checksum result
*/
NO_API unsigned short check_sum_sdr(sdr_data_res_t *pres , int *result)
{
	unsigned char buff[1024] = {0};
	unsigned char *byte_array = buff; //default
	char flag = 0;

	unsigned int sum = 0;
	int i = 0;
	int index = 0;
	int len = 0;

	/***Arg Check*/
	if(!result)
		return 0;

	if(!pres)
	{
		*result = -1;
		return 0;
	}

	/***Calc Len*/
	len = sizeof(int) + pres->pnode_map->count * sizeof(char);
	if(len > sizeof(buff))
	{
		byte_array = (unsigned char *)calloc(len , 1);
		if(!byte_array)
		{
			*result = -2;
			return 0;
		}
		flag = 1;
	}

	/***Fill Array*/
	index = 0;
	memcpy(&byte_array[index] , &pres->max_node , sizeof(pres->max_node));
	index += sizeof(pres->max_node);

	for(i=0; i<pres->pnode_map->count; i++,index++)
	{
		byte_array[index] = pres->pnode_map->node_list[i].class;
	}

	/***Calc*/
	sum = check_sum(byte_array , len);
	*result = 0;

	if(flag)
		free(byte_array);

	return sum;
}

/*
 * dump结构体数据
 * @name  该数据在父结构里的成员名 如果为""则是最外层
 * @type_name 该数据的类型名
 * @prefix 缩进
 * @pmain_node 该数据的node
 * @data 该数据的起始地址
 * @fp 打印文件句柄
 * @return:
 * 0  :  success
 * -1:  failed
 */
static int dump_struct_info(sdr_data_res_t *pres , char *name , char *type_name , char *prefix , sdr_node_t *pmain_node , char *data , FILE *fp)
{
	sdr_node_t *pnode = NULL;
	sdr_node_t *ptmp = NULL;
	long refer = 0;
	int select = 0;
	int i = 0;
	char next_prefix[128] = {0};
	char *pstr = NULL;
	int ret = 0;
	unsigned char value = 0;
	/***Arg Check*/
	//Basic Arg Not Check
	if(!type_name || strlen(type_name)<=0)
	{
		print_info(INFO_ERR , fp , "<%s> failed! type_name illegal!" , __FUNCTION__);
		return -1;
	}

	if(!data)
	{
		print_info(INFO_ERR , fp , "<%s> failed! data NULL! name:%s type:%s" , __FUNCTION__ , name , type_name);
		return -1;
	}

	if(pmain_node->class != SDR_CLASS_STRUCT)
	{
		print_info(INFO_ERR , fp , "<%s> failed! class %d not a struct! name:%s type:%s" , __FUNCTION__ , pmain_node->class , name , type_name);
		return -1;
	}

	/***Print Head*/
	snprintf(next_prefix , sizeof(next_prefix) , "%s%s" , prefix , SDR_DUMP_SPAN);
	print_info(INFO_NORMAL , fp , "%s<struct name=\"%s\" type=\"%s\">" , prefix , name , type_name);
	if(pmain_node->version > sdr_dump_max_version)
		sdr_dump_max_version = pmain_node->version;

	/***Handle*/
	pnode = fetch_node_by_index(pres , pmain_node->data.struct_value.child_idx);
	while(pnode)
	{
		//check class
		if(pnode->class != SDR_CLASS_ENTRY)
		{
			print_info(INFO_ERR , fp , "<%s> failed! entry not entry! name:%s class:%d" , __FUNCTION__ , pnode->class , pnode->node_name);
			return -1;
		}

		refer = 1; //如果不是数组则至少循环一次
		//check refer[数组]
		if(pnode->data.entry_value.count>0)
		{
			refer = pnode->data.entry_value.count; //refer == count

			if(pnode->data.entry_value.refer_idx>0)
			{
				refer = 0;	//set refer
				ptmp = fetch_node_by_index(pres , pnode->data.entry_value.refer_idx);
				if(!ptmp)
				{
					print_info(INFO_ERR , fp , "<%s> failed! refer of entry not found! entry name:%s refer_idx:%s" , __FUNCTION__ ,
							pnode->node_name , pnode->data.entry_value.refer_idx);
					return -1;
				}

				//get value of refer
				memcpy(&refer , data+ptmp->data.entry_value.offset , ptmp->size);
				//printf("entry name:%s refer name:%s refer:%d count:%d\n" , pnode->node_name , ptmp->node_name , refer,
				//		pnode->data.entry_value.count);
			}
		}

		do
		{
			//char[] 要特殊处理
			if((pnode->data.entry_value.entry_type==SDR_T_CHAR || pnode->data.entry_value.entry_type==SDR_T_UCHAR) && refer>1)
			{

				if(pnode->version > sdr_dump_max_version)
						sdr_dump_max_version = pnode->version;

				print_info(INFO_DISABLE , fp , "%s<entry name=\"%s\" type=\"%s[]\"  size=\"%d\" value=\"" , next_prefix , pnode->node_name ,
						reverse_label_type(pnode->data.entry_value.entry_type , NULL) , refer);
				for(i=0; i<refer; i++)
				{
					value = *((char *)(data+pnode->data.entry_value.offset+i));
					if (isprint(value))
						print_info(INFO_DISABLE , fp , "'%c'" , value);
					else
						print_info(INFO_DISABLE , fp , "'0x%X'" , value);
				}
				print_info(INFO_NORMAL , fp , "\" />");
				break;
			}

			//所有的都当成数组搞
			for(i=0; i<refer; i++)
			{
				//basic entry
				if(pnode->data.entry_value.entry_type>=SDR_T_CHAR && pnode->data.entry_value.entry_type<=SDR_T_MAX)
				{
					ret = dump_basic_info(pres , pnode->node_name , "" , next_prefix ,
							pnode , data+pnode->data.entry_value.offset+i*pnode->size , fp);
					if(ret < 0)
						return -1;
				}

				//struct
				if(pnode->data.entry_value.entry_type == SDR_T_STRUCT)
				{
					//找到该结构对应的struct
					ptmp = fetch_node_by_index(pres , pnode->data.entry_value.type_idx);
					if(!ptmp)
					{
						print_info(INFO_ERR , fp , "<%s>failed! get struct_type of entry failed! entry name:%s" , __FUNCTION__ , pnode->node_name);
						return -1;
					}

					//dump struct
					ret = dump_struct_info(pres , pnode->node_name , ptmp->node_name , next_prefix , ptmp , data+pnode->data.entry_value.offset+i*pnode->size ,
							fp);
					if(ret < 0)
						return -1;
				}

				//union
				if(pnode->data.entry_value.entry_type == SDR_T_UNION)
				{
					//找到选择该union对应的成员的变量
					ptmp = fetch_node_by_index(pres , pnode->data.entry_value.select_idx);
					if(!ptmp)
					{
						print_info(INFO_ERR , fp , "<%s> failed! select of entry not found! entry name:%s select_idx:%s" , __FUNCTION__ ,
								pnode->node_name , pnode->data.entry_value.select_idx);
						return -1;
					}
					if(ptmp->class != SDR_CLASS_ENTRY) //必须是个entry
					{
						print_info(INFO_ERR , fp , "<%s> failed! select of entry is not an entry! select-entry name:%s class:%s" , __FUNCTION__ ,
								ptmp->node_name , ptmp->class);
						return -1;
					}

					//获得变量值
					select = 0;
					memcpy(&select , data+ptmp->data.entry_value.offset , ptmp->size);

					//获得对应的union类型
					ptmp = fetch_node_by_index(pres , pnode->data.entry_value.type_idx);
					if(!ptmp)
					{
						print_info(INFO_ERR , fp , "<%s>failed! get union_type of entry failed! entry name:%s" , __FUNCTION__ , pnode->node_name);
						return -1;
					}

					//dump union
					ret = dump_union_info(pres , pnode->node_name , ptmp->node_name , next_prefix , ptmp , data+pnode->data.entry_value.offset+i*pnode->size ,
							select , fp);
					if(ret < 0)
						return -1;
				}

			}
			break;
		}while(0);

_continue:
		//next node
		pnode = fetch_node_by_index(pres , pnode->sibling_idx);
	}

	/***Print Tail*/
	print_info(INFO_NORMAL , fp , "%s</struct>" , prefix);
	return 0;
}

/*
 * dump联合体数据
 * @name  该数据在父结构里的成员名 (union不会在最外层)
 * @type_name 该数据的类型名
 * @prefix 缩进
 * @pmain_node 该数据的node
 * @data 该数据的起始地址
 * @select 使用的该联合内部的子成员id
 * @fp 打印文件句柄
 * @return:
 * 0  :  success
 * -1:  failed
 */
static int dump_union_info(sdr_data_res_t *pres , char *name , char *type_name , char *prefix , sdr_node_t *pmain_node , char *data , int my_select , FILE *fp)
{
	sdr_node_t *pnode = NULL;
	sdr_node_t *ptmp = NULL;
	long refer = 0;
	long select = 0;
	int i = 0;
	char next_prefix[128] = {0};
	char *pstr = NULL;
	int ret = 0;
	unsigned char value = 0;
	/***Arg Check*/
	//Basic Arg Not Check
	if(!type_name || strlen(type_name)<=0)
	{
		print_info(INFO_ERR , fp , "<%s> failed! type_name illegal!" , __FUNCTION__);
		return -1;
	}

	if(!data)
	{
		print_info(INFO_ERR , fp , "<%s> failed! data NULL! name:%s type:%s" , __FUNCTION__ , name , type_name);
		return -1;
	}

	if(pmain_node->class != SDR_CLASS_UNION)
	{
		print_info(INFO_ERR , fp , "<%s> failed! class %d not a struct! name:%s type:%s" , __FUNCTION__ , pmain_node->class , name , type_name);
		return -1;
	}

	/***Print Head*/
	snprintf(next_prefix , sizeof(next_prefix) , "%s%s" , prefix , SDR_DUMP_SPAN);
	print_info(INFO_NORMAL , fp , "%s<union name=\"%s\" type=\"%s\">" , prefix , name , type_name);

	if(pmain_node->version > sdr_dump_max_version)
		sdr_dump_max_version = pmain_node->version;

	/***Handle*/
	//根据my_select 获得 select == my_select的pnode
	pnode = fetch_node_by_index(pres , pmain_node->data.union_value.child_idx);
	while(pnode)
	{
		if(pnode->class != SDR_CLASS_ENTRY)
		{
			print_info(INFO_ERR , fp , "<%s> failed! union member class  not a entry! union type:%s member name:%s class:%d" , __FUNCTION__ , type_name , pnode->node_name ,
					pnode->class);
			return -1;
		}

		if(pnode->data.entry_value.select_id == my_select)
			break;

		pnode = fetch_node_by_index(pres , pnode->sibling_idx);
	}
	if(!pnode || pnode->data.entry_value.select_id != my_select)
	{
		print_info(INFO_ERR , fp , "<%s> failed! union member of %d not found! union:%s" , __FUNCTION__ , my_select , type_name);
		return -1;
	}

	//和struct类似 处理pnode
	//check class
	if(pnode->class != SDR_CLASS_ENTRY)
	{
		print_info(INFO_ERR , fp , "<%s> failed! entry not entry! name:%s class:%d" , __FUNCTION__ , pnode->class , pnode->node_name);
		return -1;
	}

	refer = 1; //如果不是数组则至少循环一次
	//check refer[数组] 实际union里的成员不可能有refer
	if(pnode->data.entry_value.count>0)
	{
		refer = pnode->data.entry_value.count; //refer == count
	}

	do
	{
		//char[] 要特殊处理
		if((pnode->data.entry_value.entry_type==SDR_T_CHAR || pnode->data.entry_value.entry_type==SDR_T_UCHAR) && refer>1)
		{
			if(pnode->version > sdr_dump_max_version)
				sdr_dump_max_version = pnode->version;

			print_info(INFO_DISABLE , fp , "%s<entry name=\"%s\" type=\"%s[]\"  size=\"%d\" value=\"" , next_prefix , pnode->node_name ,
					reverse_label_type(pnode->data.entry_value.entry_type , NULL) , refer);
			for(i=0; i<refer; i++)
			{
				value = *((char *)(data+pnode->data.entry_value.offset+i));
				if (isprint(value))
					print_info(INFO_DISABLE , fp , "'%c'" , value);
				else
					print_info(INFO_DISABLE , fp , "'0x%X'" , value);
			}
			print_info(INFO_NORMAL , fp , "\" />");
			break;
		}
		/*
		if(pnode->data.entry_value.entry_type==SDR_T_CHAR && pnode->data.entry_value.count>1)
		{
			pstr = (char *)calloc(1 , pnode->data.entry_value.count);
			if(!pstr)
			{
				print_info(INFO_ERR , fp , "<%s> failed! calloc string size:%d failed! entry name:%s err:%s" , __FUNCTION__ , pnode->data.entry_value.count ,
						pnode->node_name , strerror(errno));
				return -1;
			}
			strncpy(pstr , data+pnode->data.entry_value.offset , pnode->data.entry_value.count);
			print_info(INFO_NORMAL , fp , "%s<entry name=\"%s\" type=\"%s\"  value=\"%s\" />" , next_prefix , pnode->node_name , "char" ,
					pstr);
			free(pstr);

			break;
		}*/

		//所有的都当成数组搞
		for(i=0; i<refer; i++)
		{
			//basic entry
			if(pnode->data.entry_value.entry_type>=SDR_T_CHAR && pnode->data.entry_value.entry_type<=SDR_T_MAX)
			{
				ret = dump_basic_info(pres , pnode->node_name , "" , next_prefix ,
						pnode , data+pnode->data.entry_value.offset+i*pnode->size , fp);
				if(ret < 0)
					return -1;
			}

			//struct
			if(pnode->data.entry_value.entry_type == SDR_T_STRUCT)
			{
				//找到该结构对应的struct
				ptmp = fetch_node_by_index(pres , pnode->data.entry_value.type_idx);
				if(!ptmp)
				{
					print_info(INFO_ERR , fp , "<%s>failed! get struct_type of entry failed! entry name:%s" , __FUNCTION__ , pnode->node_name);
					return -1;
				}

				//dump struct
				ret = dump_struct_info(pres , pnode->node_name , ptmp->node_name , next_prefix , ptmp , data+pnode->data.entry_value.offset+i*pnode->size ,
						fp);
				if(ret < 0)
					return -1;
			}

			//union[实际不可能为union 因为没有邻居select同时存在]

		}
		break;

	}while(0);

	/***Print Tail*/
	print_info(INFO_NORMAL , fp , "%s</union>" , prefix);
	return 0;
}

/*
 * dump基础类型数据
 * @name  该数据在父结构里的成员名 如果为""则是最外层
 * @type_name 该数据的类型名
  * @prefix 缩进
 * @pmain_node 该数据的node
 * @data 该数据的起始地址
 * @fp 打印文件句柄
 * @return:
 * 0  :  success
 * -1:  failed
 */
static int dump_basic_info(sdr_data_res_t *pres , char *name , char *type_name , char *prefix , sdr_node_t *pmain_node , char *data , FILE *fp)
{
	char *my_type = NULL;
	char format[32] = {0};
	char content[SDR_LINE_LEN] = {0};
	long value = 0; //max basic size
	long long ll_value = 0;
	double double_value = 0;
	float float_value = 0;
	my_type = reverse_label_type(pmain_node->data.entry_value.entry_type , format);
	if(!my_type)
	{
		print_info(INFO_ERR , fp , "<%s> failed! reverse_label_type failed! entry name:%s type:%d" , pmain_node->node_name ,
				pmain_node->data.entry_value.entry_type);
		return -1;
	}

	if(pmain_node->version > sdr_dump_max_version)
		sdr_dump_max_version = pmain_node->version;

	strncpy(content ,  "%s<entry name=\"%s\" type=\"%s\" value=\"" , sizeof(content));
	strcat(content , format);
	strcat(content , "\" />");


	do
	{
		if(pmain_node->data.entry_value.entry_type == SDR_T_FLOAT)
		{
			memcpy(&float_value , data , pmain_node->size);
			print_info(INFO_NORMAL , fp , content , prefix , pmain_node->node_name , my_type , float_value);
			break;
		}

		if(pmain_node->data.entry_value.entry_type == SDR_T_DOUBLE)
		{
			memcpy(&double_value , data , pmain_node->size);
			print_info(INFO_NORMAL , fp , content , prefix , pmain_node->node_name , my_type , double_value);
			break;
		}

		if(pmain_node->data.entry_value.entry_type == SDR_T_LONGLONG)
		{
			memcpy(&ll_value , data , pmain_node->size);
			print_info(INFO_NORMAL , fp , content , prefix , pmain_node->node_name , my_type , ll_value);
			break;
		}

		//other
		memcpy(&value , data , pmain_node->size);
		print_info(INFO_NORMAL , fp , content , prefix , pmain_node->node_name , my_type , value);
		break;
	}while(0);

	//基本类型不会有下一层缩进了
	//print_info(INFO_NORMAL , fp , content , prefix , pmain_node->node_name , my_type , value);
	return 0;
}
