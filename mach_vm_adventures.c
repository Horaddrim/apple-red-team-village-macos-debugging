#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/mach_traps.h>

static int acquire_task(int pid, task_t *out)
{
    kern_return_t kr = task_for_pid(mach_task_self(), pid, out);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_for_pid(%d): %s (0x%x)\n",
                pid, mach_error_string(kr), kr);
        return -1;
    }

    return 0;
}

int read_process_memory(int pid, uint64_t addr, int size, char *my_addr)
{
    task_t task;
    vm_offset_t data = 0;
    mach_msg_type_number_t got = 0;
    mach_msg_type_number_t n;
    kern_return_t kr;

    if (size <= 0 || my_addr == NULL) {
        return -1;
    }

    if (acquire_task(pid, &task) != 0) {
        return -1;
    }

    kr = mach_vm_read(task, (mach_vm_address_t)addr, (mach_vm_size_t)size,
                      &data, &got);
    mach_port_deallocate(mach_task_self(), task);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_vm_read: %s (0x%x)\n", mach_error_string(kr), kr);
        return -1;
    }

    n = (got > (mach_msg_type_number_t)size) ? (mach_msg_type_number_t)size : got;
    memcpy(my_addr, (const void *)data, n);
    mach_vm_deallocate(mach_task_self(), data, got);

    return (int)n;
}

int write_process_memory(int pid, uint64_t addr, int size, char *my_addr)
{
    task_t task;
    kern_return_t kr;
    mach_vm_address_t region = (mach_vm_address_t)addr;
    mach_vm_size_t region_size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    vm_prot_t original = VM_PROT_NONE;
    int restore = 0;

    if (size <= 0 || my_addr == NULL) {
        return -1;
    }

    if (acquire_task(pid, &task) != 0) {
        return -1;
    }

    kr = mach_vm_region(task, &region, &region_size, VM_REGION_BASIC_INFO_64,
                        (vm_region_info_t)&info, &info_count, &object);

    if (object != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), object);
    }

    if (kr == KERN_SUCCESS && (info.protection & VM_PROT_WRITE) == 0) {
        original = info.protection;
        kr = mach_vm_protect(task, (mach_vm_address_t)addr, (mach_vm_size_t)size,
                             FALSE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "mach_vm_protect: %s (0x%x)\n",
                    mach_error_string(kr), kr);
            mach_port_deallocate(mach_task_self(), task);
            return -1;
        }
        restore = 1;
    }

    kr = mach_vm_write(task, (mach_vm_address_t)addr, (vm_offset_t)my_addr,
                       (mach_msg_type_number_t)size);

    if (restore) {
        mach_vm_protect(task, (mach_vm_address_t)addr, (mach_vm_size_t)size,
                        FALSE, original);
    }

    mach_port_deallocate(mach_task_self(), task);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_vm_write: %s (0x%x)\n", mach_error_string(kr), kr);
        return -1;
    }

    return size;
}

static int parse_hex_bytes(const char *s, unsigned char **out)
{
    size_t len = strlen(s);
    size_t n;
    size_t i;
    unsigned char *buf;

    if (len == 0 || (len % 2) != 0) {
        return -1;
    }

    n = len / 2;
    buf = malloc(n);
    if (buf == NULL) {
        return -1;
    }

    for (i = 0; i < n; i++) {
        unsigned int byte;
        if (sscanf(s + (i * 2), "%2x", &byte) != 1) {
            free(buf);
            return -1;
        }
        buf[i] = (unsigned char)byte;
    }

    *out = buf;
    return (int)n;
}

static void hexdump(const unsigned char *buf, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");

    for (i = 0; i < n; i++) {
        unsigned char c = buf[i];
        printf("%c", (c >= 0x20 && c < 0x7f) ? c : '.');
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    if (argc == 5 && strcmp(argv[1], "read") == 0) {
        int pid = atoi(argv[2]);
        uint64_t addr = strtoull(argv[3], NULL, 0);
        int size = atoi(argv[4]);
        char *buf;
        int got;

        if (size <= 0) {
            fprintf(stderr, "size must be positive\n");
            return 2;
        }

        buf = calloc(1, (size_t)size);
        if (buf == NULL) {
            return 1;
        }

        got = read_process_memory(pid, addr, size, buf);
        if (got < 0) {
            free(buf);
            return 1;
        }

        printf("read %d bytes from pid %d @ 0x%llx\n",
               got, pid, (unsigned long long)addr);
        hexdump((const unsigned char *)buf, got);
        free(buf);
        return 0;
    }

    if (argc == 5 && strcmp(argv[1], "write") == 0) {
        int pid = atoi(argv[2]);
        uint64_t addr = strtoull(argv[3], NULL, 0);
        unsigned char *buf = NULL;
        int n;
        int wrote;

        n = parse_hex_bytes(argv[4], &buf);
        if (n < 0) {
            fprintf(stderr, "payload must be an even-length hex string\n");
            return 2;
        }

        wrote = write_process_memory(pid, addr, n, (char *)buf);
        free(buf);

        if (wrote < 0) {
            return 1;
        }

        printf("wrote %d bytes to pid %d @ 0x%llx\n",
               wrote, pid, (unsigned long long)addr);
        return 0;
    }

    fprintf(stderr,
            "usage:\n"
            "  %s read  <pid> <addr> <size>\n"
            "  %s write <pid> <addr> <hexbytes>\n",
            argv[0], argv[0]);
    return 2;
}
