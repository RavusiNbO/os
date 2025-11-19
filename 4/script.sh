#!bin/bash


for i in $(seq 1 100)
do
	./sobel | tee -a "results.txt"
done
