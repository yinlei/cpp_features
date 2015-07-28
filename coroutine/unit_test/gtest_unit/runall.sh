#!/bin/sh

make
for t in `ls *.t`
do
    echo ''
    echo ------------ run $t --------------
    ./$t
    echo ----------------------------------
done
