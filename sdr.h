/*
 * sdr.h
 *
 *  Created on: 2015-3-11
 *      Author: soullei
 *
 *      主要功能:将符合规则的XML文件转换为对应的.h头文件，同时持久化为解析的.bin文件，用于打包解包
 *      目标XML文件的规则请见参考文档
 */

#ifndef SDR_H_
#define SDR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sdr_types.h"

/******************EXTERN*/
/*
 * 加载目标sdr文件入内存
 */
sdr_data_res_t *sdr_load_bin(char *file_name , FILE *log_fp);

/*
 * 释放加载入内存的sdr文件
 */
int sdr_free_bin(sdr_data_res_t *pres);

/*
 * 打包in_buff数据到out_buff缓冲区
 * 只打包版本号小于version的数据
 * @type_name 类型名
 * out_buff前八个字节为两个整形 分别是version+length
 * net_byte:打包成网络序(0:直接打包 1:转成网络序)
 *   若打开网络字节转换 浮点型将透传字节，不进行转换
 *   若可以保证打解包双方大小端一致可以关闭网络转换以提高性能，同时兼容浮点型
 * return:>=0压缩后的数据
 * else:错误
 */
int sdr_pack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , int version , char net_byte , FILE *log_fp);

/*
 * 解包in_buff数据到out_buff
 * 只解包版本号小于version的数据
 * @type_name 类型名
 * in_buff前八个字节为两个整形 分别是version+length
 * net_byte:从网络序解包(0:直接解包 1:从网络序解包)
 * return:>=0解压缩后的数据
 * else:错误
 */
int sdr_unpack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , char net_byte , FILE *log_fp);


//扩展接口 appended on 2017-02-25
/*
 * 获得某成员相对父结构之偏移[打包前或者解包后之正常结构]
 * @type_name:结构体名
 * @member_name:成员名
 * @return
 * >=0:偏移量
 * -1:Failed
*/
int sdr_member_offset(sdr_data_res_t *pres , char *type_name , char *member_name);

/*
 * 获得某结构体当前成员的下一个成员名
 * @type_name:结构体名
 * @curr_member:当前成员名;如果为空则是表明是第一个
 * @next_member:基于curr_member的下一个成员名
 * @len:缓冲区长度
 * @return:
 * >=0:下一个成员相对该结构之偏移
 * -1:错误
 * -2:没有下一个成员
 */
int sdr_next_member(sdr_data_res_t *pres , char *type_name , char *curr_member , char *next_member , int len);


#define SDR_DUMP_SPAN "    " //four space
/*
 * 打印结构体的数据信息
 * @type_name:xml里定义的结构体名
 * @结构体数据的起始地址
 * @打印到的目标文件
 * @return:
 * 0 : success
 * -1:failed
 */
int sdr_dump_struct(sdr_data_res_t *pres , char *type_name , char *struct_data , FILE *fp);

//supported
int sdr_bin2xml(sdr_data_res_t *pres , char *file_name , FILE *log_fp);
unsigned int BKDRHash(char *str);
char *reverse_label_type(char sdr_type , char *format_buf);

#ifdef __cplusplus
}
#endif

#endif /* SDR_H_ */
