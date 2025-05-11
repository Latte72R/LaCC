#!/bin/bash

echo \[unitests.c\]
<<<<<<< HEAD
./rf.sh ./unitests.c

echo \[prime.c\]
./rf.sh ./prime.c

echo \[fizzbuzz.c\]
./rf.sh ./fizzbuzz.c
=======
./rf.sh $1 ./unitests.c

echo \[prime.c\]
./rf.sh $1 ./prime.c

echo \[fizzbuzz.c\]
./rf.sh $1 ./fizzbuzz.c
>>>>>>> cdb0c1a (Mission Accomplished)
