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
| `target`          | `0x22000005` | `CS_GET_TASK_ALLOW`                   |
| `target-hardened` | `0x22010001` | `CS_RUNTIME`                          |
| `launchd`         | `0x26014a01` | `CS_RESTRICT` · `CS_PLATFORM_BINARY`  |

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
  <span class="op-60"> bit</span> <span class="text-[#00ff41]">0x800</span>
  <span class="op-60"> é o que bloqueia </span><code>DYLD_INSERT_LIBRARIES</code><span class="op-60">
  — não o hardened runtime</span>
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

Click 2 — and here is the correction I owe the audience if they were paying
attention earlier. The interpose demo STILL WORKS against target-hardened. That
looks wrong until you read the flags: dyld insertion is governed by CS_RESTRICT
(0x800), library validation by CS_REQUIRE_LV (0x2000). Hardened runtime is
CS_RUNTIME (0x10000) — a completely different bit. codesign -o runtime sets only
CS_RUNTIME, so the binary was never restricted for dyld. launchd has 0x800, which
is why nothing works against it.

If asked about libsystem_secinit (the Project Zero fuzzing article): that is the
App Sandbox, a userspace library initializer that decides whether to contact
secinitd for a sandbox profile. It is NOT the hardened runtime. Both read the
same entitlements blob, but AMFI enforces in the kernel at exec while secinit
runs in userspace before main(). Nice symmetry worth mentioning: P0's workaround
was to interpose xpc_copy_entitlements_for_self — the same DYLD_INTERPOSE trick
from the first demo, pointed at the security layer itself.

Demo: ./csflags <pid> against target, target-hardened and 1.

QUEM ESCREVE: nobody in userspace. AMFI plus the kernel parse the CodeDirectory
at exec() and write the word once. The entitlement-derived bits are even grouped
in xnu as CS_ENTITLEMENT_FLAGS = CS_GET_TASK_ALLOW | CS_INSTALLER |
CS_DATAVAULT_CONTROLLER | CS_NVRAM_UNRESTRICTED. 0x4 can only come from a signed
entitlements blob.

QUEM LE: the kernel itself, for the task_for_pid gate — and dyld, which is a
pure consumer. xnu's own comment on CS_RESTRICT is literally "tell dyld to treat
restricted", and dyld exports dyld_process_is_restricted(). dyld never writes
these flags; it is told.
-->

---
layout: two-cols-header
layoutClass: gap-8
clicks: 4
---

# tentando atacar essa flag diretamente rola?

<div class="text-sm op-60 -mt-3 mb-4">quatro tentativas — só uma funciona, e não é a mais divertida</div>

::left::

<v-clicks depth="2">

- <span class="no">✗</span> &nbsp;`csops(CS_OPS_SET_STATUS)`
  - o kernel mascara a entrada: só bits de hardening
- <span class="no">✗</span> &nbsp;`mach_vm_write` na flag
  - csflags mora no kernel, não no processo
- <span class="no">✗</span> &nbsp;patchear o CodeDirectory
  - limpa `CS_RUNTIME` — e o `0x4` continua ausente
- <span class="yes">✓</span> &nbsp;`codesign -s - --entitlements`
  - reassina; AMFI relê no próximo `exec()`

</v-clicks>

::right::

```c {1-7|9}
/* only allow setting a subset
   of all code sign flags */
flags &= CS_HARD | CS_EXEC_SET_HARD |
         CS_KILL | CS_EXEC_SET_KILL |
         CS_RESTRICT | CS_REQUIRE_LV |
         CS_ENFORCEMENT |
         CS_EXEC_SET_ENFORCEMENT;

proc_csflags_set(p, flags);
```

<div class="text-xs op-70 mt-3 leading-relaxed">
  <code>proc_csflags_set</code> é um <span class="text-[#00ff41]">OR</span>, nunca
  um assign. E <code>CS_GET_TASK_ALLOW</code> sequer aparece na máscara.
</div>

<div class="xnusrc">
  <code>bsd/kern/kern_proc.c</code> &nbsp;·&nbsp;
  <code>bsd/sys/codesign.h</code>
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

3. Patching the CodeDirectory — I actually did this. Flipped the flags field in
   the CD blob, cleared CS_RUNTIME. Results: the binary still runs, csflags drops
   to 0x22000001, and `codesign --verify` still reports "valid on disk" and
   "satisfies its Designated Requirement" — it does not notice. Adhoc signatures
   have no CMS signature over the CD, so a modified CD is just a different, valid
   adhoc identity. AND IT CHANGES NOTHING: task_for_pid still returns 0x5,
   because CS_RUNTIME was never the blocker. This is the beat of the slide — you
   successfully attacked the wrong bit.

4. codesign — works because it is the only path that goes back through the
   supported pipeline: new signature on disk, AMFI re-reads it at the next
   exec(), kernel writes a new csflags word. Measured, keeping hardened runtime
   ON and adding the entitlement:
       th-gta  csflags = 0x22010005   CS_RUNTIME *and* CS_GET_TASK_ALLOW
   and task_for_pid succeeded. The two flags are orthogonal. That is exactly what
   an Xcode debug build of a hardened app looks like.

Caveat if asked: all measured with SIP disabled. The CodeDirectory patch in
particular may behave differently with SIP on.
-->

---

# Finally, proper debugging

<!--
TODO: content.

Suggested: now that we have a task port, what do we actually do with it —
exception ports, thread state, memory read/write.
-->

---

# Putting it together

<!--
TODO: content.

Suggested: assemble the pieces from the prior slides into one coherent path
from "I have a pid" to "I am stopped at a breakpoint."
-->

---

# Okay, but what about LLDB?

<!--
TODO: content.

Suggested close: everything we just built by hand is what debugserver does for you —
and now the entitlement errors it prints are readable instead of mysterious.
-->
