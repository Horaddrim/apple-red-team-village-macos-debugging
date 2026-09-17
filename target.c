#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/proc.h>

#include "proc_flags.h"

static const unsigned char kEncodedSecret[] = {
    0x00, 0x17, 0x0c, 0x1c, 0x0b, 0x1a, 0x0d, 0x04,
    0x1e, 0x16, 0x10, 0x00, 0x1b, 0x03, 0x12, 0x16,
    0x0c, 0x19, 0x0a, 0x15, 0x1b, 0x16, 0x15, 0x17,
    0x16, 0x01, 0x11, 0x0d, 0x05, 0x09, 0x04, 0x05,
    0x07, 0x11, 0x1c, 0x12, 0x0e, 0x0a, 0x1c, 0x0a,
};

static const char kAzeroth[] = "azeroth";

int detect_debugger(void)
{
    struct kinfo_proc info;
    size_t size = sizeof(info);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    unsigned int status = 0;
    int traced;

    memset(&info, 0, sizeof(info));
    if (sysctl(mib, 4, &info, &size, NULL, 0) != 0) {
        return 0;
    }

    traced = (info.kp_proc.p_flag & P_TRACED) != 0;

    printf("p_flag=0x%08x  P_TRACED=%d  p_debugger=%d\n",
           info.kp_proc.p_flag, traced, info.kp_proc.p_debugger);
    print_flag_bits(kProcFlags, FLAG_COUNT(kProcFlags),
                    (unsigned int)info.kp_proc.p_flag, "not in sys/proc.h");

    /* Same process, the other flag word: what AMFI wrote at exec(). */
    if (csops(getpid(), CS_OPS_STATUS, &status, sizeof(status)) == 0) {
        printf("csflags=0x%08x\n", status);
        print_flag_bits(kCSFlags, FLAG_COUNT(kCSFlags), status,
                        "not in cs_blobs.h");
    }

    return traced;
}

int check_passphrase(const char *candidate)
{
    char buf[sizeof(kEncodedSecret) + 1];
    size_t klen = sizeof(kAzeroth) - 1;
    size_t i;
    int ok;

    if (candidate == NULL || strlen(candidate) != sizeof(kEncodedSecret)) {
        return 0;
    }

    for (i = 0; i < sizeof(kEncodedSecret); i++) {
        buf[i] = (char)(kEncodedSecret[i] ^ kAzeroth[i % klen]);
    }
    buf[sizeof(kEncodedSecret)] = '\0';

    ok = (strcmp(buf, candidate) == 0);
    memset(buf, 0, sizeof(buf));

    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s <passphrase> [hold]\n", argv[0]);
        return 2;
    }

    if (detect_debugger()) {
        fprintf(stderr, "debugger detected\n");
        return 3;
    }

    if (argc == 3 && strcmp(argv[2], "hold") == 0) {
        printf("pid=%d kEncodedSecret=%p\n", getpid(), (const void *)kEncodedSecret);
        fflush(stdout);
        for (;;) {
            printf("check_passphrase=%d\n", check_passphrase(argv[1]));
            fflush(stdout);
            sleep(2);
        }
    }

    if (check_passphrase(argv[1])) {
        printf("access granted\n");
        return 0;
    }

    printf("access denied\n");
    return 1;
}
