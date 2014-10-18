
CFLAGS=-Wall -g
#CC=clang
boa:


t:
	for X in test/*.boa; do echo $$X; ./boa $$X -o /dev/null || exit 1; done
