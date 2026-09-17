#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/mach_traps.h>
#include <mach/arm/thread_status.h>

/*
 * ARM64 has no int3. The software breakpoint instruction is BRK #imm16,
 * encoded as 0xd4200000 | (imm16 << 5), so brk #0 is 0xd4200000 - which is
 * exactly what lldb writes: read the 4 bytes under one of its breakpoints out
 * of a live process and this is what comes back.
 */
#define ARM64_BRK0 0xd4200000u

/*
 * MIG generates the server side of mach_exc.defs (see build.sh). It decodes
 * the exception message and calls catch_mach_exception_raise() below.
 */
extern boolean_t mach_exc_server(mach_msg_header_t *request,
                                 mach_msg_header_t *reply);

/*
 * The MIG callback takes no user context, so the breakpoint being serviced has
 * to live at file scope.
 */
static struct {
    int pid;
    uint64_t addr;
    uint32_t saved_insn;
    int armed;
    int hits;
} g_bp;

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

/*
 * Installing a breakpoint is just a read and a write - no new Mach API beyond
 * what read/write_process_memory already do. Save the instruction that is
 * there, then overwrite it with brk #0.
 */
int add_breakpoint(int pid, uint64_t addr, uint32_t *saved_insn)
{
    uint32_t brk = ARM64_BRK0;

    if (saved_insn == NULL) {
        return -1;
    }

    if (read_process_memory(pid, addr, (int)sizeof(*saved_insn),
                            (char *)saved_insn) != (int)sizeof(*saved_insn)) {
        return -1;
    }

    if (write_process_memory(pid, addr, (int)sizeof(brk),
                             (char *)&brk) != (int)sizeof(brk)) {
        return -1;
    }

    return 0;
}

int remove_breakpoint(int pid, uint64_t addr, uint32_t saved_insn)
{
    if (write_process_memory(pid, addr, (int)sizeof(saved_insn),
                             (char *)&saved_insn) != (int)sizeof(saved_insn)) {
        return -1;
    }

    return 0;
}

/*
 * MIG calls this once the exception message has been decoded. Everything here
 * runs in OUR process; the target's faulting thread is stopped, waiting for
 * the reply we send after this returns.
 */
kern_return_t catch_mach_exception_raise(mach_port_t exception_port,
                                         mach_port_t thread,
                                         mach_port_t task,
                                         exception_type_t exception,
                                         mach_exception_data_t code,
                                         mach_msg_type_number_t code_count)
{
    arm_thread_state64_t state;
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    kern_return_t kr;

    (void)exception_port;
    (void)task;

    g_bp.hits++;

    printf("\n*** trapped ***\n");
    printf("  exception       %d%s\n", exception,
           exception == EXC_BREAKPOINT ? "  (EXC_BREAKPOINT)" : "");
    if (code_count > 0) {
        printf("  code[0]         0x%llx\n", (unsigned long long)code[0]);
    }
    if (code_count > 1) {
        printf("  code[1]         0x%llx  (the faulting address)\n",
               (unsigned long long)code[1]);
    }

    kr = thread_get_state(thread, ARM_THREAD_STATE64,
                          (thread_state_t)&state, &count);
    if (kr == KERN_SUCCESS) {
        printf("  pc              0x%llx\n",
               (unsigned long long)__darwin_arm_thread_state64_get_pc(state));
        printf("  x0              0x%llx\n",
               (unsigned long long)state.__x[0]);
        printf("  lr              0x%llx\n",
               (unsigned long long)__darwin_arm_thread_state64_get_lr(state));
    } else {
        printf("  thread_get_state: %s (0x%x)\n", mach_error_string(kr), kr);
    }

    /*
     * One-shot: put the original instruction back so the thread re-executes it
     * when we reply. BRK is a synchronous exception and the pc is left ON the
     * brk, not after it, so nothing needs to be rewound.
     */
    if (g_bp.armed) {
        if (remove_breakpoint(g_bp.pid, g_bp.addr, g_bp.saved_insn) == 0) {
            printf("  restored        0x%08x\n", g_bp.saved_insn);
            g_bp.armed = 0;
        } else {
            printf("  restore FAILED - the target will trap again\n");
        }
    }

    return KERN_SUCCESS;
}

/* Required by the MIG server, unused: we ask for EXCEPTION_DEFAULT. */
kern_return_t catch_mach_exception_raise_state(mach_port_t exception_port,
        exception_type_t exception, const mach_exception_data_t code,
        mach_msg_type_number_t code_count, int *flavor,
        const thread_state_t old_state, mach_msg_type_number_t old_state_count,
        thread_state_t new_state, mach_msg_type_number_t *new_state_count)
{
    (void)exception_port; (void)exception; (void)code; (void)code_count;
    (void)flavor; (void)old_state; (void)old_state_count; (void)new_state;
    (void)new_state_count;
    return MIG_BAD_ID;
}

kern_return_t catch_mach_exception_raise_state_identity(
        mach_port_t exception_port, mach_port_t thread, mach_port_t task,
        exception_type_t exception, mach_exception_data_t code,
        mach_msg_type_number_t code_count, int *flavor,
        thread_state_t old_state, mach_msg_type_number_t old_state_count,
        thread_state_t new_state, mach_msg_type_number_t *new_state_count)
{
    (void)exception_port; (void)thread; (void)task; (void)exception;
    (void)code; (void)code_count; (void)flavor; (void)old_state;
    (void)old_state_count; (void)new_state; (void)new_state_count;
    return MIG_BAD_ID;
}

/*
 * The whole debugger loop, such as it is: own a port, tell the kernel to send
 * this task's breakpoint exceptions there, arm the breakpoint, block on
 * mach_msg, hand the message to MIG, reply.
 *
 * Note what is NOT here: ptrace. Exceptions on Darwin are Mach messages, and a
 * task port is enough to receive them.
 */
int run_breakpoint(int pid, uint64_t addr, int timeout_ms)
{
    task_t task;
    mach_port_t port = MACH_PORT_NULL;
    kern_return_t kr;
    union { mach_msg_header_t head; char pad[4096]; } request, reply;
    exception_mask_t old_masks[EXC_TYPES_COUNT];
    mach_port_t old_ports[EXC_TYPES_COUNT];
    exception_behavior_t old_behaviors[EXC_TYPES_COUNT];
    thread_state_flavor_t old_flavors[EXC_TYPES_COUNT];
    mach_msg_type_number_t old_count = EXC_TYPES_COUNT;
    int rc = -1;

    if (acquire_task(pid, &task) != 0) {
        return -1;
    }

    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_port_allocate: %s (0x%x)\n",
                mach_error_string(kr), kr);
        goto out;
    }

    kr = mach_port_insert_right(mach_task_self(), port, port,
                                MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_port_insert_right: %s (0x%x)\n",
                mach_error_string(kr), kr);
        goto out;
    }

    /* Whoever had EXC_BREAKPOINT before us gets it back on the way out. */
    kr = task_get_exception_ports(task, EXC_MASK_BREAKPOINT, old_masks,
                                  &old_count, old_ports, old_behaviors,
                                  old_flavors);
    if (kr != KERN_SUCCESS) {
        old_count = 0;
    }

    kr = task_set_exception_ports(task, EXC_MASK_BREAKPOINT, port,
                                  EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                  THREAD_STATE_NONE);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_set_exception_ports: %s (0x%x)\n",
                mach_error_string(kr), kr);
        goto out;
    }

    printf("  exception port  0x%x  (EXC_MASK_BREAKPOINT, no ptrace)\n", port);

    g_bp.pid = pid;
    g_bp.addr = addr;
    g_bp.hits = 0;

    if (add_breakpoint(pid, addr, &g_bp.saved_insn) != 0) {
        fprintf(stderr, "add_breakpoint failed\n");
        goto restore;
    }

    g_bp.armed = 1;
    printf("  saved           0x%08x\n", g_bp.saved_insn);
    printf("  wrote           0x%08x  (brk #0)\n", ARM64_BRK0);
    printf("  waiting for the target to run into it...\n");
    fflush(stdout);

    kr = mach_msg(&request.head, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
                  sizeof(request), port, (mach_msg_timeout_t)timeout_ms,
                  MACH_PORT_NULL);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_msg(receive): %s (0x%x)\n",
                mach_error_string(kr), kr);
        goto disarm;
    }

    if (!mach_exc_server(&request.head, &reply.head)) {
        fprintf(stderr, "mach_exc_server: message not handled (id %d)\n",
                request.head.msgh_id);
        goto disarm;
    }

    kr = mach_msg(&reply.head, MACH_SEND_MSG, reply.head.msgh_size, 0,
                  MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_msg(reply): %s (0x%x)\n",
                mach_error_string(kr), kr);
        goto disarm;
    }

    printf("  replied         KERN_SUCCESS - the target runs on\n");
    rc = 0;

disarm:
    if (g_bp.armed) {
        remove_breakpoint(pid, addr, g_bp.saved_insn);
        g_bp.armed = 0;
    }

restore:
    if (old_count > 0) {
        task_set_exception_ports(task, EXC_MASK_BREAKPOINT, old_ports[0],
                                 old_behaviors[0], old_flavors[0]);
    }

out:
    if (port != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), port);
        mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
    }
    mach_port_deallocate(mach_task_self(), task);
    return rc;
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

    if (argc == 4 && strcmp(argv[1], "break") == 0) {
        int pid = atoi(argv[2]);
        uint64_t addr = strtoull(argv[3], NULL, 0);

        printf("breakpoint\n");
        printf("  pid             %d\n", pid);
        printf("  addr            0x%llx\n", (unsigned long long)addr);
        fflush(stdout);

        if (run_breakpoint(pid, addr, 30000) != 0) {
            return 1;
        }

        printf("  hits            %d\n", g_bp.hits);
        return 0;
    }

    fprintf(stderr,
            "usage:\n"
            "  %s read  <pid> <addr> <size>\n"
            "  %s write <pid> <addr> <hexbytes>\n"
            "  %s break <pid> <addr>\n",
            argv[0], argv[0], argv[0]);
    return 2;
}
