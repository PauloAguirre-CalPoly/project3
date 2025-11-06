all: 	memSim


clean:	
	rm -rf memSim

memSim: memSim.c 
	gcc -Wall -g -o $@ $?

submission: BACKING_STORE.bin memSim.c Makefile README
	tar -cf project3_submission.tar BACKING_STORE.bin memSim.c Makefile README 
	gzip project3_submission.tar
