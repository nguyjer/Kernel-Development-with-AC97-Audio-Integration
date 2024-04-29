#include "libc.h"

void one(int fd)
{
    printf("*** fd = %d\n", fd);
    printf("*** len = %d\n", len(fd));
    cp(fd, 2);
}

int main(int argc, char **argv)
{

    
    int fd = open("/data/stereo.wav", 0);
    play_audio(fd);
    close(fd);
    fd = open("/data/d4vdstereo.wav", 0);
    play_audio(fd);
    close(fd);

    printf("Exited sys call.\n");
    
    shutdown();
    return 0;
}


