#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#include "sandman.h"

int main(int argc, char **argv)
{
    if (argc < 3 || !*(argv[1]) || !*(argv[2])) {
        printf("Usage: %s [dev] [action]\n"
               "   action '0': suspend disk\n"
               "   action '1': resume disk\n", argv[0]);
        return 1;
    }

    int err = 0;
    char *disk = argv[1];
    char cmd = argv[2][0];

    int fd = open("/dev/sandman_dev1", 0);
    if (fd == -1) {
        perror("open()");
        goto fail;
    }

    if (cmd == '0')
        err = ioctl(fd, IOCTL_SANDMAN_SUSPEND, disk);
    else if (cmd == '1')
        err = ioctl(fd, IOCTL_SANDMAN_RESUME, disk);
    if (err)
        goto fail;

    close(fd);
    return 0;

fail:
    if (fd != -1)
        close(fd);
    perror("ioctl()");
    return errno;
}
