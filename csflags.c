#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "flags.h"

int main(int argc, char **argv)
{
    unsigned int status = 0;
    pid_t pid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <pid>\n", argv[0]);
        return 2;
    }

    pid = (pid_t)atoi(argv[1]);

    if (csops(pid, CS_OPS_STATUS, &status, sizeof(status)) != 0) {
        perror("csops");
        return 1;
    }

    printf("pid %d  csflags = 0x%08x\n\n", pid, status);

    print_flag_bits(kCSFlags, FLAG_COUNT(kCSFlags), status, "not in cs_blobs.h");

    return 0;
}
