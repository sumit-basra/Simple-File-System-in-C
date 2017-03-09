#!/bin/bash

make clean;
clear;
make;


echo -e "\n\n"
echo "Testing info"
./test-fs.x info driver > our_info.txt
./fs.x info driver > ref_info.txt
diff our_info.txt ref_info.txt


#	make
#	info
#	ls
#	add
#	rm
#	cat
#	stat


echo -e "\n\n"
echo "Testing ls"
./test-fs.x ls driver > our_ls.txt
./fs.x ls driver > ref_ls.txt
diff our_ls.txt ref_ls.txt


./test-fs.x add file1.txt driver 


echo -e "\n\n"
echo "Cleaning Files"
#rm *.txt



