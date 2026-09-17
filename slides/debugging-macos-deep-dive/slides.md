---
theme: default
title: Debugging macOS
info: |
  ## Debugging macOS — a deep dive
  Mach task ports, entitlements, and what the debugger is really doing.
colorSchema: dark
fonts:
  sans: 'Monaco'
  serif: 'Monaco'
  mono: 'Monaco'
  provider: none
themeConfig:
  primary: '#00ff41'
lineNumbers: false
transition: fade
duration: 45min
drawings:
  persist: false
layout: center
class: text-center
---

# Debugging macOS

<div class="mt-14 text-sm tracking-wide op-70">
  Igor Franca &nbsp;·&nbsp; Apple Red Team Village - MTS 12026 HE<span class="blink">_</span>
</div>

<style>
.blink {
  animation: blink 1.1s steps(1) infinite;
}
@keyframes blink {
  50% { opacity: 0; }
}
</style>

---
layout: two-cols
layoutClass: gap-12
---

# whoami

<v-clicks>

- AppSec Specialist & Tech Lead @ `insert_big_bank_here`
- Focado em SigInt e segurança mobile
- Engenharia de Compiladores, Engenharia Reversa, Game Hacking e Velejar

</v-clicks>

::right::

# motivações

<v-clicks>

- Cansei de ouvir pessoas com medo de debuggers
- Ecosistema Apple tem seus quirks, sim
- Porque eu acredito lol

</v-clicks>

---

# só relembrando Mach traps vs syscalls

<div class="text-sm op-60 -mt-3 mb-5">mesma instrução, mesmo kernel — o que muda é o sinal de <code>x16</code></div>

<div class="recap">

|               | Mach trap         | BSD syscall            | rotina MIG                  |
| ------------- | ----------------- | ---------------------- | --------------------------- |
| exemplo       | `task_for_pid()`  | `task_read_for_pid()`  | `mach_vm_read()`            |
| `x16`         | `-45`             | `539`                  | —                           |
| entrada       | `svc #0x80`       | `svc #0x80`            | `mach_msg()` → trap `-31`   |
| declarado em  | `mach_traps.h`    | `syscalls.master`      | `.defs` + `mig(1)`          |

</div>

<div class="xnusrc">
  <code>osfmk/mach/mach_traps.h</code> &nbsp;·&nbsp;
  <code>bsd/kern/syscalls.master</code> &nbsp;·&nbsp;
  <code>osfmk/mach/mach_vm.defs</code>
</div>

<div class="mt-7 text-xs op-70 leading-relaxed">
  <code>mach_vm.defs</code> &nbsp;·&nbsp; 627 linhas
  &nbsp;→&nbsp; <span class="text-[#00ff41]">11.058</span> linhas de C geradas
  &nbsp;<span class="op-60">(User.c + Server.c + header)</span>
</div>

<style>
.recap table {
  font-size: 0.72rem;
}
.recap th,
.recap td {
  padding: 0.4em 0.75em;
}
.recap td:first-child {
  opacity: 0.6;
}
</style>

<!--
Recap slide, sets up everything after it.

The punchline is that a Mach trap and a BSD syscall are the SAME instruction.
Verified live with lldb:

  task_for_pid          mov x16, #-0x2d    (-45)   svc #0x80
  task_read_for_pid     mov x16, #0x21b    (539)   svc #0x80
  task_inspect_for_pid  mov x16, #0x21a    (538)   svc #0x80

Identical svc. The kernel looks at the SIGN of x16: negative indexes the Mach
trap table, positive indexes the BSD syscall table. That is the entire
distinction people make such a fuss about.

MIG is a different animal — it is not a trap at all. A MIG routine is a message
send over a port. The generated client stub packs the arguments into a
mach_msg_header_t and calls mach_msg(), and mach_msg itself is what eventually
traps (mach_msg_trap is -31, mach_msg2_trap is -47).

The codegen number is the thing to dwell on. mig(1) ships at /usr/bin/mig
(bootstrap_cmds-138) and the .defs files are right there in the SDK:

  mig -user mach_vmUser.c -server mach_vmServer.c \
      -header mach_vm.h /path/to/mach/mach_vm.defs

  627 lines in  ->  5310 User.c + 4239 Server.c + 1509 header = 11058 lines out

Nobody hand-writes Mach IPC. That is why the signatures in the SDK headers look
the way they do — they are generated, not designed for humans. Good moment to
mention that this is also why the parameter names are so terse.
-->

---
clicks: 5
---

<div class="relative w-full" style="height: 430px">

<div
  v-motion
  :initial="{ y: 170 }"
  :enter="{ y: 170 }"
  :click-1="{ y: 0, transition: { duration: 650 } }"
  class="absolute top-0 left-0 w-full text-center"
>

# ta, primeiro, que p***a é um debugger mesmo?

</div>

<div
  v-motion
  :initial="{ opacity: 1 }"
  :enter="{ opacity: 1 }"
  :click-5="{ opacity: 0, transition: { duration: 350 } }"
  class="absolute left-0 w-full px-10 mech-list"
  style="top: 130px"
>

<v-clicks at="2">

- **`DYLD_INTERPOSE`** · troca o simbolo antes da `main()` sequer executar, ou seja, nem envolve o kernel
- **`task_for_pid()`** · pede pro kernel a task_port do PID alvo, mas via uma mach trap (id `-45`)
- **`task_read_for_pid()`** · mesmo esquema, pede pro kernel a task_port, mas via uma syscall (id `539`)

</v-clicks>

</div>

<div
  v-motion
  :initial="{ opacity: 0 }"
  :enter="{ opacity: 0 }"
  :click-5="{ opacity: 1, transition: { duration: 500, delay: 250 } }"
  class="absolute left-0 w-full px-14"
  style="top: 118px"
>

<div class="fbox red">task_for_pid(mach_task_self(), pid, &amp;port)</div>
<div class="farrow">trap <span class="fsw">-45</span> &nbsp;·&nbsp; user → kernel</div>
<div class="fbox blue">mac_proc_check_get_task(cred, &amp;pident, TASK_FLAVOR_CONTROL)</div>
<div class="farrow">send right &nbsp;·&nbsp; kernel → user</div>
<div class="fbox red">mach_vm_read_overwrite(port, addr, size, data, &amp;outsize)</div>
<div class="farrow">MIG &nbsp;·&nbsp; user → kernel</div>
<div class="fbox grey">target vm_map &nbsp;—&nbsp; never scheduled</div>

<div class="xnusrc">
  <code>osfmk/mach/mach_traps.h</code> &nbsp;·&nbsp;
  <code>bsd/kern/kern_proc.c</code> &nbsp;·&nbsp;
  <code>osfmk/mach/mach_vm.defs</code>
</div>

</div>

</div>

<style>
.mech-list ul {
  font-size: 0.82rem;
  line-height: 1.5;
}
.fbox {
  text-align: center;
  font-size: 0.7rem;
  padding: 0.5em 0.6em;
  border: 1px solid;
  border-radius: 2px;
}
.fbox.red {
  border-color: #ef4444;
  background: rgba(239, 68, 68, 0.1);
  color: #ffb4b4;
}
.fbox.blue {
  border-color: #3b82f6;
  background: rgba(59, 130, 246, 0.1);
  color: #b9d2ff;
}
.fbox.grey {
  border-color: #6b7280;
  background: rgba(107, 114, 128, 0.1);
  color: #c9ccd1;
}
.farrow {
  text-align: center;
  font-size: 0.6rem;
  opacity: 0.55;
  padding: 0.28em 0;
}
.farrow::before {
  content: '↓';
  display: block;
  font-size: 0.9rem;
  line-height: 1;
}
.fsw {
  color: #00ff41;
}
</style>

<!--
Framing slide. The question the whole talk answers: what IS a debugger, mechanically?

Three answers, increasing in privilege:

1. DYLD_INTERPOSE — not really debugging at all. You rewrite the process from the
   inside before main() runs. No kernel involvement, no permission check, nothing to
   entitle. This is the demo binary's gallywix hook.

2. task_for_pid() — now you are asking the kernel for authority over someone else.
   Mach trap -45. This is where the entitlement story starts.

3. task_read_for_pid() — same question, least privilege. Syscall 539.

Then the diagram. The point to land: the target process NEVER RUNS during any of this.
It is grey because it is passive — it never executes a single instruction on the
debugger's behalf. Everything happens in the debugger's context (red) or the kernel's
(blue). Each colour change is a context switch.

Note mach_vm_read_overwrite's first parameter is typed vm_map_read_t, not task_t —
the flavor hierarchy from the next slide is already visible in the signature.
-->

---
layout: two-cols-header
layoutClass: gap-8
---

# por baixo dos panos

<div class="text-sm op-60 -mt-3 mb-4">o "encanamento" Mach + MIG que tudo depende, do Frida ao LLDB</div>

::left::

<v-clicks depth="2">

- **Darwin &lt; 9** · `task_for_pid()`, uid-gated <!-- <sup class="text-[#00ff41]">*</sup> -->
  - meio que era isso lol
- **Darwin 9 → 19** · MacOs Leopard adiciona um gate:
  - usa um daemon chamado `taskgated`, ele é hardenizado pelo SIP
  - `task_for_pid-allow` · `com.apple.system-task-ports`
- **Darwin 20** · Big Sur quebra as task port em portas dedicadas:
  - `control > read > inspect > name`
  - Dropa as Mach traps pra usar syscalls
- **Darwin 21 → ??** · Monterey "granularizou" mais ainda:
  - `…system-task-ports.control` · `.read`
  - `.name.safe` · `.token.read`

</v-clicks>

::right::

```c {1-4|6-13}
/* mach_traps.h — filed under "Obsolete interfaces." */
kern_return_t task_for_pid(
    mach_port_name_t target_tport,
    int pid, mach_port_name_t *t);

/* syscalls.master — 538 / 539, no public header */
int task_inspect_for_pid(
    mach_port_name_t target_tport,
    int pid, mach_port_name_t *t);

int task_read_for_pid(
    mach_port_name_t target_tport,
    int pid, mach_port_name_t *t);
```

<div class="xnusrc">
  <code>osfmk/mach/mach_traps.h</code> &nbsp;·&nbsp;
  <code>bsd/kern/syscalls.master</code>
</div>

<!-- <div class="abs-bl m-6 text-xs op-50">
  <span class="text-[#00ff41]">*</span> também existiam outras Mach APIs como a processor_set_tasks()
</div> -->

<style>
/* Scoped to the ul, not .col-left: .col-left is the layout component's
   element and never receives this slide's scope id. */
.col-left ul {
  font-size: 0.78rem;
  line-height: 1.45;
}
.col-left ul > li {
  margin: 0.26em 0;
}
.col-left ul ul > li {
  margin: 0.12em 0;
  font-size: 0.95em;
  opacity: 0.9;
}
</style>

---
layout: center
class: text-center
---

# talk is cheap

<!--
TODO: content.

Suggested: the "show me the code" slide — the smallest possible program that
acquires a task port, so the audience sees there is no magic, just a trap and
a port right.
-->

---
clicks: 1
---

# mas o que bloqueiou a gente?

<div class="text-sm op-60 -mt-3 mb-3">mesma syscall, mesma trap,  porém entitlements diferentes</div>

<div class="relative w-full" style="height: 350px">

<div
  v-motion
  :initial="{ opacity: 1 }"
  :enter="{ opacity: 1 }"
  :click-1="{ opacity: 0, transition: { duration: 350 } }"
  class="absolute inset-0 grid grid-cols-2 gap-6 blocked"
>

<div>

```text
$ codesign -d --entitlements - /usr/bin/vmmap

[Key] com.apple.system-task-ports.read
[Key] com.apple.system-task-ports.read.safe
[Key] com.apple.private.dt.instruments…
[Key] com.apple.rootless.datavault.metadata
```

<div class="text-xs op-70 mt-3 leading-relaxed">
  <span class="text-[#00ff41]">.read.safe</span> não é documentada abertamente pela Apple — absent from
  pelo menos tu não acha ela no XNU OSS. Quem da o enforce é o AMFI.
  Tu também acha esse entitlement em outras ferramentas da Apple como:
  <code>vmmap</code>, <code>leaks</code>, <code>heap</code>,
  <code>sample</code>.
</div>

</div>

<div>

```text
$ codesign -d --entitlements - ./mach_vm_adventures

Executable=/Users/horaddrim/labs/mach_vm_adventures
```

<div class="text-xs op-70 mt-3 leading-relaxed">
  No <code>[Dict]</code>. Vazio porque não temos entitlements mesmo.
</div>

</div>

</div>

<div
  v-motion
  :initial="{ opacity: 0 }"
  :enter="{ opacity: 0 }"
  :click-1="{ opacity: 1, transition: { duration: 450, delay: 200 } }"
  class="absolute inset-0 grid grid-cols-2 gap-6 blocked"
>

<div>

```asm
libsystem_kernel.dylib`task_for_pid:
  mov  x16, #-0x2d   ; -45, o ID da Mach trap aqui
  svc  #0x80         ; aqui passamos o contexto pro kernel
  ret                ; x0 = the kern_return_t
```

<div class="text-xs op-70 mt-3 leading-relaxed">
  tudo que estamos conversando acontece dentro dessa chamada <code>svc</code>.
</div>

</div>

<div>

```text
(lldb) finish
(lldb) register read x0

vs target             x0 = 0x0   KERN_SUCCESS
vs target-hardened    x0 = 0x5   KERN_FAILURE
```

<div class="text-xs op-70 mt-3 leading-relaxed">
  Mesma tool, mesma API, mesmo PID. O registrador só serve pra nos mostrar a decisão que o kernel já tomou.
</div>

</div>

</div>

</div>

<style>
.blocked pre {
  font-size: 0.62rem !important;
  line-height: 1.5;
}
.blocked code {
  font-size: inherit;
}
</style>

<!--
The payoff slide for the whole entitlement thread.

Beat 1 — two codesign dumps. vmmap has a wall of entitlements; our tool has
literally no entitlements blob. That is the only difference that matters.

Kill the obvious objection before it is raised: it is NOT that vmmap asks for a
weaker flavor. I tested all four flavors from an unentitled binary against
target-hardened — control, read AND inspect all fail identically. Only
task_name_for_pid survives, and a name port gives you neither read nor write.
Asking for less does not get you more. vmmap wins on entitlements, full stop.

That table is SIP-independent: I re-measured all four flavors against all three
processes with SIP enabled and it came back identical, cell for cell. If someone
asks whether SIP changes the answer here — it does not.

.read.safe is worth dwelling on: it is not in Apple's public entitlement
documentation and it did not appear anywhere in the xnu source sweep, because
the check lives in AMFI, which is closed source. You only learn it exists by
running codesign against Apple's own tools.

Beat 2 — click. Now the mechanism, live. Breakpoint on task_for_pid lands on the
libSystem stub, which IS the trap: mov x16, #-45 then svc #0x80. That is the -45
from the earlier slide, in real ARM64, three instructions long.

Demo note: run ./lldb_trap_demo.sh here. si, si, finish, register read x0.
Run it twice — once against target, once against target-hardened — and the only
thing that changes is x0.
-->

---
clicks: 2
---

# no fim das contas, é um bit

<div class="text-sm op-60 -mt-3 mb-4">o kernel decide no <code>exec()</code></div>

<div class="cspipe">
  <span class="csbox sig">assinatura + entitlements</span>
  <span class="csarrow">→</span>
  <span class="csbox amfi">AMFI + kernel &nbsp;·&nbsp; <code>exec()</code></span>
  <span class="csarrow">→</span>
  <span class="csbox word">csflags</span>
</div>

<div class="text-xs op-55 mt-2 mb-4">
  quem lê: o kernel (gate do <code>task_for_pid</code>)
  &nbsp;·&nbsp;
  o <code>dyld</code> (honra ou ignora <code>DYLD_*</code>) — só lê, nunca escreve
</div>

<div class="csf">

| processo          | `csflags`    | bit que decide                        |
| ----------------- | ------------ | ------------------------------------- |
| `target`          | `0x22000205` | `CS_GET_TASK_ALLOW`                   |
| `target-hardened` | `0x22011311` | `CS_RUNTIME` · `CS_FORCED_LV`         |
| `launchd`         | `0x26015b11` | `CS_RESTRICT` · `CS_PLATFORM_BINARY`  |

</div>

<div class="xnusrc">
  <code>osfmk/kern/cs_blobs.h</code>
</div>

<div v-click="1" class="mt-7 text-sm">
  <span class="op-60"> bit</span> <span class="text-[#00ff41]">0x4</span>
  <span class="op-60"> presente → </span><code>task_for_pid</code><span class="op-60"> passa</span>
  &nbsp;·&nbsp;
  <span class="op-60"> ausente → </span><span class="text-[#ff6b6b]">KERN_FAILURE</span>
</div>

<div v-click="2" class="mt-3 text-sm">
  <span class="op-60"> bit</span> <span class="text-[#00ff41]">0x10</span>
  <span class="op-60"> bloqueia </span><code>DYLD_INSERT_LIBRARIES</code><span class="op-60">
  — quem seta é o AMFI, não o </span><code>codesign -o runtime</code>
</div>

<style>
.csf table {
  font-size: 0.72rem;
}
.csf th,
.csf td {
  padding: 0.4em 0.8em;
}
/* Not .pb/.pa — UnoCSS generates those as padding utilities. */
.cspipe {
  display: flex;
  align-items: center;
  gap: 0.6em;
  font-size: 0.62rem;
}
.csbox {
  border: 1px solid;
  border-radius: 2px;
  padding: 0.35em 0.7em;
}
.csbox.sig {
  border-color: #6b7280;
  color: #c9ccd1;
}
.csbox.amfi {
  border-color: #3b82f6;
  color: #b9d2ff;
  background: rgba(59, 130, 246, 0.1);
}
.csbox.word {
  border-color: #00ff41;
  color: #00ff41;
  background: rgba(0, 255, 65, 0.08);
}
.csarrow {
  opacity: 0.45;
}
</style>

<!--
The payoff. Everything in this talk collapses into one 32-bit word that AMFI
writes at exec time and csops(CS_OPS_STATUS) reads back.

Read the table left to right, then land the two clicks.

Click 1 — CS_GET_TASK_ALLOW is 0x4. target has it, target-hardened does not.
That single bit is the entire difference between the demo working and printing
KERN_FAILURE. Not SIP, not the flavor you asked for, not root. One bit.

Click 2 — the nuance that makes this slide worth its time. "Hardened runtime
blocks DYLD_INSERT_LIBRARIES" is the folk answer and it is wrong in an
interesting way.

codesign -o runtime sets exactly ONE bit on disk: CS_RUNTIME (0x10000). Look at
the CD: flags=0x10002(adhoc,runtime). Nothing else. It does NOT set CS_RESTRICT
(0x800), it does NOT set CS_REQUIRE_LV (0x2000).

What actually blocks the insertion on this machine is CS_FORCED_LV (0x10) —
library validation — and AMFI adds it AT EXEC, derived from CS_RUNTIME, along
with CS_HARD (0x100) and CS_ENFORCEMENT (0x1000). That is why the flags word
(0x22011311) has four bits the signature never carried.

Proof that it is derived and not signed in: the CodeDirectory patch two slides
from now clears CS_RUNTIME alone, and all three of those bits disappear with it.

So the honest phrasing on stage: hardened runtime does not block dyld insertion
by itself — it makes AMFI force library validation, and THAT blocks it. Name the
bit, not the feature. CS_RESTRICT never appears on our binary at all; it is
launchd's bit (0x800), which is a different story: a platform binary.

I MEASURED THE OTHER SIDE OF THIS. With SIP disabled, AMFI does NOT set
CS_FORCED_LV — target-hardened came up 0x22010001 and the interpose hook fired
happily against it. Same binary, same signature, same codesign -o runtime. If
someone in the audience tries this on a machine with SIP off and gets the
opposite result, that is why, and it is worth saying out loud: the code
signature is identical, only the AMFI policy changed.

If asked about libsystem_secinit (the Project Zero fuzzing article): that is the
App Sandbox, a userspace library initializer that decides whether to contact
secinitd for a sandbox profile. It is NOT the hardened runtime. Both read the
same entitlements blob, but AMFI enforces in the kernel at exec while secinit
runs in userspace before main(). Nice symmetry worth mentioning: P0's workaround
was to interpose xpc_copy_entitlements_for_self — the same DYLD_INTERPOSE trick
from the first demo, pointed at the security layer itself.

Demo: ./csflags <pid> against target, target-hardened and 1.

QUEM ESCREVE: nobody in userspace. AMFI plus the kernel parse the CodeDirectory
at exec() and write the word there. Careful with "once", because I measured
otherwise: attaching a debugger makes the KERNEL rewrite it — CS_DEBUGGED and
CS_INVALID_ALLOWED go on, CS_KILL comes off (0x22000205 -> 0x32000025 on
target). Still nothing in userspace writing it, but it is not frozen at exec
either. The very next slide has the full table. The entitlement-derived bits are even grouped
in xnu as CS_ENTITLEMENT_FLAGS = CS_GET_TASK_ALLOW | CS_INSTALLER |
CS_DATAVAULT_CONTROLLER | CS_NVRAM_UNRESTRICTED. 0x4 can only come from a signed
entitlements blob.

QUEM LE: the kernel itself, for the task_for_pid gate — and dyld, which is a
pure consumer. xnu's own comment on CS_RESTRICT is literally "tell dyld to treat
restricted", and dyld exports dyld_process_is_restricted(). dyld never writes
these flags; it is told.
-->

---
clicks: 3
---

# essa palavra muda debaixo de você

<div class="text-sm op-60 -mt-3 mb-4">quem escreve o <code>csflags</code> depois do <code>exec()</code></div>

<div class="csd">

| estado do processo              | `P_TRACED` | `csflags`    | o que entra / sai        |
| ------------------------------- | ---------- | ------------ | ------------------------ |
| intocado                        | `0`        | `0x22000205` | `VALID GET_TASK_ALLOW KILL SIGNED` |
| só `task_for_pid`               | `0`        | `0x22000225` | `+INVALID_ALLOWED`       |
| `hddb break` &nbsp;· brk + trap | `0`        | `0x22000225` | — nada muda              |
| `lldb` &nbsp;· ou só `ptrace`   | `1`        | `0x32000025` | `+DEBUGGED` `−KILL`      |

</div>

<div class="xnusrc">
  <code>./csflags &lt;pid&gt;</code> &nbsp;·&nbsp; mesmo pid, quatro momentos
  &nbsp;·&nbsp; nomes sem o prefixo <code>CS_</code>
</div>

<div v-click="1" class="mt-6 text-sm">
  <span class="op-60">o task port já custa </span><code>+CS_INVALID_ALLOWED</code>
  <span class="op-60"> — antes de ler um byte</span>
</div>

<div v-click="2" class="mt-3 text-sm">
  <code>+CS_DEBUGGED</code> <code>-CS_KILL</code>
  <span class="op-60"> são assinatura do </span><code>ptrace</code><span class="op-60">,
  não de "ter um debugger"</span>
</div>

<div v-click="3" class="mt-3 text-sm">
  <span class="op-60">e </span><code>CS_DEBUGGED</code>
  <span class="op-60"> não volta atrás — </span>
  <span class="text-[#00ff41]">is or HAS BEEN debugged</span>
</div>

<style>
.csd table {
  font-size: 0.6rem;
}
.csd th,
.csd td {
  padding: 0.35em 0.7em;
}
.csd td:first-child {
  opacity: 0.7;
}
.csd td:last-child {
  color: #00ff41;
  opacity: 0.85;
}
.csd tr:first-child td:last-child {
  color: inherit;
  opacity: 0.5;
}
.csd tr:last-child td:nth-child(2),
.csd tr:last-child td:nth-child(3) {
  color: #00ff41;
}
</style>

<!--
The slide that qualifies the one before it. "AMFI writes the word at exec" is
true; "and then it never changes" is not, and I only found this because target
started printing its own csflags.

THE TABLE IS ONE PID, FOUR MOMENTS. Run ./csflags against the same running
target between each step. Nothing is rebuilt or re-signed in between.

Row 2 is the one that surprises people: merely handing out the task control port
flips CS_INVALID_ALLOWED — before a single byte is read or written. I isolated
it with a tool that only calls task_for_pid and immediately deallocates. The
kernel is pre-authorising the port holder to invalidate pages, because that is
what a port holder is for.

Row 3 is our own debugger. It writes a brk, catches EXC_BREAKPOINT, restores,
and the target runs on — and the word is IDENTICAL to row 2. No CS_DEBUGGED.
CS_KILL still set.

Row 4 is lldb. And the important part: I reproduced that exact word with
ptrace(PT_ATTACHEXC) alone — no Mach exception ports, no task_for_pid, fifteen
lines of C. So it is ptrace that does this, not debugserver, and not "being
debugged" in the abstract.

CLICK 2 IS THE ONE TO DWELL ON. Look at which bit ptrace takes AWAY: CS_KILL,
"kill if it becomes invalid". A software breakpoint is a write into a signed
executable page, which invalidates it. The ptrace path asks the kernel for a
waiver up front. Our Mach path never asks — and is never killed either, which I
did not expect and cannot fully explain: the mach_vm_protect(VM_PROT_COPY) page
is presumably not what the check watches. Say that honestly if it comes up; I
did not chase it into xnu.

CLICK 3 — CS_DEBUGGED survives detach. Kill lldb, csflags stays 0x32000025. The
flag name is literal: "is or HAS BEEN debugged". A process carries it for life.

HONESTY: all of this is measured behaviour, not source-confirmed. I did not find
the xnu line that sets CS_DEBUGGED. If someone asks "where in the kernel?", the
correct answer is "I measured it from outside, I did not read that path".

Demo, if there is time: ./csflags <pid>, then ./hddb break <pid> <addr> in
another terminal, then ./csflags <pid> again — unchanged. Then attach lldb and
watch it jump.
-->

---
clicks: 1
---

# e o lldb? aí sim o hardened runtime morde

<div class="text-sm op-60 -mt-3 mb-5">mesmo comando, mesmo <code>debugserver</code> — o alvo é que muda</div>

<div class="grid grid-cols-2 gap-6 attach">

<div>

```text
$ lldb -b -o run -- ./target <pass>

p_flag=0x00005806  P_TRACED=1
  0x00000800  P_TRACED
  0x00001000  P_DISABLE_ASLR
csflags=0x32000025
  0x00000020  CS_INVALID_ALLOWED
  0x10000000  CS_DEBUGGED
```

<div class="text-xs op-70 mt-3 leading-relaxed">
  o binário decodifica as duas palavras sozinho — e as duas mudaram.
</div>

</div>

<div>

```text
$ lldb -b -o run -- ./target-hardened <pass>

error: attach failed
(Not allowed to attach to process.)
```

<div class="text-xs op-70 mt-3 leading-relaxed">
  o <code>debugserver</code> é entitled — e mesmo assim o AMFI recusa.
</div>

</div>

</div>

<div v-click="1" class="mt-7 text-sm">
  <span class="op-60">o alvo não tem entitlement nenhum pra isso — quem decide é o
  </span><code>CS_RUNTIME</code><span class="op-60"> gravado no </span><code>exec()</code>
</div>

<style>
.attach pre {
  font-size: 0.62rem !important;
  line-height: 1.5;
}
</style>

<!--
The beat this deck did not have until SIP got turned back on. Worth the minute.

Two lldb runs, same flags, same debugserver. Against target it attaches. Against
target-hardened: "attach failed (Not allowed to attach to process)".

WHY THIS IS THE INTERESTING ONE. debugserver carries
com.apple.private.cs.debugger — it is the most entitled debugging thing on the
machine, and it still gets told no. So this is not the entitlement story from the
last two slides running again. The decision is on the TARGET side: AMFI wrote
CS_RUNTIME at exec, and a hardened-runtime process without
com.apple.security.cs.debugger (or get-task-allow) is simply not debuggable. The
attacker being privileged does not enter into it.

Left column ties back to the anti-debug demo: P_TRACED is lit, arthas notices,
"frostmourne hungers". Same run, two different layers watching each other — the
kernel let lldb in, and the process can still see that it happened.

Callback to the previous slide: csflags=0x32000025 here is row 4 of that table.
CS_DEBUGGED and CS_INVALID_ALLOWED on, CS_KILL off — ptrace's doing, not the
hardened runtime's. Point at it, do not re-explain it.

HONESTY, and it matters because someone will try this at home: on a machine with
SIP DISABLED this slide does not exist. I measured it — with SIP off lldb
attaches to target-hardened perfectly happily, because that is exactly what SIP
relaxes. Same binary, same signature. If you demo this, confirm `csrutil status`
says enabled first.

Do not oversell it either: this blocks the DEBUGGER. reveal_password.sh against
target-hardened fails for a completely different reason (no CS_GET_TASK_ALLOW),
and it failed with SIP off too. Two separate gates, and the next slide separates
them cleanly by turning this one off with a single byte.

Demo: lldb -b -o run -o quit -- ./target-hardened <pass>, then the same against
./target. Nothing else needed.
-->

---
layout: two-cols-header
layoutClass: gap-8
clicks: 4
---

# tentando atacar essa flag diretamente rola?

<div class="text-sm op-60 -mt-3 mb-4">quatro tentativas — uma funciona, e uma funciona pela metade</div>

::left::

<v-clicks>

- <span class="no">✗</span> &nbsp;`csops(CS_OPS_SET_STATUS)`
  - o kernel mascara a entrada: só bits de hardening
- <span class="no">✗</span> &nbsp;`mach_vm_write` na flag
  - csflags mora no kernel, não no processo
- <span class="half">~</span> &nbsp;patchear o CodeDirectory
  - limpa `CS_RUNTIME` — volta o `lldb`, mas o `0x4` continua ausente
- <span class="yes">✓</span> &nbsp;`codesign -s - --entitlements`
  - reassina; AMFI relê no próximo `exec()`

</v-clicks>

::right::

<div class="relative w-full atk" style="height: 300px">

<div v-click="[1, 2]" class="absolute inset-0">

```c
/* only allow setting a subset
   of all code sign flags */
flags &= CS_HARD | CS_EXEC_SET_HARD |
         CS_KILL | CS_EXEC_SET_KILL |
         CS_RESTRICT | CS_REQUIRE_LV |
         CS_ENFORCEMENT |
         CS_EXEC_SET_ENFORCEMENT;

proc_csflags_set(p, flags);
```

<div class="text-xs op-70 mt-2 leading-relaxed">
  é um <span class="text-[#00ff41]">OR</span>, nunca um assign — e
  <code>CS_GET_TASK_ALLOW</code> sequer aparece na máscara.
</div>

<div class="xnusrc"><code>bsd/kern/kern_proc.c</code></div>

</div>

<div v-click="[2, 3]" class="absolute inset-0">

```text
$ ./mach_vm_adventures write <pid> <addr> ...

  csflags  →  struct proc, no kernel
  vm_map   →  o que mach_vm_write alcança
```

<div class="text-xs op-70 mt-2 leading-relaxed">
  não tem endereço pra mirar. e é circular: pra escrever eu precisaria
  justamente do task port que estou tentando conseguir.
</div>

<div class="xnusrc"><code>bsd/sys/proc_internal.h</code></div>

</div>

<div v-click="[3, 4]" class="absolute inset-0">

```text
CD flags   0x00010002  →  0x00000002

csflags    0x22011311  →  0x22000201
codesign --verify      →  valid on disk
lldb / DYLD_INSERT     →  voltaram
task_for_pid           →  0x5 ainda
```

<div class="text-xs op-70 mt-2 leading-relaxed">
  um byte derruba <code>CS_RUNTIME</code>, <code>CS_FORCED_LV</code>,
  <code>CS_HARD</code> e <code>CS_ENFORCEMENT</code> — e não adiciona
  entitlement nenhum.
</div>

</div>

<div v-click="[4, 5]" class="absolute inset-0">

```text
$ codesign -s - -f -o runtime \
    --entitlements get-task-allow.plist th-gta

csflags   0x22010005
          CS_RUNTIME + CS_GET_TASK_ALLOW
task_for_pid            →  kr = 0
```

<div class="text-xs op-70 mt-2 leading-relaxed">
  hardened runtime <span class="text-[#00ff41]">e</span> debugável ao mesmo
  tempo. é exatamente um debug build do Xcode.
</div>

</div>

</div>

<style>
.col-left ul {
  font-size: 0.76rem;
  line-height: 1.45;
}
.col-left ul > li {
  padding-left: 0;
  margin: 0.55em 0;
}
.col-left ul > li::before {
  content: none;
}
.col-left ul ul > li {
  margin: 0.1em 0 0 1.6em;
  font-size: 0.92em;
  opacity: 0.75;
}
.no {
  color: #ff6b6b;
}
.yes {
  color: #00ff41;
}
.half {
  color: #e7c44d;
}
.atk pre {
  font-size: 0.6rem !important;
  line-height: 1.5;
}
.atk code {
  font-size: inherit;
}
</style>

<!--
The "can I just attack the flag" slide. Answer: the flags are a one-way ratchet.

WHAT csflags IS — one 32-bit word per process, written once by AMFI plus the
kernel at exec() from the code signature, then read by everyone who cares. It is
not configuration. There is no API to relax it.

1. csops(CS_OPS_SET_STATUS) — the snippet on the right is the real xnu code, with
   Apple's own comment. Two independent reasons it cannot help: the mask contains
   only hardening bits (CS_GET_TASK_ALLOW and CS_RUNTIME are not in it), and
   proc_csflags_set is defined as
       proc_csflags_update(p, proc_getcsflags(p) | flags)
   an OR. There IS a proc_csflags_clear that ANDs with ~flags, but it is
   kernel-internal and unreachable from the csops path. The ops list tells the
   same story: MARKHARD, MARKKILL, MARKRESTRICT all exist; the only clear-ops are
   CLEARINSTALLER, CLEAR_LV and CLEARPLATFORM — and CLEARPLATFORM is annotated
   DEVELOPMENT-only, so it is not in a release kernel.

2. mach_vm_adventures — cannot help even in principle. csflags is kernel proc
   state, not memory in the target's address space, so mach_vm_write has nothing
   to aim at. And it is circular: you would need the task port you are trying to
   obtain in order to write anything at all.

3. Patching the CodeDirectory — I actually did this, and this is the beat of
   the slide. Work on a COPY. The flags field is a big-endian u32 at offset 12 of
   the CodeDirectory blob; find the blob through LC_CODE_SIGNATURE (dataoff) plus
   the superblob index. Clear CS_RUNTIME: 0x00010002 -> 0x00000002.

   DEMO ORDER, and it is worth doing live because it undoes the previous slide in
   one byte:

     a) unpatched target-hardened  — lldb says "Not allowed to attach to
        process", interpose prints "access denied"
     b) flip the bit
     c) the SAME binary — lldb attaches ("frostmourne hungers"), interpose fires
        ("hello" + "access granted")
     d) codesign --verify -vvv still says "valid on disk" and "satisfies its
        Designated Requirement". It does not notice.

   csflags on the patched copy: 0x22000201. Clearing ONE bit took CS_FORCED_LV,
   CS_HARD and CS_ENFORCEMENT with it, because AMFI derives all three from
   CS_RUNTIME at exec. That is the cleanest evidence in the talk that those bits
   are policy, not signature.

   WHY IT WORKS, say it before someone asks: this is an adhoc signature. There is
   no CMS blob over the CodeDirectory, so a modified CD is just a different,
   equally valid adhoc identity. A Developer-ID-signed binary would fail here.
   Do NOT let this land as "code signing is broken".

   AND THE HALF THAT FAILS — task_for_pid STILL returns 0x5 against the patched
   copy. All four flavors still fail exactly as before; only task_name_for_pid
   survives. No CD flag flip adds an entitlement, and 0x4 can only come from a
   signed entitlements blob. So: CS_RUNTIME gates the DEBUGGER, the
   get-task-allow entitlement gates task_for_pid. Two mechanisms, and this is the
   single best piece of evidence that they are separate — lldb gets in, our tool
   still does not.

4. codesign — works because it is the only path that goes back through the
   supported pipeline: new signature on disk, AMFI re-reads it at the next
   exec(), kernel writes a new csflags word. Measured, keeping hardened runtime
   ON and adding the entitlement:
       th-gta  csflags = 0x22010005   CS_RUNTIME *and* CS_GET_TASK_ALLOW
   and task_for_pid succeeded. The two flags are orthogonal. That is exactly what
   an Xcode debug build of a hardened app looks like.

Everything on this slide is measured with SIP ENABLED, on a stock machine. The
csflags numbers under SIP off are different (target-hardened is 0x22010001
there, without the AMFI-derived bits) but every conclusion on this slide holds
either way.
-->

---
clicks: 1
---

# e como eu sei disso? desliguei o SIP e refiz tudo

<div class="text-sm op-60 -mt-3 mb-4">mesmo binário, mesma assinatura — só a política do AMFI muda</div>

<div class="sipt">

| medição                            | SIP off      | SIP on       | o que muda, em nome           |
| ---------------------------------- | ------------ | ------------ | ----------------------------- |
| `csflags` · `target`               | `0x22000005` | `0x22000205` | `+KILL`                       |
| `csflags` · `target-hardened`      | `0x22010001` | `0x22011311` | `+FORCED_LV +HARD +KILL +ENFORCEMENT` |
| `csflags` · `launchd`              | `0x26014a01` | `0x26015b11` | `+FORCED_LV +HARD +ENFORCEMENT` |
| `DYLD_INSERT` → `target-hardened`  | hook roda    | access denied | quem barra: `FORCED_LV`      |
| `lldb` → `target-hardened`         | anexa        | attach failed | quem barra: `RUNTIME`        |

</div>

<div class="text-xs op-55 mt-4 leading-relaxed">
  nomes sem o prefixo <code>CS_</code> &nbsp;·&nbsp; idêntico nos dois:
  <code>task_for_pid</code> nos três alvos &nbsp;·&nbsp; as quatro flavors
  &nbsp;·&nbsp; o patch no CodeDirectory &nbsp;·&nbsp; <code>DYLD_INSERT</code> no
  <code>target</code> &nbsp;·&nbsp; <code>P_TRACED</code>
</div>

<div v-click="1" class="mt-6 text-sm">
  <span class="op-60">a assinatura no disco é byte a byte a mesma — quem muda de ideia
  é o </span><span class="text-[#00ff41]">AMFI</span><span class="op-60">, no </span><code>exec()</code>
</div>

<style>
.sipt table {
  font-size: 0.62rem;
}
.sipt th,
.sipt td {
  padding: 0.35em 0.7em;
}
.sipt td:last-child {
  color: #00ff41;
  opacity: 0.85;
}
.sipt td:first-child {
  opacity: 0.75;
}
</style>

<!--
The methodology slide. Short, but it is the one that makes everything before it
trustworthy — and it is the answer to "mas na minha máquina deu diferente".

How this table exists: every measurement in this talk was originally taken on a
machine with SIP DISABLED, because that is how the lab was set up. Then I ran
`csrutil enable`, rebooted, and re-ran the whole list from scratch. Confirm the
machine first, always: `csrutil status` and `nvram boot-args`.

Read the three csflags rows as one fact: no bit in any of them comes from a
different signature. The binaries were not rebuilt, not re-signed — the files on
disk are byte for byte identical across both columns. AMFI simply derives more
bits at exec() when SIP is on: CS_FORCED_LV, CS_HARD, CS_ENFORCEMENT, CS_KILL.

The two behaviour rows follow from that. CS_FORCED_LV is what kills
DYLD_INSERT_LIBRARIES against target-hardened, and CS_RUNTIME being actually
enforced is what makes debugserver get told no. Neither demo works the same way
in a lab with SIP off — which is exactly the trap I fell into, and the reason
this slide exists instead of a confident wrong claim.

What did NOT move is just as interesting, and it is the bottom line of the
slide: task_for_pid against all three targets, all four flavors, the
CodeDirectory patch, dyld insertion against plain target, P_TRACED. Entitlement
decisions are AMFI's job with or without SIP. SIP is not the gate people think
it is — it changes the hardening policy around the gate.

If asked "should I demo with SIP off?": no. You will draw conclusions that do not
hold on anyone else's machine. Every number on these slides is SIP ON unless the
slide says otherwise.
-->

---

# pra não falar que eu não mencionei iOS

<div class="text-sm op-60 -mt-3 mb-5">mesmo protocolo, só que a trap acontece do outro lado do cabo</div>

<div class="ios">

<div class="ibox host">lldb &nbsp;<span class="op-50">· host, macOS</span></div>
<div class="iarrow">GDB Remote Serial Protocol</div>
<div class="ibox wire">usbmuxd &nbsp;<span class="op-50">· <code>/var/run/usbmuxd</code></span></div>
<div class="iarrow">USB &nbsp;ou&nbsp; WiFi</div>
<div class="ibox svc">lockdownd &nbsp;<span class="op-50">· autentica o pareamento</span></div>
<div class="iarrow"><code>com.apple.debugserver</code></div>
<div class="ibox svc">debugserver &nbsp;<span class="op-50">· roda no device</span></div>
<div class="iarrow"><code>task_for_pid()</code> &nbsp;·&nbsp; a mesma trap <span class="fsw2">-45</span></div>
<div class="ibox app">o app alvo</div>

</div>

<style>
.ios {
  margin-top: 0.2rem;
}
.ibox {
  text-align: center;
  font-size: 0.68rem;
  padding: 0.4em 0.6em;
  border: 1px solid;
  border-radius: 2px;
}
.ibox.host {
  border-color: #ef4444;
  background: rgba(239, 68, 68, 0.1);
  color: #ffb4b4;
}
.ibox.wire {
  border-color: #6b7280;
  background: rgba(107, 114, 128, 0.1);
  color: #c9ccd1;
}
.ibox.svc {
  border-color: #3b82f6;
  background: rgba(59, 130, 246, 0.1);
  color: #b9d2ff;
}
.ibox.app {
  border-color: #6b7280;
  background: rgba(107, 114, 128, 0.06);
  color: #9aa0a6;
}
.iarrow {
  text-align: center;
  font-size: 0.56rem;
  opacity: 0.5;
  padding: 0.18em 0;
}
.iarrow::before {
  content: '↓';
  display: block;
  font-size: 0.85rem;
  line-height: 1;
}
.fsw2 {
  color: #00ff41;
}
</style>

<!--
The "sim, eu sei que iOS existe" slide. One minute, no demo.

The point: none of this talk was macOS-only. The protocol between lldb and
debugserver is the GDB Remote Serial Protocol either way. What changes on iOS is
only the TRANSPORT and the fact that you cannot just fork/exec debugserver
yourself — you have to ask lockdownd for it.

usbmuxd multiplexes TCP over the USB cable (socket lives at /var/run/usbmuxd on
the host). Over WiFi it is the same protocol without the multiplexer. lockdownd
is the device-side gatekeeper: it validates the pairing record and only then
starts the requested service by name.

Service names verified in libimobiledevice's debugserver.h:
    #define DEBUGSERVER_SERVICE_NAME "com.apple.debugserver"
    #define DEBUGSERVER_SECURE_SERVICE_NAME  ... ".DVTSecureSocketProxy"

And the punchline for the talk: once debugserver is running on the device, it
calls task_for_pid on the target app — the exact same trap -45, gated by the
exact same entitlement logic we spent the whole talk on. get-task-allow on the
app is what makes a development build debuggable. Same bit, different silicon.

Caveat: the service names come from libimobiledevice, not from Apple docs. Xcode
is not installed on this laptop so I could not confirm them in DVTFoundation.
-->

---

# uns macetes aleatórios

<div class="text-sm op-60 -mt-3 mb-5">hooka isso e você vê todo o XPC do processo</div>

<div class="xpc">

```text {all|6}
$ dyld_info -exports /usr/lib/system/libxpc.dylib | grep xpc_pipe

    0x00001B9C  _xpc_pipe_create
    0x00001B8C  _xpc_pipe_create_from_port
    0x00033ECC  _xpc_pipe_create_reply_from_port
    0x00005E6C  _xpc_pipe_routine
    0x00033E28  _xpc_pipe_routine_async
    0x00005DDC  _xpc_pipe_routine_with_flags
    0x0001609C  _xpc_pipe_simpleroutine
```

</div>

<div class="text-xs op-60 mt-5 leading-relaxed">
  o símbolo é <span class="text-[#00ff41]"><code>_xpc_pipe_routine</code></span>
  — com underscore, que é o mangling C de <code>xpc_pipe_routine()</code>
</div>

<div class="text-xs op-45 mt-2 leading-relaxed">
  17 símbolos <code>xpc_pipe*</code> ao todo &nbsp;·&nbsp; e nenhuma declaração:
  nem em <code>/usr/include/xpc/</code>, nem no <code>libxpc.tbd</code> do SDK ;)
</div>

<style>
.xpc pre {
  font-size: 0.62rem !important;
  line-height: 1.55;
}
</style>

<!--
Slide de um item só. Tempo pra falar, não pra ler.

xpc_pipe_routine é o funil por onde passa praticamente toda conversa XPC de um
processo. Hooka essa função e você vê request e reply de tudo — sem precisar de
task port, sem entitlement, sem kernel. É DYLD_INTERPOSE do primeiro demo
apontado pra uma coisa que importa de verdade.

Verificado nesta máquina, não é de cabeça:

  dyld_info -exports /usr/lib/system/libxpc.dylib | grep xpc_pipe
      0x00005E6C  _xpc_pipe_routine
      0x00033E28  _xpc_pipe_routine_async
      0x00001B9C  _xpc_pipe_create
      0x00001B8C  _xpc_pipe_create_from_port

O detalhe que vale contar: NÃO existe declaração dela em lugar nenhum. Não está
em /usr/include/xpc/, não está no libxpc.tbd do SDK, não está em nenhum header
do disco. Só o símbolo, no dyld shared cache. Se alguém perguntar a assinatura,
seja honesto: a que circula por aí vem de engenharia reversa da comunidade, não
de documentação da Apple.

Ligação com o resto da talk: esse é o vetor mais barato de todos. Nada de
task_for_pid, nada de csflags, nada de entitlement. Só trocar um símbolo antes
da main() rodar.
-->

---
clicks: 2
---

# uns macetes aleatórios #2

<div class="text-sm op-60 -mt-3 mb-4">rodar código iOS nativo no Mac — e por que isso é essa talk inteira</div>

<div class="p0">

<div class="pbox">
  <code>posix_spawn</code> &nbsp;<span class="op-50">· <code>PLATFORM_IOS</code> + <code>START_SUSPENDED</code></span>
</div>
<div class="parrow">o processo nasce parado</div>
<div class="pbox hot">
  <code>task_for_pid</code> &nbsp;<span class="op-50">· <code>com.apple.security.cs.debugger</code> ou root</span>
</div>
<div class="parrow"><code>vm_protect</code> + <code>vm_write</code> &nbsp;·&nbsp; a mesma dupla do nosso <code>hddb</code></div>
<div class="pbox hot">
  patch em <code>_amfi_check_dyld_policy_self</code> &nbsp;<span class="op-50">· <code>return 0x5f</code></span>
</div>
<div class="parrow">agora o dyld aceita interpose</div>
<div class="pbox hot">
  <code>DYLD_INTERPOSE</code> em <code>xpc_copy_entitlements_for_self</code>
</div>
<div class="parrow">devolve <code>com.apple.private.security.no-sandbox</code></div>
<div class="pbox done">
  <code>libsystem_secinit</code> desiste do sandbox &nbsp;<span class="op-50">· <code>SIGCONT</code></span>
</div>

</div>

<div v-click="1" class="mt-5 text-xs op-70 leading-relaxed">
  sem isso: <code>secinit</code> pede perfil pro <code>secinitd(8)</code>, que não
  reconhece o binário iOS, e o processo dá <code>abort(3)</code>
  <span class="text-[#00ff41]">antes da <code>main()</code></span>.
</div>

<div v-click="2" class="mt-3 text-xs op-70 leading-relaxed">
  <code>secinit</code> <span class="op-60">não é</span> hardened runtime: userspace
  antes da <code>main()</code>, decide <span class="text-[#00ff41]">sandbox</span> —
  enquanto <code>CS_RUNTIME</code> é kernel, no <code>exec()</code>. mesmo blob de
  entitlements, dois momentos.
</div>

<div class="mt-4 text-[0.6rem] op-45">
  Samuel Groß · Project Zero · 2021 —
  <span class="op-70">projectzero.google/2021/05/fuzzing-ios-code-on-macos-at-native.html</span>
</div>

<style>
.p0 {
  margin-top: 0.1rem;
}
.pbox {
  text-align: center;
  font-size: 0.62rem;
  padding: 0.4em 0.6em;
  border: 1px solid #6b7280;
  border-radius: 2px;
  background: rgba(107, 114, 128, 0.08);
  color: #c9ccd1;
}
.pbox.hot {
  border-color: #ef4444;
  background: rgba(239, 68, 68, 0.1);
  color: #ffb4b4;
}
.pbox.done {
  border-color: #00ff41;
  background: rgba(0, 255, 65, 0.08);
  color: #00ff41;
}
.parrow {
  text-align: center;
  font-size: 0.55rem;
  opacity: 0.55;
  padding: 0.18em 0;
}
.parrow::before {
  content: '↓';
  display: block;
  font-size: 0.8rem;
  line-height: 1;
}
</style>

<!--
The closing callback. Everything in this talk, used by someone else, for real
work. Three of the four red boxes are demos the audience already watched.

WHAT THEY WANTED: run iOS binaries natively on a Mac, so fuzzing runs at full
arm64 speed with no emulator. The CPU was never the problem - same instructions.
The problem is that macOS refuses to finish setting up an iOS process.

THE WALL: libsystem_secinit.dylib, a userspace library initializer that runs
before main(). Its logic: if the active platform is iOS and the entitlement
com.apple.private.security.no-sandbox is NOT present, set up the App Sandbox. It
asks secinitd(8) for a profile, secinitd cannot identify the app, and secinit
calls abort(3). Dead before main().

THE LOADER, step by step - and this is the slide:

  posix_spawn with PLATFORM_IOS and POSIX_SPAWN_START_SUSPENDED
  task_for_pid on the suspended child  <- needs com.apple.security.cs.debugger
                                          or root. OUR ENTITLEMENT SLIDE, exactly.
  vm_protect + vm_write to patch dyld's _amfi_check_dyld_policy_self so it just
  returns 0x5f, meaning "interposing is allowed"   <- OUR hddb, exactly: writing
                                          instructions into another process
                                          through a task port
  DYLD_INTERPOSE xpc_copy_entitlements_for_self to claim no-sandbox
                                       <- OUR FIRST DEMO, exactly
  SIGCONT

So the punchline to land: they did not find an exotic bug. They used
task_for_pid, they patched instructions in another process's memory, and they
swapped a symbol before main(). That is demos 1, 4 and 9 of this deck, composed
into a working iOS loader.

CLICK 2 - the distinction worth being precise about, because people conflate
these constantly. libsystem_secinit is userspace, before main(), and decides the
SANDBOX. Hardened runtime is CS_RUNTIME (0x10000), evaluated by AMFI in the
KERNEL at exec(). Different layers, different enforcement points. What they share
is the entitlements blob - read twice, at two different moments. That is the
symmetry the csflags slide already set up.

ONE MORE THING FROM THE ARTICLE, consistent with our own measurements: AMFI will
not accept a self-signed certificate for an iOS binary even with SIP off - it
wants an Apple signature or a valid provisioning profile. So this technique gets
the process running; it does not get you arbitrary signing.

VERIFIED: author, function names and the entitlement string were read off the
article, not from memory. It is Samuel Groß, not Ivan Fratric - notes.txt had
that wrong for a while.
-->

---
layout: center
class: text-center
---

# obrigado!

<div class="thanks">

<div class="tbox name">Igor Franca</div>

<div class="tbox typed">
  <span class="tw" style="--d: 0.4s">obrigado especial a <a href="https://instagram.com/mobseccrew" target="_blank">@mobseccrew</a></span>
  <span class="tw" style="--d: 1.7s">e a <a href="https://instagram.com/appleredteamvillage" target="_blank">@appleredteamvillage</a></span>
  <span class="tw end" style="--d: 3s">e a vocês por assistirem 🫶</span>
</div>

<div class="tbox link">
  <a href="https://github.com/Horaddrim" target="_blank">
    <carbon:logo-github /> &nbsp;github.com/Horaddrim
  </a>
</div>

<div class="tbox link">
  <a href="https://linkedin.com/in/igor.franca" target="_blank">
    <carbon:logo-linkedin /> &nbsp;linkedin.com/in/igor.franca
  </a>
</div>

</div>

<style>
.thanks {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.7rem;
  margin-top: 2rem;
}
.tbox {
  min-width: 24rem;
  padding: 0.55em 1.2em;
  border: 1px solid var(--matrix-green-faint);
  border-radius: 2px;
  font-size: 0.9rem;
}
.tbox.name {
  border-color: var(--matrix-green);
  color: var(--matrix-green);
  font-size: 1.1rem;
}
.tbox.typed {
  border-style: dashed;
  color: var(--matrix-green-mid);
  font-size: 0.82rem;
  padding: 1em 1.9em;
  line-height: 1.75;
  max-width: 94%;
}
.tbox.link a {
  border-bottom: none;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  font-size: 0.82rem;
}
/* Uma mascara por linha, escalonada pelo --d inline. Como desliza por
   porcentagem, nao depende do numero de caracteres — da pra editar o texto
   sem mexer no CSS. */
.tw {
  position: relative;
  display: block;
  width: fit-content;
  margin: 0 auto;
  white-space: nowrap;
}
.tw::after {
  content: '';
  position: absolute;
  top: 0.12em;
  bottom: 0.12em;
  left: 0;
  right: -2px;
  background: var(--matrix-bg);
  border-left: 2px solid transparent;
  animation: tw 1.2s steps(26, end) var(--d, 0.4s) forwards,
             twcur 0.6s step-end var(--d, 0.4s) 2;
}
.tw.end::after {
  animation: tw 1.2s steps(26, end) var(--d, 0.4s) forwards,
             twcur 0.6s step-end var(--d, 0.4s) infinite;
}
@keyframes tw {
  to {
    left: 100%;
  }
}
@keyframes twcur {
  0%,
  100% {
    border-left-color: var(--matrix-green);
  }
  50% {
    border-left-color: transparent;
  }
}
</style>

<!--
Fim. Deixa esse slide no ar durante o Q&A — os links ficam visíveis.
-->
