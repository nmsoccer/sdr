# SDR
A Simple Data Representation  
一种简单的数据表示及序列反序列化接口.本文将简单介绍sdr的特性与使用方法，同时给出使用范例，并在文末提供与protbuf-c粗略的性能比较

- **数据描述** 通过XML文件描述数据结构
- **有限规则** 支持少数几种严格限制的标签即可描述C语言所使用的结构及数据类型
- **转换工具** 提供转换工具将xml文本转换为协议描述文件，同时支持将描述文件反转为xml结构化文本
- **静态分配** 与protobuf-c不同，被描述的嵌套结构体在外层结构体定义时内部结构体同时统一静态定义，不需要protobuf-c一样只定义指针而需要开发者针对每个内部结构体一一动态分配
- **打包解包** 将数据结构序列化为二进制数据，及对二进制流反序列化
- **版本兼容** 对每个标签赋予版本号来支持版本兼容

---

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
3. sdrconv -s 将执行工具使用的节点数目，默认是20K

### 编译(以demo为例)
1. 在demo目录下创建协议文件demo.xml
2. 在demo目录下执行sdrconv -I demo.xml 如果成功执行将会生成demo.h
3. gcc -g demo.c -lsdr -o demo 生成可执行文件
_如果找不到动态库，需要将/usr/local/lib加入/etc/ld.so.conf 然后执行/sbin/ldconfig_



## API
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

## 版本兼容
### 约束
- 如果xml里的结构未指定version 则该结构version为最低值0
- 已有字段不应重新设置version,已有version的字段不能修改version值
- 原有结构体内新增的字段需要添加version属性，且属性值需要高于该结构最近一次序列化所使用的版本号
- 序列化结构时所使用的协议号最好要>=该结构及成员version的最大值。序列化时version高于参数version的字段将不会序列化
- 反序列化二进制数据时，目标结构version>二进制携带versio的字段将不会得到解析,从而新字段得到保留

### 举例
- 假设当前对某结构体user_info进行version=2的序列化操作，那么user_info里version>2的成员将不会序列化
- 修改结构体user_info 新增成员entry(注意 成员只能增不能减)
- 现在对user_info新加成员version=3,然后将原有version=2的序列化数据进行反序列化，则version=3的成员使用默认值，其他<=2的成员会成功赋值
- 后面只需要按照version=3进行序列化即可

to be continue...
best whishes!
1222
