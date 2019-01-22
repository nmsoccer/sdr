#!/bin/bash
WORK_DIR=`pwd`
HEADER_DIR="/usr/local/include/sdr/"
LIB_DIR="/usr/local/lib/"
CONVER_DIR="${WORK_DIR}/sdrconv/"
HEADER_FILES="sdr.h sdr_types.h"
TARGET_SO="libsdr.so"
LINK_NAME="libsdr.so.1"
TARGET_LIB="libsdr.a"
SRC_FILE="sdr.c"

#install sdrconv
cd ${CONVER_DIR}
./build.sh

if [[ $? -ne 0 ]]
then
  echo "build sdrconv failed!"
  exit 1
fi

cd ${WORK_DIR}
#compile libsdr.so
gcc -g -fPIC -shared sdr.c -o ${TARGET_SO}
if [[ ! -e ${TARGET_SO} ]]
then
  echo "compile ${TARGET_SO} failed!"
  exit 2
fi

#compile libsdr.a
gcc -g -c ${SRC_FILE}
ar crsv ${TARGET_LIB} *.o
if [[ ! -e ${TARGET_LIB} ]]
then
  echo "compile ${TARGET_LIB} failed!"
  exit 3
fi


#install
mkdir -p ${HEADER_DIR}
cp ${HEADER_FILES} ${HEADER_DIR}
if [[ $? -ne 0 ]]
then
  echo "install ${HEADER_FILES} to ${HEADER_DIR} failed!"
  exit 2
fi
echo "install ${HEADER_FILES} to ${HEADER_DIR} success!"

cp ${TARGET_SO} ${LIB_DIR}
rm ${TARGET_SO}
cd ${LIB_DIR}
rm ${LINK_NAME}
ln -s ${TARGET_SO} ${LINK_NAME}
if [[ $? -ne 0 ]]
then
  echo "install ${TARGET_SO} to ${LIB_DIR} failed!"
  exit 2
fi
echo "install ${TARGET_SO} to ${LIB_DIR} success!"

cd ${WORK_DIR}
cp -f ${TARGET_LIB} ${LIB_DIR}
rm ${TARGET_LIB}
rm *.o
echo "install ${TARGET_LIB} to ${LIB_DIR} success!"



echo "install complete!"
cd ${WORK_DIR}
exit 0


