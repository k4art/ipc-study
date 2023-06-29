#!/bin/bash

printf "\t%s\t%s\t\t%s\n" "msg size" "mmap" "socket"

for i in {1,8,16,64,128,256,512,1024,2048,4096,16384};
do
  printf "\t%05s\t\t" $i;
  ./build/bench mmap $i | ./walltime.sh;
  echo -en "\t";
  ./build/bench socket $i | ./walltime.sh;
  echo "";
done
