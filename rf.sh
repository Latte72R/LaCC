./9cc $1 > ./tmp.s
cc -o ./tmp ./tmp.s ./extensions.c
./tmp
