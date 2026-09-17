#include <stdio.h>

#define DYLD_INTERPOSE(_replacement, _replacee) \
    __attribute__((used)) static struct { \
        const void *replacement; \
        const void *replacee; \
    } _interpose_##_replacee __attribute__((section("__DATA,__interpose"))) = { \
        (const void *)(unsigned long)&_replacement, \
        (const void *)(unsigned long)&_replacee \
    };

int check_passphrase(const char *candidate);

int fake_check_passphrase(const char *candidate)
{
    (void)candidate;
    printf("hello\n");
    return 1;
}

DYLD_INTERPOSE(fake_check_passphrase, check_passphrase)
