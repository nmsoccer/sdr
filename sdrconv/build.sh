#!/bin/bash
INSTALL_DIR="/usr/local/bin/"
BIN_FILE="sdrconv"
TOOL_SCR="conv2sdr.sh"

gcc -g -Werror -I.. sdrconvlib.c sdrconv.c ../sdr.c -o ${BIN_FILE} 
if [[ ! -e ${BIN_FILE} ]]
then
  echo "${BIN_FILE} not exist!"
  exit 1
fi

cp -f ${BIN_FILE} ${INSTALL_DIR}
if [[ $? -ne 0 ]]
then
  echo "install ${BIN_FILE} to ${INSTALL_DIR} failed!"
  exit 2
fi

rm ${BIN_FILE}

cd ../tools/
chmod a+x ${TOOL_SCR}
cp -f ${TOOL_SCR} ${INSTALL_DIR}

echo "install ${BIN_FILE} to ${INSTALL_DIR} success!"
exit 0
