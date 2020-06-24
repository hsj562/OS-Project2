#!/bin/bash/
echo "fcntl diff begin";
for i in $(seq 1 3)
do
	sudo diff ../input/my_input_1/my_input_${i}.txt ../output/my_output_1/fcntl_result_${i}.txt 
done

echo "fcntl diff end, mmap diff begin"

for j in $(seq 1 3)
do
	sudo diff ../input/my_input_1/my_input_${j}.txt ../output/my_output_1/mmap_result_${j}.txt 
done

echo "mmap diff end"

