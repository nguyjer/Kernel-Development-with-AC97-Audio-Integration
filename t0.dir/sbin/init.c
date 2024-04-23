#include "libc.h"

void one(int fd)
{
    printf("*** fd = %d\n", fd);
    printf("*** len = %d\n", len(fd));
    cp(fd, 2);
}

int main(int argc, char **argv)
{

    
    int fd = open("data/audiofile.wav", 0);
    play_audio(fd);

    
    shutdown();
    return 0;
}
