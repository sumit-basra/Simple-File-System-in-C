#!/bin/bash

make clean;
clear;
make;

echo -e "\n\n";
echo "Creating Virtual Disk of size 8192";
rm *driver;
./fs.x make our_driver 8192;
./fs.x make ref_driver 8192;


echo -e "\n\n";
echo "Testing info";
./test-fs.x info our_driver > our_info.txt;
./fs.x info ref_driver > ref_info.txt;
diff our_info.txt ref_info.txt;
rm our_info.txt ref_info.txt;


echo -e "\n\n";
echo "Testing empty ls";
./test-fs.x ls our_driver > our_ls.txt;
./fs.x ls ref_driver > ref_ls.txt;
diff our_ls.txt ref_ls.txt;
rm our_ls.txt ref_ls.txt;

