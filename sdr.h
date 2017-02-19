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

#include "sdr_types.h"

/******************EXTERN*/
sdr_data_res_t *sdr_load_bin(char *file_name , FILE *log_fp);
int sdr_free_bin(sdr_data_res_t *pres);
int sdr_bin2xml(sdr_data_res_t *pres , char *file_name , FILE *log_fp);
int sdr_pack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , int version , FILE *log_fp);
int sdr_unpack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , FILE *log_fp);

unsigned int BKDRHash(char *str);
char *reverse_label_type(char sdr_type);

#endif /* SDR_H_ */
