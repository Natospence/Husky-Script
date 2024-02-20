all: hscript.c 
	gcc -o hscript -Wall hscript.c
clean:
	rm hscript
submission:
	tar czvf prog4.tgz Makefile hscript.c README
