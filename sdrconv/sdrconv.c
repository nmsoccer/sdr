/*
 * sdrconv.c
 *
 *  Created on: 2015-3-11
 *      Author: soullei
 */
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "sdr.h"
#include "sdrconv.h"



/**********LOCAL DATA*/
static sdr_conv_env_t sdr_conv_env;

/**********LOCAL FUNCTION*/
static void sdr_show_help(void);
static int sdr_parse_xml(sdr_conv_env_t *penv);
static int sdr_end_convert(sdr_conv_env_t *penv , char result);

int main(int argc , char **argv)
{
	sdr_conv_env_t *penv;
	sdr_data_res_t *pres;
	char opt;
	int i;
	int ret;

	penv = &sdr_conv_env;
	memset(&sdr_conv_env , 0 , sizeof(sdr_conv_env_t));
	sdr_conv_env.max_node = DEFAULT_MAX_NODE_COUNT;

	/***Get Opt*/
	while((opt = getopt(argc , argv , "vRI:s:hO:d")) != -1)
	{

		switch(opt)
		{
		case 'v':
			printf("sdrconv 1.0");
			return 0;
		case 's':
			sdr_conv_env.max_node = atoi(optarg);
			break;
		case 'R':
			sdr_conv_env.is_reverse = 1;
			break;
		case 'd':
			sdr_conv_env.debug_info = 1;
			break;
		case 'I':
			strncpy(sdr_conv_env.input_name , optarg , SDR_NAME_LEN);
			break;
		case 'O':
			strncpy(sdr_conv_env.output_name , optarg , SDR_NAME_LEN);
			break;
		case 'h':
		default:
			sdr_show_help();
			return 0;
		}
	}

	/***Check Arg*/
	if(strlen(sdr_conv_env.input_name) <= 0)
	{
		//printf("Please Use 'sdrconv -h' \n");
		sdr_show_help();
		return -1;
	}

		//反转格式单独函数处理
	if(sdr_conv_env.is_reverse == 1)
		return sdr_reverse_bin(penv);


	if(sdr_conv_env.max_node <= 0)
	{
		sdr_conv_env.max_node = DEFAULT_MAX_NODE_COUNT;
	}

	/***Init*/
	//Modify Output Name
	if(strlen(sdr_conv_env.output_name) <= 0)
	{
		for(i=0; i<strlen(sdr_conv_env.input_name); i++)
		{
			if(sdr_conv_env.input_name[i] == '.')
				break;
			sdr_conv_env.output_name[i] = sdr_conv_env.input_name[i];
		}

		//add .sdr
		if(strlen(sdr_conv_env.output_name)+1+4 <= SDR_NAME_LEN)
		{
			strcat(sdr_conv_env.output_name , ".sdr");
		}

		sdr_conv_env.output_name[strlen(sdr_conv_env.output_name)] = 0;
	}

	//Open File
	penv->in_fp = fopen(penv->input_name , "r");
	if(penv->in_fp == NULL)
	{
		printf("Open %s Failed!\n" , penv->input_name);
		return -1;
	}

	penv->out_fd = open(penv->output_name , O_RDWR|O_CREAT|O_TRUNC , 0644);
	if(penv->out_fd < 0)
	{
		printf("Open %s Failed!\n" , penv->output_name);
		return -1;
	}

	//Malloc Memory
	penv->psym_table = (sym_table_t *)malloc(sizeof(sym_table_t) + penv->max_node * sizeof(sym_entry_t));
	if(penv->psym_table == NULL)
	{
		printf("Malloc Memory to Sym Table Failed!. Need Use:0x%x\n" , sizeof(sym_table_t) + penv->max_node * sizeof(sym_entry_t));
		return -1;
	}

	penv->pnode_map = (sdr_node_map_t *)malloc(sizeof(sdr_node_map_t) + penv->max_node * sizeof(sdr_node_t));
	if(penv->pnode_map == NULL)
	{
		printf("Malloc Memory to Node Map Failed!. Need Use:0x%x\n" , sizeof(sdr_node_map_t) + penv->max_node * sizeof(sdr_node_t));
		return -1;
	}

	//Init Res
	strncpy(penv->sdr_res.magic , SDR_MAGIC_STR , sizeof(penv->sdr_res.magic));
	penv->sdr_res.max_node = penv->max_node;
	penv->sdr_res.pnode_map = penv->pnode_map;
	penv->sdr_res.psym_table = penv->psym_table;

	/***Handle*/
	do
	{
		//Parse XML
		ret = sdr_parse_xml(penv);
		if(ret < 0)
			break;

		//Gen H File
		ret = sdr_gen_h(penv);
		if(ret < 0)
			break;

		//Gen Bin File
		ret = sdr_gen_bin(penv);

	}while(0);

	/***Convert End*/
	sdr_end_convert(penv , ret);
	return 0;
}

/*
 * 解析整个XML文件，并将相关节点放入sym_table和node_map中
 */
static int sdr_parse_xml(sdr_conv_env_t *penv)
{
	sdr_node_t *pnode;
	int ret;

	/***Arg Check*/
	if(!penv)
		return -1;

	printf("Ready to Parse %s\n" , penv->input_name);

	/***Handle*/
	//检查文件头
	if(read_one_line(penv) < 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Not a Xml File!\n");
		return -1;
	}

	penv->working_line[strlen("<?xml")] = 0;
	if(strcmp(penv->working_line , "<?xml") != 0)
	{
		sdr_print_info(penv , SDR_INFO_ERR , "Not a Xml File!\n");
		return -1;
	}

	//首先注册一个start_node
	pnode = get_node(penv);
	if(!pnode)
		exit(-1);

	pnode->class = SDR_CLASS_START;
	penv->pcurr_main_node = pnode;	//当前工作的主节点

	do
	{
		//read one line
		if(read_one_line(penv) < 0)
			break;

		//寻找'<'
		ret = forward_to_char(penv->working_line , '<');
		if(ret == -1)
		{
			sdr_print_info(penv , SDR_INFO_ERR , "When Searching '%c" , XML_LEFT_BRACKET);
			continue;
		}
		if(ret == -2)	//没有'<'
			continue;

		//找到<符号,检查下一个符号
		switch(penv->working_line[1])
		{
		case '!':	//注释
			ret = sdr_parse_comment(penv);
			if(ret == 0)
				sdr_print_info(penv , SDR_INFO_NORMAL , "Parse Comment Success!");
			else
				return -1;
			break;
		case 'm':	//macro
			pnode = sdr_parse_macro(penv);
			if(pnode)
			{
				sdr_print_info(penv , SDR_INFO_NORMAL , "Macro Name:%s,Value:%s,Desc:%s" , pnode->node_name , pnode->data.macro_value , pnode->node_desc);
				//放入hash表
				ret = insert_sym_table(penv , pnode->node_name , pnode->my_idx);
				if(ret < 0)
					return -1;

				//弄成串串
				penv->pcurr_main_node->sibling_idx = pnode->my_idx;
				penv->pcurr_main_node = pnode;
			}
			else
				return -1;
			break;
		case 's':	//struct
			pnode = sdr_parse_struct(penv);
			if(pnode)
			{
				sdr_print_info(penv , SDR_INFO_NORMAL , "Struct Name:%s,Version:%d,Size:%d,Desc:%s" , pnode->node_name , pnode->version , pnode->size , pnode->node_desc);
				//放入hash表
				ret = insert_sym_table(penv , pnode->node_name , pnode->my_idx);
				if(ret < 0)
					return -1;

				//弄成串串
				penv->pcurr_main_node->sibling_idx = pnode->my_idx;
				penv->pcurr_main_node = pnode;
			}
			else
				return -1;
			break;
		case 'u':	//union
			pnode = sdr_parse_union(penv);
			if(pnode)
			{
				sdr_print_info(penv , SDR_INFO_NORMAL , "Union Name:%s,Version:%d,Size:%d,Desc:%s" , pnode->node_name , pnode->version , pnode->size , pnode->node_desc);
				//放入hash表
				ret = insert_sym_table(penv , pnode->node_name , pnode->my_idx);
				if(ret < 0)
					return -1;

				//弄成串串
				penv->pcurr_main_node->sibling_idx = pnode->my_idx;
				penv->pcurr_main_node = pnode;
			}
			else
				return -1;
			break;
		default:
			sdr_print_info(penv , SDR_INFO_ERR , "illegal character after '<");
			return -1;
			break;
		}
	}while(1);
	return 0;
}



static int sdr_end_convert(sdr_conv_env_t *penv , char result)
{
	/***Arg Check*/
	if(!penv)
		return -1;

	if(penv->psym_table)
		free(penv->psym_table);

	if(penv->pnode_map)
		free(penv->pnode_map);

	if(result == 0)
		printf("sdr converting success!\n");
	return 0;
}



static void sdr_show_help(void)
{
	printf("-v \t show version\n");
	printf("-d \t show debug info\n");
	printf("-R \t reverse xx.sdr to xxx.sdr.xml\n");
	printf("-s xx\t specify node number when conversing! if memory not enough\n");
	printf("-I <input file>\t specify iuput file\n");
	printf("-O <output file>\t specify output file\n");
	printf("-h \t display help info\n");
}


