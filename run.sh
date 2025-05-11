
echo "$1" > ./tmp.c
./lcc ./tmp.c > ./tmp.s
cc -o ./tmp ./tmp.s ./extensions.c
./tmp
echo $?
