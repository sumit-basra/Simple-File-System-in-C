#!/bin/bash

clear;
make clean;
make;

echo "Testing info"
./test-fs.x info driver


