#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

extern int csops(pid_t pid, unsigned int ops, void *useraddr, size_t usersize);

#define CS_OPS_STATUS 0

struct csflag {
    unsigned int bit;
    const char *name;
    const char *note;
};

static const struct csflag kFlags[] = {
    { 0x00000001, "CS_VALID",                  "signature currently valid" },
    { 0x00000002, "CS_ADHOC",                  "ad hoc signed" },
    { 0x00000004, "CS_GET_TASK_ALLOW",         "task_for_pid is permitted" },
    { 0x00000008, "CS_INSTALLER",              "installer entitlement" },
    { 0x00000010, "CS_FORCED_LV",              "library validation forced on" },
    { 0x00000020, "CS_INVALID_ALLOWED",        "may run with invalid pages" },
    { 0x00000100, "CS_HARD",                   "do not load invalid pages" },
    { 0x00000200, "CS_KILL",                   "kill if it becomes invalid" },
    { 0x00000400, "CS_CHECK_EXPIRATION",       "check certificate expiration" },
    { 0x00000800, "CS_RESTRICT",               "dyld treats process as restricted" },
    { 0x00001000, "CS_ENFORCEMENT",            "code signing enforced" },
    { 0x00002000, "CS_REQUIRE_LV",             "library validation required" },
    { 0x00004000, "CS_ENTITLEMENTS_VALIDATED", "entitlements checked against signature" },
    { 0x00008000, "CS_NVRAM_UNRESTRICTED",     "may set nvram unrestricted" },
    { 0x00010000, "CS_RUNTIME",                "hardened runtime policies" },
    { 0x00020000, "CS_LINKER_SIGNED",          "signed automatically by the linker" },
    { 0x00100000, "CS_EXEC_SET_HARD",          "set CS_HARD on exec" },
    { 0x00200000, "CS_EXEC_SET_KILL",          "set CS_KILL on exec" },
    { 0x00400000, "CS_EXEC_SET_ENFORCEMENT",   "set CS_ENFORCEMENT on exec" },
    { 0x00800000, "CS_EXEC_INHERIT_SIP",       "inherit SIP status on exec" },
    { 0x01000000, "CS_KILLED",                 "killed for invalidity" },
    { 0x02000000, "CS_NO_UNTRUSTED_HELPERS",   "no non-platform dyld or Rosetta" },
    { 0x04000000, "CS_PLATFORM_BINARY",        "Apple platform binary" },
    { 0x08000000, "CS_PLATFORM_PATH",          "on a platform path" },
    { 0x10000000, "CS_DEBUGGED",               "is or has been debugged" },
    { 0x20000000, "CS_SIGNED",                 "has a signature" },
    { 0x40000000, "CS_DEV_CODE",               "developer code" },
    { 0x80000000, "CS_DATAVAULT_CONTROLLER",   "datavault controller entitlement" },
};

int main(int argc, char **argv)
{
    unsigned int status = 0;
    unsigned int seen = 0;
    pid_t pid;
    size_t i;

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

    for (i = 0; i < sizeof(kFlags) / sizeof(kFlags[0]); i++) {
        if (status & kFlags[i].bit) {
            printf("  0x%08x  %-26s %s\n",
                   kFlags[i].bit, kFlags[i].name, kFlags[i].note);
            seen |= kFlags[i].bit;
        }
    }

    if ((status & ~seen) != 0) {
        printf("  0x%08x  %-26s %s\n", status & ~seen, "(unknown)", "not in cs_blobs.h");
    }

    return 0;
}
