./lcc $1 > ./tmp.s
cc -o ./tmp ./tmp.s
./tmp
