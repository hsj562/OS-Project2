#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#define num_page 50
#define PAGE_SIZE 4096
#define BUF_SIZE 512
int main (int argc, char* argv[])
{
	char buf[BUF_SIZE];
	int file_num = atoi(argv[1]);
	int i, dev_fd, file_fd[100];// the fd for the device and the fd for the input file
	size_t ret, file_size[100], data_size = -1;
	char file_name[100][50];
	char method[20];
	char ip[20];
	struct timeval start;
	struct timeval end;
	double trans_time; //calulate the time between the device is opened and it is closed
	char *kernel_address, *file_address;
	strcpy(method, argv[2 + file_num]);
	strcpy(ip, argv[3 + file_num]);
	if( (dev_fd = open("/dev/slave_device", O_RDWR)) < 0)//should be O_RDWR for PROT_WRITE when mmap()
	{
		perror("failed to open /dev/slave_device\n");
		return 1;
	}
	gettimeofday(&start ,NULL);
	for(int j = 0; j < file_num; j++){
		strcpy(file_name[j], argv[2 + j]);
		if( (file_fd[j] = open (file_name[j], O_RDWR | O_CREAT | O_TRUNC)) < 0)
		{
			perror("failed to open input file\n");
			return 1;
		}
	}
	for(int j = 0; j < file_num; j++){
	if(ioctl(dev_fd, 0x12345677, ip) == -1)	//0x12345677 : connect to master in the device
	{
		perror("ioclt create slave socket error\n");
		return 1;
	}
	size_t offset = 0;
	switch(method[0])
	{
		case 'f'://fcntl : read()/write()
			do
			{
				ret = read(dev_fd, buf, sizeof(buf)); // read from the the device
				write(file_fd[j], buf, ret); //write to the input file
				file_size[j] += ret;
			}while(ret > 0);
			break;
		case 'm':
			while ((ret = ioctl(dev_fd, 0x12345678)) != 0) {
				posix_fallocate(file_fd[j], offset, ret);
				file_address = mmap(NULL, ret, PROT_WRITE, MAP_SHARED, file_fd[j], offset);
				kernel_address = mmap(NULL, ret, PROT_READ, MAP_SHARED, dev_fd, offset);
				memcpy(file_address, kernel_address, ret);
				munmap(file_address, ret);
				offset += ret;
			}	
			file_size[j] = offset;
			break;
	}


	if(ioctl(dev_fd, 0x12345679) == -1)// end receiving data, close the connection
	{
		perror("ioclt client exits error\n");
		return 1;
	}
	gettimeofday(&end, NULL);
	trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.0001;
	printf("Transmission time: %lf ms, File size: %d bytes\n", trans_time, file_size[j] / 8);
	if(kernel_address){
		ioctl(dev_fd, 0x12345680);
		munmap(kernel_address, num_page * PAGE_SIZE);
	}
	}
	for(int j = 0; j < file_num; j++)
		close(file_fd[j]);
	close(dev_fd);
	return 0;
}
