#!/bin/bash/
echo "fcntl diff begin";
for i in $(seq 1 10)
do
	sudo diff ../input/sample_input_1/target_file_${i} ../output/sample_output_1/fcntl_result_${i}.txt 
done

echo "fcntl diff end, mmap diff begin"

for j in $(seq 1 10)
do
	sudo diff ../input/sample_input_1/target_file_${j} ../output/sample_output_1/mmap_result_${j}.txt 
done

echo "mmap diff end"

