#!/bin/sh

make
ulimit -c 500000
for t in `ls *.t`
do
    echo ''
    echo ------------ run $t --------------
    ./$t
    echo ----------------------------------
done
