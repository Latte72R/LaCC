#!/bin/bash

echo \[unitests.c\]
./rf.sh $1 ./unitests.c

echo \[prime.c\]
./rf.sh $1 ./prime.c

echo \[fizzbuzz.c\]
./rf.sh $1 ./fizzbuzz.c
