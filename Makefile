.PHONY: build run_a run_u


all: build run_a run_u

build: 1/archiver.c 1/unarchiver.c 1/functions.c
	gcc 1/archiver.c 1/functions.c -o archiver -g
	gcc 1/unarchiver.c 1/functions.c -o unarchiver -g

run_a: archiver
	./archiver 4 > archiver_output.txt

run_u: unarchiver
	./unarchiver 4_archived > unarchiver_output.txt