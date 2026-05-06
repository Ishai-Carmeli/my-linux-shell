all: build/myshell build/looper build/Printers build/mypipe

build/myshell: src/myshell.c src/LineParser.c
	gcc -g -Wall -m32 -o build/myshell src/myshell.c src/LineParser.c

build/looper: src/looper.c
	gcc -g -Wall -m32 -o build/looper src/looper.c

build/Printers: src/Printers.c
	gcc -g -Wall -m32 -o build/Printers src/Printers.c

build/mypipe: src/mypipe.c
	gcc -g -Wall -m32 -o build/mypipe src/mypipe.c

.PHONY: clean

clean: 
	rm -f build/*