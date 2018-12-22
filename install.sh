#!/bin/bash
WORK_DIR=`pwd`
HEADER_DIR="/usr/local/include/sdr/"
LIB_DIR="/usr/local/lib/"
CONVER_DIR="${WORK_DIR}/sdrconv/"
HEADER_FILES="sdr.h sdr_types.h"
TARGET_SO="libsdr.so"
LINK_NAME="libsdr.so.1"

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

echo "install complete!"
cd ${WORK_DIR}
exit 0


