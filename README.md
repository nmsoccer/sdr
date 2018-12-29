# SDR
A Simple Data Representation  
一种简单的数据表示及序列反序列化接口.本文将简单介绍sdr的特性与使用方法，同时给出使用范例，并在文末提供与protbuf简单的性能比较

- **数据描述** 通过XML文件描述数据结构
- **有限规则** 支持少数几种严格限制的标签即可描述C语言所使用的结构及数据类型
- **转换工具** 提供转换工具将xml文本转换为协议描述文件，同时支持将描述文件反转为xml结构化文本
- **静态分配** 与protobuf-c不同，被描述的嵌套结构体在外层结构体定义时内部结构体同时统一静态定义，不需要protobuf-c一样只定义指针而需要开发者针对每个内部结构体一一动态分配
- **打包解包** 将数据结构序列化为二进制数据，及对二进制流反序列化
- **版本兼容** 对每个标签赋予版本号来支持版本兼容


## 数据描述
_协议数据通过xml文件进行定义，为了解析与协议的规范性，只支持少数几种标签，同时对部分空格的使用进行了严格限制_  

### 协议示例
```
<?xml version="1.0" encoding="utf8" ?>
<!--
基本格式如下 
 -->

<!--  VERSION -->
<macro name="PROTO_VERSION" value="1" />

<!--  技能 -->
<macro name="Q_SKILL" value="1" />
<macro name="W_SKILL" value="2" />
<macro name="E_SKILL" value="3" />
<macro name="R_SKILL" value="4" />
<union name="skill_data" >
    <entry name="qskill" type="char" id="Q_SKILL" />
    <entry name="wskill" type="uint" id="W_SKILL" />
    <entry name="eskill" type="uchar" id="E_SKILL" count="12"/>
    <entry name="rskill" type="short" id="R_SKILL" />
</union>
<struct name="skill_info">
    <entry name="type" type="char" />
    <entry name="data" type="skill_data" select="type" />
</struct>

<!-- 技能列表 -->
<macro name="MAX_SKILL_COUNT" value="32" />
<struct name="skill_list">    
    <entry name="skill_count" type="char" />        
    <entry name="info_list" type="skill_info" count="MAX_SKILL_COUNT" refer="skill_count" />    
</struct>

<!--  玩家信息 -->
<macro name="MAX_NAME_LEN" value="32" />
<struct name="user_info" version="1">
    <entry name="sex" type="char" desc="性别" />
    <entry name="name_len" type="char" />
    <entry name="user_name" type="char" count="MAX_NAME_LEN" refer="name_len" desc="姓名"/>
    <entry name="age" type="short" desc="年龄" />
    <entry name="skill" type="skill_list" desc="技能列表" />
    <entry name="money" type="long long" version="2" desc="金币" />
    <entry name="gold" type="ulong" version="3" desc="金币" />
</struct>

```

### 标签
- **总约束** 
  * 每个标签必须要有name属性用来定义标签名
  * 每个标签可选desc属性作为注释
  * 每个标签可选version属性，如果不定义则以打包时传入的版本号作为该标签所属版本
  * 所有属性值需以""双引号包围,各属性之间需空格分隔
  * 名字长度，注释长度及value长度不应大于64字节
  
- **```<macro name="xx" value="xx"/>```** 单行宏定义
  * name  宏名 (必须)
  * value 宏值 (必须)
  * desc  注释 [可选]
- **```<struct name="xx" ...><entry name="xx" .../></struct>```** 结构体.```<struct>```与```</struct>```需成对出现并位于不同行
  * name  结构体名 (必须)
  * desc  注释 [可选]
  * version 版本号 [可选]
  * ```<entry />``` 结构体成员 (必须)
- **```<union name="xx" ...><entry name="xx" .../></union>```** 联合体.```<union>```与```</union>```需成对出现并位于不同行
  * name  联合体名 (必须)
  * desc  注释 [可选]
  * version 版本号 [可选]
  * ```<entry />``` 联合体成员 (必须)
- **```<entry name="xx" type="xx" .../>```** 结构&联合体成员，必须放置于结构体或联合体内部使用  
  * name 成员名 (必须)
  * type 类型 (必须)
  * count 该类型数目.用于定义数组 (可选)
  * refer 如果定义了count，该属性用于关联一个兄弟entry用来说明该数组的实际长度.如果没有refer则用count的实际长度打包 (可选)
  * id 用于联合体中，用来给该联合体内的entry指定唯一的序号，用于打包 (联合体成员必须)
  * select 用于type=union的entry中，用来关联一个兄弟entry的值，从而选择哪个union的成员来打解包 (联合体类型必须)
  
### 类型
|    类型   |   说明         | 32位 |64位|
|:-------:  |:-------------:|:----:|:---:|
| char      | 符号字符型     |  1Byte   | 1Byte |
| uchar     | 无符号字符型   |  1Byte   | 1Byte |
| short     | 符号短整型     | 2Bytes    | 2Bytes |
| ushort    | 无符号短整型   | 2Bytes    | 2Bytes |
| int       | 符号整型       | 4Bytes   | 4Bytes  |
| uint      | 无符号整型     | 4Bytes   | 4Bytes  |
| long      | 符号长整型     | 4Bytes   | 8Bytes  |
| ulong     | 无符号长整型   | 4Bytes   | 8Bytes  |
| long long | 符号长长整型   | 8Bytes   | 8Bytes  |
| float     | 浮点(暂不支持) | 4Bytes   | 4Bytes  |
| double    | 双精度浮点(暂不支持) | 8Bytes | 8Bytes |

## 安装使用
### 安装
1. 下载sdr.zip到本地目录
2. unzip解压缩 然后进入sdr目录
3. 在sdr目录下执行./install.sh(注意需root权限)
4. 将会在/usr/local/bin下安装sdrconv程序
5. 将会在/usr/local/include/sdr/目录下存放头文件
6. 将会在/usr/local/lib/下安装libsdr.so动态库

### 工具
sdrconv用于生成与解析*.sdr协议描述文件
1. sdrconv -I demo.xml 将解析xml协议文件并生成demo.sdr协议描述文件
2. sdrconv -R -I demo.sdr 将解析demo.sdr协议描述文件并生成对应的demo.sdr.xml文本化描述文件(原有的注释会被丢弃)  
3. 在转换时推荐使用conv2sdr.sh demo.xml 脚本进行sdr协议文件生成

### 编译(以demo为例)
1. 在demo目录下创建协议文件demo.xml
2. 在demo目录下执行conv2sdr.sh demo.xml 如果成功执行将会生成demo.h
3. gcc -g demo.c -lsdr -o demo 生成可执行文件
_如果找不到动态库，需要将/usr/local/lib加入/etc/ld.so.conf 然后执行/sbin/ldconfig_
4. 或者直接将源文件编译  
   gcc -g demo.c ../sdr.c -o demo


## API
### 基本
- ```sdr_data_res_t *sdr_load_bin(char *file_name , FILE *log_fp);```
  * 加载由sdrconv生成的*.sdr协议描述文件
  * file_name 通过sdrconv生成的*.sdr协议描述文件
  * log_fp 提供给应用程序的日志接口 如果不需要设置为NULL
  * return 成功则返回协议描述指针sdr_data_res_t*; 失败返回NULL
  
- ```int sdr_pack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , int version , FILE *log_fp);```
  * 序列化数据结构(版本号将被带入序列化的二进制数据)
  * pres 通过sdr_load_bin成功返回的协议描述符句柄
  * pout_buff 序列化数据后存储的地址(需自己定义足够长度)
  * pin_buff  被序列化的数据地址
  * type_name 在xml里定义的数据结构名
  * version   序列化当前数据的版本号（version>该版本号的成员将不会被序列化）
  * log_fp    应用程序传入的日志句柄 or NULL
  * return    成功则返回序列化之后的数据长度; 失败返回-1
  
- ```int sdr_unpack(sdr_data_res_t *pres , char *pout_buff , char *pin_buff , char *type_name , FILE *log_fp);```  
  * 反序列化二进制数据(版本号已经被打入序列化的二进制数据,version>二进制数据版本号的成员将被跳过)
  * pres 通过sdr_load_bin成功返回的协议描述符句柄
  * pout_buff 反序列化后的数据结构地址
  * pin_buff  被反序列化的二进制数据地址
  * type_name 在xml里定义的数据结构名
  * log_fp    应用程序传入的日志句柄 or NULL
  * return    成功则返回反序列化之后的数据长度; 失败返回-1

- ```int sdr_free_bin(sdr_data_res_t *pres);```  
  * 释放生成的*.sdr协议描述文件  
  
### 扩展
- ```int sdr_member_offset(sdr_data_res_t *pres , char *type_name , char *member_name);```  
  * 获得一个结构内成员相对父结构的偏移
  * type_name 结构名
  * member_name 成员名
  * return >=0 偏移量; -1 失败
  
- ```int sdr_next_member(sdr_data_res_t *pres , char *type_name , char *curr_member , char *next_member , int len);```
  * 获得当前结构体成员的下一个成员名
  * type_name 结构名
  * curr_member 当前成员名(NULL则为第一个)
  * next_member 返回的下一个成员名地址
  * len 下一个成员名缓冲区长度
  * return >=0 下个成员之偏移 -1:错误 -2:无下一个成员

- ```int sdr_dump_struct(sdr_data_res_t *pres , char *type_name , char *struct_data , FILE *fp);```  
  * @pres:成功加载的sdr描述符指针  
  * @type_name:将要dump的数据结构名.注意是在xml里定义，而不是生成的.h里的结构名  
  * @struct_data:内存里的结构体起始地址  
  * @fp:dump出的文件句柄
  * @return: -1 failed; 0 success

## 版本兼容
### 约束
- 序列化数据结构时，所有数据结构中version大于传入版本号的成员将不会处理
- 反序列化二进制数据时，目标结构version大于二进制versio的字段将不会处理

### 细则
- xml定义的结构若未指定version 则该结构version默认为最低值0
- 序列化输入的版本号必须>=目标结构定义时设置的版本号(如果不显示设置则为0)
- 结构&联合体一经定义，不应重新设置version
- 结构&联合体内已有字段不应重新设置version
- 结构&联合体内新增字段需要添加version属性，且属性值应高于该数据结构最近一次序列化所使用的版本号


### 实例
- 假设当前对某结构体user_info进行version=1的序列化操作，那么user_info里version>1的成员将不会序列化
- 修改结构体user_info 新增成员entry(注意 成员只能增不能减)
- 现在对user_info新加成员version=2,然后将原有version=1的序列化数据进行反序列化，则version=2的成员不受影响，其他<=1的成员会成功赋值
- 后面只需要按照version=2进行序列化即可

### 代码
- 我们使用上面的xml来定义一个user_info结构，并设置其version=1, 然后再代码里使用这个结构，并根据不同的协议号对其打解包
- user_info里有两个成员,money version=2; gold version=3
- 我们依次从version=0到3对user_info进行序列反序列化，观察这两个成员变化
- 代码如下所示(源文件见压缩包)
  ```
  int main(int argc , char **argv)
  {
  
	 sdr_data_res_t *pres;
	 char buff[MAX_BUFF_LEN] = {0};
	   user_info_t src_user;
    user_info_t dst_user;
    int len = 0;
    int version = -1;
    //init
	   memset(&src_user , 0 , sizeof(src_user));
	   memset(&dst_user , 0 , sizeof(dst_user));

    src_user.age = 32;
    src_user.gold = 5000;
    src_user.money = 1289;
    src_user.sex = 1;
    src_user.name_len = strlen("cs_f**k_suomei");
    strncpy(src_user.user_name , "cs_f**k_suomei" , sizeof(src_user.user_name));

    src_user.skill.skill_count = 2;
    src_user.skill.info_list[0].type = Q_SKILL;
    src_user.skill.info_list[0].data.qskill = 111;

    src_user.skill.info_list[1].type = E_SKILL;
    strncpy(src_user.skill.info_list[1].data.eskill , "wear" , sizeof(src_user.skill.info_list[1].data.eskill));

    printf("1) orignal==========================\n\n");
    print_user_info(&src_user);
    print_user_info(&dst_user);

    //sdr
    pres = sdr_load_bin(SDR_PROTO_FILE , NULL);
    if(!pres)
    {
    	printf("load %s failed!\n" , SDR_PROTO_FILE);
    	return -1;
    }

    while(version<3)
    {
    	version++;
    	printf("\n\nversion [%d] ==========================\n" , version);

    	//pack
    	len = sdr_pack(pres , buff , (char *)&src_user , "user_info" , version , NULL);
    	if(len < 0)
    	{
    		printf("sdr_pack failed!\n");
    		continue;
    	}
    	printf("sdr_pack len:%d\n" , len);

    	//unpack
    	len = sdr_unpack(pres , (char *)&dst_user , buff , "user_info" , NULL);
    	if(len < 0)
    	{
    		printf("sdr_unpack failed!\n");
    		return -1;
    	}
    	printf("sdr_unpack len:%d\n" , len);
    	print_user_info(&dst_user);
    }

    //free
    sdr_free_bin(pres);
    return 0;
  }
  ```
  - 代码定义两个user_info结构，src_user和dst_user. src_user用于序列化，dst_user用来接收每次反序列化结果
  - print_user_info函数用来打印uesr_info的成员变量
  - 执行结果如下：
    1. 首先初始化src_user和dst_user并打印(虚线上面是src_user 下面是dst_user)：
    ```
    sex:1 name:cs_f**k_suomei age:32 money:1289 gold:5000
    skill<0> type:1
    qskill:111
    skill<1> type:3
    eskill:wear
    -------------------------------
    sex:0 name: age:0 money:0 gold:0
    ```
    2. 用version=0对src_user序列化
    ```
    version [0] ==========================
    Error:sdr pack Failed! type 'user_info' version=1 is larger than curr version.0
    sdr_pack failed!
    ```
      序列化失败，原因在于user_info的version=1，高于输入版本号
    
    3. 用version=1对src_user序列化并反序列化到dst_user
    ```
    version [1] ==========================
    pack 'user_info' success! 469 -> 34
    sdr_pack len:42
    ready to unpack 'user_info', version:1,length:34
    unpack 'user_info' success! 34->469
    sdr_unpack len:469
    -------------------------------
    sex:1 name:cs_f**k_suomei age:32 money:0 gold:0
    skill<0> type:1
    qskill:111
    skill<1> type:3
    eskill:wear
    ```
    可以看到操作成功，但是由于money(versio=2),gold(version=3)高于序列化版本version=1,所以不会被序列化到二进制数据里.虚线下打印的是成功反序列     化的dst_user数据,money=0,gold=0是默认值
      
    4. 用version=2来再次相同操作:
    ```
    version [2] ==========================
    pack 'user_info' success! 469 -> 42
    sdr_pack len:50
    ready to unpack 'user_info', version:2,length:42
    unpack 'user_info' success! 42->469
    sdr_unpack len:469
    -------------------------------
    sex:1 name:cs_f**k_suomei age:32 money:1289 gold:0
    skill<0> type:1
    qskill:111
    skill<1> type:3
    eskill:wear
    ```
    可以看出相比version=1，这次将money(version=2)也成功序列化，并且序列化后的字节数相比3.多个了8个字节，这个就是money成员。但gold仍未操作，因为其version=3.
    
    5. 用version=3继续：
    ```
    version [3] ==========================
    pack 'user_info' success! 469 -> 50
    sdr_pack len:58
    ready to unpack 'user_info', version:3,length:50
    unpack 'user_info' success! 50->469
    sdr_unpack len:469
    -------------------------------
    sex:1 name:cs_f**k_suomei age:32 money:1289 gold:5000
    skill<0> type:1
    qskill:111
    skill<1> type:3
    eskill:wear
    ```
    可以看到gold字段得到了处理,序列化后的数据又增加了8字节
    
## 性能
### 说明
protobuffer是google的数据序列&反序列化的开源解决方案 https://github.com/protocolbuffers/protobuf 支持多种语言(不支持C).这里使用C++  
protobuf-c是googole protobuffer的C实现 https://github.com/protobuf-c/protobuf-c  

- **测试平台**: linux-3.10.104 x86_64 Intel(R) Xeon(R) CPU E5-2620 v3 @ 2.40GHz 24核15G
- **软件版本**: protobuf-all-3.6.1.tar.gz protobuf-c-1.3.1.tar.gz sdr-1.0.tar.gz
- **测试结构**: 分别制定bag.proto与bag.xml协议文件，双方结构基本保持一致.背包结构里存放item_info结构，上限1024，实存128
- **测试流程**: 分别使用protobuf,protobuf-c与sdr编写的程序对bag结构进行序列&反序列化各100万次，检查耗时和内存消耗

### 序列化

  | 库           |   耗时(秒)   |   CPU   |   内存       |     数据       | 成功率 |
  |:------------:| :---------: | :------:|:------------:|:-------------:|:-------:|  
  |protobuf-c    |    6.37     |  99%    |  max:936K    |  6280->1813   |   100%  |  
  |sdr           |    12.46    |  98%    |  max:2292K   |  20544->2624  |   100%  |
  |protobuf(c++) |    41.25    |  98%    |  max:2920K   |  xx->1812     |   100%  |
  
### 反序列化

  | 库           |   耗时(秒)   |   CPU   |   内存       |     数据       | 成功率 |
  |:------------:| :---------: | :------:|:------------:|:-------------:|:-------:|
  |protobuf-c    |    24.55     |  98%    |  max:928K   |  1813->6280   |   100%  |
  |sdr           |    12.87     |  99%    |  max:2272K  |  2624->20544  |   100%  |
  |protobuf(c++) |    41.39    |  98%    |  max:3964K   |  1812->xx     |   100%  |

### 总结
- protobuf-c在序列化数据上性能更高，基本达到sdr一倍
- protobuf-c在反序列化数据时性能不如sdr，基本只为sdr的1/2
- protobuf-c适用于序列化数据频次非常高的场合，适用于写频率较高的场景，比如频繁的保存数据
- sdr 在序列&反序列化上性能较为均衡基本一致，适用于读频率较高的场景，比如频繁的登录查询
- 在内存占用上，由于protobuf-c是动态分配数据，所以占用内存较少；sdr是静态分配内存，所以内存占用率由结构体大小提前决定
- 在数据压缩上，也是由于动态&静态分配内存不同，sdr的压缩比和实际使用数目相关，实际使用数目越低则压缩率越高
- 在使用便捷上，个人认为probuf-c必须对每一个结构的子结构进行动态分配和释放，可用性较低，尤其对于较复杂结构使用的复杂性非常高，基本不能维护. sdr则由于静态分配内存省却了必须跟踪每一个子结构体的分配释放烦恼，提高了开发效率
- protobuf 使用c++编写测试案例，由于语言不同，只做参考
- 所有测试文件均在performance目录下，使用前请装好相关环境

## More
更多内容:https://github.com/nmsoccer/sdr/wiki


