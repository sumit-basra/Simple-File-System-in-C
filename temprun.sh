#!/bin/bash

make clean;
clear;
make;

echo -e "\n\n"
echo -e "\n\n"
./fs.x info ecs150_fs
echo -e "\n\n"
echo -e "\n\n"
./test-fs.x info ecs150_fs


