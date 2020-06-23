#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define num_page 50
#define PAGE_SIZE 4096
#define BUF_SIZE 512
size_t get_filesize(const char* filename);//get the size of the input file


int main (int argc, char* argv[])
{
	int file_cnt = atoi(argv[1]);

	char buf[BUF_SIZE];
	int i, dev_fd, file_fd[100];// the fd for the device and the fd for the input file
	size_t ret, file_size[100], offset = 0, tmp;
	char file_name[100][50], method[20];
	
	strcpy(method, argv[2 + file_cnt]);
	
	char *kernel_address = NULL, *file_address = NULL;
	struct timeval start;
	struct timeval end;
	double trans_time; //calulate the time between the device is opened and it is closed
	if( (dev_fd = open("/dev/master_device", O_RDWR)) < 0)
	{
		perror("failed to open /dev/master_device\n");
		return 1;
	}
	gettimeofday(&start ,NULL);
	for(int j = 0; j < file_cnt; j++){
		strcpy(file_name[j], argv[2 + j]);	// if file_cnt = 1
		if( (file_fd[j] = open (file_name[j], O_RDWR)) < 0 )
		{
			perror("failed to open input file\n");
			return 1;
		}
		if( (file_size[j] = get_filesize(file_name[j])) < 0)
		{
		perror("failed to get filesize\n");
		return 1;
		}
	}
	for(int j = 0; j < file_cnt; j++){
	if(ioctl(dev_fd, 0x12345677) == -1) //0x12345677 : create socket and accept the connection from the slave
	{
		perror("ioclt server create socket error\n");
		return 1;
	}

	size_t map_sz = PAGE_SIZE * 5;
	switch(method[0])
	{
		case 'f': //fcntl : read()/write()
			do
			{
				ret = read(file_fd[j], buf, sizeof(buf)); // read from the input file
				//printf("write %d bytes\n", ret);
				write(dev_fd, buf, ret);//write to the the device
			}while(ret > 0);
			break;
		case 'm': // mmap
			while(file_size - offset > map_sz) {
				file_address = mmap(NULL, map_sz, PROT_READ, MAP_SHARED, file_fd[j], offset);
				kernel_address = mmap(NULL, map_sz, PROT_WRITE, MAP_SHARED, dev_fd, offset);
				offset += map_sz;
				memcpy(kernel_address, file_address, map_sz);
				ioctl(dev_fd, 0x12345678, map_sz);
			}
			if(file_size - offset > 0) {
				file_address = mmap(NULL, file_size-offset, PROT_READ, MAP_SHARED, file_fd[j], offset);
				kernel_address = mmap(NULL, file_size-offset, PROT_WRITE, MAP_SHARED, dev_fd, offset);
				memcpy(kernel_address, file_address, file_size-offset);
				ioctl(dev_fd, 0x12345678, file_size-offset);
				offset = file_size;
			}
	}
	if((ret = ioctl(dev_fd, 0x12345679)) == -1) // end sending data, close the connection
	{
		perror("ioclt server exits error\n");
		return 1;
	}
	//printf("ret = %d\n", ret);
	gettimeofday(&end, NULL);
	trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.0001;
	printf("Transmission time: %lf ms, File size: %d bytes\n", trans_time, file_size[j] / 8);
	if(kernel_address){
		ioctl(dev_fd, 0x12345680);
		munmap(dev_fd, num_page * PAGE_SIZE);
	}
	}
	for(int j = 0; j < file_cnt; j++)
		close(file_fd[j]);
	close(dev_fd);
	return 0;
}

size_t get_filesize(const char* filename)
{
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}
