/*
 * sdrconv.h
 *
 *  Created on: 2015-3-17
 *      Author: soullei
 */

#ifndef SDRCONV_H_
#define SDRCONV_H_

#include "sdr.h"


#define SDR_INFO_NORMAL 1
#define SDR_INFO_MAIN	2
#define SDR_INFO_ERR	3
#define SDR_INFO_PANIC 4


/***********STRUCT*/
/***全局配置*/
struct _sdr_conv_env
{
	char is_reverse;	//0:xml->bin 1:bin->xml
	int max_node;	//最多节点
	char input_name[SDR_NAME_LEN];
	char output_name[SDR_NAME_LEN];

	/*符号表*/
	sym_table_t *psym_table;

	/*映射表*/
	sdr_node_map_t *pnode_map;

	/*Input*/
	FILE *in_fp;
	int curr_line;
	char src_line[SDR_LINE_LEN];	//读入的行
	char working_line[SDR_LINE_LEN];	//工作的行

	/*Output*/
	sdr_data_res_t sdr_res;	//输出结构
	int out_fd;	//输出文件描述符

	/*Misc*/
	char debug_info;	//是否打印debug信息
	sdr_node_t *pcurr_main_node;	//当前所指主节点
};
typedef struct _sdr_conv_env sdr_conv_env_t;


extern int sdr_parse_comment(sdr_conv_env_t *penv);
extern sdr_node_t *sdr_parse_macro(sdr_conv_env_t *penv);
extern sdr_node_t *sdr_parse_struct(sdr_conv_env_t *penv);
extern sdr_node_t *sdr_parse_union(sdr_conv_env_t *penv);
extern int sdr_gen_h(sdr_conv_env_t *penv);
extern int sdr_gen_bin(sdr_conv_env_t *penv);
extern int sdr_reverse_bin(sdr_conv_env_t *penv);

extern sdr_node_t *get_node(sdr_conv_env_t *penv);
extern int read_one_line(sdr_conv_env_t *penv);
extern int forward_miss_char(char *src , char miss);
extern int forward_to_char(char *src , char target);
extern int copy_str(char *dest , char *src);
extern int insert_sym_table(sdr_conv_env_t *penv , char *sym_name , int index);
extern int fetch_sym_map_index(sdr_conv_env_t *penv , char *sym_name);
extern int sdr_print_info(sdr_conv_env_t *penv , char type , ...);

#endif /* SDRCONV_H_ */
