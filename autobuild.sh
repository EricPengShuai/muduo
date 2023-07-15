#!/bin/bash

# 在脚本执行过程中，如果任何一条命令返回非零的退出状态码（即命令执行失败），则立即终止脚本的执行，并返回相应的非零状态码
set -e

# 如果没有 build 目录就创建该目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build && 
    cmake .. && make -j4

# 回到顶层根目录
cd ..

# 把头文件拷贝到 /usr/include/mymuduo | so 库文件拷贝到 /usr/lib |PATH
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

for header in `ls *.h`
do
    cp $header /usr/include/mymuduo
done

cp `pwd`/lib/libmymuduo.so /usr/lib

ldconfig