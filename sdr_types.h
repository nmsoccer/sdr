/*
 * sdr_types.h
 *
 *  Created on: 2017-2-19
 *      Author: leiming
 */

#ifndef SDR_TYPES_H_
#define SDR_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/************************宏****************/
enum SDR_BASE_CLASS	//XML文档基本节点.主节点是构成XML第一层次的节点，不包含SDR_CLASS_ENTRY
{
	SDR_CLASS_START,	//起始
	SDR_CLASS_MACRO,	//宏常量
	SDR_CLASS_STRUCT,	//结构体
	SDR_CLASS_UNION,	//联合
	SDR_CLASS_ENTRY		//结构/联合成员
};

enum SDR_TYPE	//数据类型
{
	SDR_T_UNION = 1,	//联合
	SDR_T_STRUCT,	//结构体
	SDR_T_COMPOS,	//占位复合类型
	SDR_T_CHAR,	//字符
	SDR_T_UCHAR, //无符号字符
	SDR_T_SHORT,	//短整形
	SDR_T_USHORT,	//无符号
	SDR_T_INT,	//整形
	SDR_T_UINT,	//无符号整形
	SDR_T_LONG,	//长整形
	SDR_T_ULONG,	//无符号长整形
	SDR_T_FLOAT,	//浮点
	SDR_T_DOUBLE, //双精度浮点
	SDR_T_LONGLONG, //长长整型
	SDR_T_MAX = SDR_T_LONGLONG //max
};

#define XML_LEFT_BRACKET '<'
#define XML_RIGHT_BRACKET '>'
#define XML_LABEL_MACRO "macro"
#define XML_LABEL_STRUCT "struct"
#define XML_LABEL_UNION "union"
#define XML_LABEL_ENTRY "entry"
#define XML_LABEL_NAME "name"
#define XML_LABEL_VALUE "value"
#define XML_LABEL_TYPE "type"
#define XML_LABEL_COUNT "count"
#define XML_LABEL_REFER "refer"
#define XML_LABEL_SELECT "select"
#define XML_LABEL_ID "id"
#define XML_LABEL_VERSION "version"
#define XML_LABEL_DESC "desc"

//类型关键字
#define XML_LABEL_CHAR "char"
#define XML_LABEL_UCHAR "uchar"
#define XML_LABEL_SHORT "short"
#define XML_LABEL_USHORT "ushort"
#define XML_LABEL_INT "int"
#define XML_LABEL_UINT "uint"
#define XML_LABEL_LONG "long"
#define XML_LABEL_ULONG "ulong"
#define XML_LABEL_FLOAT "float"
#define XML_LABEL_DOUBLE "double"
#define XML_LABEL_LONGLONG "long long"

#define SDR_NAME_LEN	64	//名字长度
#define SDR_DESC_LEN	64	//注释长度
#define SDR_LINE_LEN 1024	 //行长度
#define MACRO_VALUE_LEN 64	//MACRO 值长度

#define INFO_NORMAL	1
#define INFO_MAIN	2
#define INFO_ERR	3
/************************结构****************/
/*
 * 一个类型的节点
 */
struct _sdr_node
{
	char class;	//基本节点类型
	int my_idx;	//该节点所在map中的序号
	int version;	//版本号
	int size;	//字节长度
	char node_name[SDR_NAME_LEN];
	char node_desc[SDR_DESC_LEN];
	int sibling_idx;	//兄弟节点
	union
	{
		char macro_value[MACRO_VALUE_LEN];	//SDR_MACRO
		struct	//SDR_STRUCT
		{
			int child_idx;	//第一个entry的index
		}struct_value;

		struct	//SDR_UNION
		{
			int child_idx;	//第一个entry的index
		}union_value;

		struct	//SDR_ENTRY
		{
			char entry_type;	//entry的类型
			int type_idx;	//如果是复合类型(struct/union)其对应的节点ID
			int count;	//数目:如果1则是普通类型 >1数组
			char count_name[SDR_NAME_LEN];	//如果是宏，则宏名放到这里
			int refer_idx;	//如果是数组，需要参考的变量
			int select_idx;	//如果entry类型是union，则该标记应该会记录设置其具体ID的某成员
			int select_id;	//如果是union的成员，则记录其ID
			char id_name[SDR_NAME_LEN];	//id的名字
			int offset;	//该entry相对于父结构的偏移
		}entry_value;
	}data;
}__attribute__((packed));
typedef struct _sdr_node sdr_node_t;

#define DEFAULT_MAX_NODE_COUNT (20*1024)	//默认NODE 20K数目
/*
 * MEM<->FILE MAP 用于记录所有节点的数据结构，同时也可持久化到BIN文件中
 * 顺序存储
 */
struct _sdr_node_map
{
	int count;
	sdr_node_t node_list[0];
}__attribute__((packed));
typedef struct _sdr_node_map sdr_node_map_t;


/***符号表(只存放SDR_MACRO SDR_STRUCT 和 SDR_UNION 不存放SDR_ENTRY成员名)*/
struct _sym_entry
{
	char sym_name[SDR_NAME_LEN];
	int index;		//index of node table
}__attribute__((packed));
typedef struct _sym_entry sym_entry_t;

struct _sym_table
{
	int count;
	sym_entry_t entry_list[0];
}__attribute__((packed));
typedef struct _sym_table sym_table_t;

//这是放入文件里的符号表，根据内存里的符号表紧缩排列，减小sdr尺寸
struct _packed_sym_entry
{
	int pos;	// pos of sym_table.entry_list[];
	sym_entry_t entry;
}__attribute__((packed));
typedef struct _packed_sym_entry packed_sym_entry_t;

struct _packed_sym_table
{
	int sym_list_size;   // size of sym_table.entry_list
	int my_list_size; //size of this.entry_list
	packed_sym_entry_t entry_list[0];
}__attribute__((packed));
typedef struct _packed_sym_table packed_sym_table_t;

/***持久化结构*/
#define SDR_MAGIC_STR	"!sdr"

struct _sdr_data_res
{
	char magic[4];		//magic
	int max_node;
	sdr_node_map_t *pnode_map;
	sym_table_t *psym_table;
}__attribute__((packed));
typedef struct _sdr_data_res sdr_data_res_t;

/***打包解包INFO*/
struct _sdr_buff_info
{
	char *src;
	int index;
	int length;	//解包需要
};
typedef struct _sdr_buff_info sdr_buff_info_t;

#ifdef __cplusplus
}
#endif

#endif /* SDR_TYPES_H_ */
