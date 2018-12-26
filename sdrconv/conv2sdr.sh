#!/bin/bash
#this script only roughly calcs total node
#and uses sdrconv to convert xml file
#added by soul 2018-12-26

SDRCONV="/usr/local/bin/sdrconv"
XMLFILE=$1


function show_help()
{
  echo "usage:$0 <xml_file>"
  exit 0
}

#check INPUTFILE
if [[ -z $1 ]]
then
  echo "Please input xml file!"
  show_help
  exit 1
fi

if [[ ! -e ${XMLFILE} ]]
then
  echo "${XMLFILE} not exist!"
  exit 1
fi

#check sdrconv
if [[ ! -e ${SDRCONV} ]]
then
  echo "${SDRCONV} not found! Please check and install sdrconv!"
  exit 1
fi

#calc target node
macro_num=`cat ${XMLFILE} | grep -i '^[[:space:]]*<macro' | wc -l`
struct_num=`cat ${XMLFILE} | grep -i '^[[:space:]]*<struct' | wc -l`
union_num=`cat ${XMLFILE} | grep -i '^[[:space:]]*<union' | wc -l`
entry_num=`cat ${XMLFILE} | grep -i '^[[:space:]]*<entry' | wc -l`

let total_num=macro_num+struct_num+union_num+entry_num+1
#printf "macro:%d struct:%d union:%d entry:%d total:%d\n" ${macro_num} \
#  ${struct_num} ${union_num} ${entry_num} ${total_num}

#convert
${SDRCONV} -s ${total_num} -I ${XMLFILE} 

