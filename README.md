## Before start ...

sudo dd if=/dev/zero of=file bs=4096 count=10000

## How to run the program

sudo ./cs238 --truncate file

## Check memory leak

sudo valgrind --leak-check=full ./cs238 file
