
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv)
{
    int fd;
    int status;
    double distance;
    if(argc != 3)
    {
		printf("Usage: %s <dev> <on | off>\n", argv[0]);
		return -1;
    }

    fd = open(argv[1],O_RDWR);
    if(fd == -1)
    {
		printf("can not open file %s\n", argv[1]);
		return -1;
    }

   
    while(1)
    {
        read(fd,&status,4);
        printf("status is = %d\n",status);
        distance = 340*status;
        distance/=2000000000;
        printf("distance is = %lf\n",distance);
        sleep(1);
    }
    close(fd);
    return 0;
}












