#include <stdio.h>

#define DYLD_INTERPOSE(_replacement, _replacee) \
    __attribute__((used)) static struct { \
        const void *replacement; \
        const void *replacee; \
    } _interpose_##_replacee __attribute__((section("__DATA,__interpose"))) = { \
        (const void *)(unsigned long)&_replacement, \
        (const void *)(unsigned long)&_replacee \
    };

int gallywix(const char *candidate);

int goblin_gallywix(const char *candidate)
{
    (void)candidate;
    printf("hello\n");
    return 1;
}

DYLD_INTERPOSE(goblin_gallywix, gallywix)
