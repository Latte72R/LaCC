
echo "$1" > ./tmp.c
./9cc ./tmp.c > ./tmp.s
cc -o ./tmp ./tmp.s ./extensions.c
./tmp
echo $?
