#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct thread threads[NTHREAD];

struct proc *initproc;

int nextpid = 1;
int nextTid = 1;
struct spinlock pid_lock;
struct spinlock tid_lock;

extern void forkret(void);
extern int growproc(int);
static void freeThread(struct thread *t);
static void wakeup1(struct thread *chan);

extern char trampoline[]; // trampoline.S

void acq_proc(struct proc *p, char *mname) {
  acquire(&p->lock);
  if(LOG_LOCKS && p->pid != 0){
    printf("h[%d] acq'd PROC [%d] in %s()\n", cpuid(), p->pid, mname);
  }
}

void rel_proc(struct proc *p, char *mname) {
  if(LOG_LOCKS && p->pid != 0){
    printf("h[%d] rel'ing P [%d] in %s()\n", cpuid(), p->pid, mname);
  }
  release(&p->lock);  
}

void acq_thread(struct thread *t, char *mname) {
  acquire(&t->lock);
  if(LOG_LOCKS && (t->tid == 3 || t->tid == 4)){
    printf("h[%d] acq'd T [%d] in %s()\n", cpuid(), t->tid, mname);
  }
}

void rel_thread(struct thread *t, char *mname) {
  if(LOG_LOCKS && (t->tid == 3 || t->tid == 4)){
    printf("h[%d] rel'ing T [%d] in %s()\n", cpuid(), t->tid, mname);
  }
  release(&t->lock);
}

void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
  }

  struct thread *t;
  initlock(&tid_lock, "nexttid");
  for(t = threads; t < &threads[NTHREAD]; t++) {
      initlock(&t->lock, "thread");

      // Allocate a page for the thread's kernel stack.
      // Map it high in memory, followed by an invalid
      // guard page.
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      uint64 va = KSTACK((int) (t - threads));
      kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
      t->kstack = va;
  }

  kvminithart();
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  struct proc *p = 0;
  push_off();
  struct thread *t = mythread();
  if (t != 0x0) {
    p = t->parentProc;
  }
  pop_off();
  return p;
}

struct thread*
mythread(void) {
  push_off();
  struct cpu *c = mycpu();
  struct thread *t = c->thread;
  pop_off();
  return t;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

int alloctid() {
  int tid;

  acquire(&tid_lock);
  tid = nextTid;
  nextTid = nextTid + 1;
  release(&tid_lock);

  return tid;
}

// Returns with t->lock held.
static struct thread*
allocThread(struct proc *p, uint64 fnAddr, uint64 stackPtrAddr, int isMain) {
  struct thread *t;

  int found = 0;
  for(int i = 0; i < NTHREAD; i++) {
    t = &threads[i];
    acq_thread(t, "allocthread");
    if(t->state == UNUSED || t->state == ZOMBIE) {
      found = 1;
      t->tid = alloctid();

      // Allocate a trapframe page.
      if((t->tf = (struct trapframe *)kalloc()) == 0) {
        rel_thread(t, "allocthread_failTrapframeAlloc");
        return 0;
      }

      break;
    }

    rel_thread(t, "allocthread_notFound");
  }

  if (found == 0) {
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&t->context, 0, sizeof t->context);

  t->context.ra = (uint64)forkret;
  t->context.sp = t->kstack + PGSIZE;

  if (!isMain){
    *(t->tf) = *(p->threads[0]->tf);
    t->tf->epc = fnAddr;
    t->tf->sp = stackPtrAddr;
  }

  t->state = RUNNABLE;

  return t;
}

int createThread(uint64 va) {
  struct thread *t = mythread();
  int i;

  acq_proc(t->parentProc, "createThread");
  if (!t->parentProc->threads[1]) {
    uint64 oldSize = t->parentProc->sz;
    logf("oldsize for %s - %p\n", t->parentProc->name, oldSize);
    if (growproc((NTHREADPERPROC - 1) * THREADSTACKSIZE) < 0) {
      rel_proc(t->parentProc, "createThread_growFail");
      return 0;
    }

    for (i = 1; i < NTHREADPERPROC; i++) {
      t->parentProc->ustacks[i] = oldSize + THREADSTACKSIZE * i;
    }
  }

  struct thread *temp;
  int found = 0;
  for (i = 1; i < NTHREADPERPROC; i++) {
    temp = t->parentProc->threads[i];
    if (temp != t) {
      if (temp == 0) {
        found = 1;
        break;
      }
    }
  }

  if (!found) {
    rel_proc(t->parentProc, "createThread_noThreadFound");
    return 0;
  }

  struct thread *nt = allocThread(t->parentProc, va, t->parentProc->ustacks[i], 0); // !isMain
  if (!nt) {
    rel_proc(t->parentProc, "createThread_allocThreadFailed");
    return 0;
  }

  t->parentProc->threads[i] = nt;
  nt->parentProc = t->parentProc;

  for(i = 0; i < NOFILE; i++)
    if(nt->parentProc->ofile[i])
      nt->parentProc->ofile[i] = filedup(nt->parentProc->ofile[i]);

  rel_thread(nt, "createThread_sucess");
  rel_proc(t->parentProc, "createThread_success");

  return nt->tid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->thread[0]->lock held.
// If there are no free procs, return 0.
int
allocproc(struct proc *p)
{
  if(p->state != UNUSED) {
    return 0;
  }

  p->pid = allocpid();
  p->threads[0] = allocThread(p, 0, 0, 1); // isMain
  p->threads[0]->parentProc = p;
  //p->ustacks[0] = p->kstacks[0] + PGSIZE;

  // An empty user page table.
  p->pagetable = proc_pagetable(p);

  return 1;
}

// BUGBUG Should require that t->lock must be held?
static void
freeThread(struct thread *t)
{
  // Handle linking of thread->previous & thread->next
  if(t->tf) {
    logthreadf(t, "freeThread");
    kfree((void*)t->tf);
  }
  t->tf = 0;
  t->tid = 0;
  t->chan = 0;
  t->parentProc = 0;
  t->state = UNUSED;
  t->xstate = 0;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if (p->threads) {
    for (int i = 0; i < NTHREADPERPROC; i++) {
      struct thread *t = p->threads[i];
      if (p->threads[i])
        freeThread(t);
    }

    logf("freed all threads\n");
  }

  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->killed = 0;
  p->state = UNUSED;
}

// Create a page table for a given process,
// with no user pages, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  mappages(pagetable, TRAMPOLINE, PGSIZE,
           (uint64)trampoline, PTE_R | PTE_X);

  // map the trapframe just below TRAMPOLINE, for trampoline.S.

  mappages(pagetable, TRAPFRAME, PGSIZE,
           (uint64)(p->threads[0]->tf), PTE_R | PTE_W);

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, PGSIZE, 0);
  uvmunmap(pagetable, TRAPFRAME, PGSIZE, 0);
  if(sz > 0)
    uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x05, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x05, 0x02,
  0x9d, 0x48, 0x73, 0x00, 0x00, 0x00, 0x89, 0x48,
  0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0xbf, 0xff,
  0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x01,
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  int found = 0;
  for (p = proc; p < &proc[NPROC]; p++) {
    acq_proc(p, "userinit");
    if (allocproc(p) == 0) {
      rel_proc(p, "userinit_allocProcFailed");
    }
    else {
      found = 1;
      break;
    }
  }

  if(found == 0) {
    panic("no free proc found for init\n");
  }
  
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->threads[0]->tf->epc = 0;      // user program counter
  p->threads[0]->tf->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  p->threads[0]->state = RUNNABLE;

  rel_thread(p->threads[0], "userinit_mainSuccess");
  rel_proc(p, "userinit_mainSuccess");
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  // BUGBUG? acquire proc lock here?
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  // TODO fail here if p->threads[0] != mythread()

  // Allocate process.
  int found = 0;
  for (np = proc; np < &proc[NPROC]; np++) {
    if (p == np) {
      continue;
    }

    acq_proc(np, "fork");
    if (allocproc(np) == 0) {
      rel_proc(np, "fork_allocProcFailed");
    }
    else {
      found = 1;
      break;
    }
  }

  if(found == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    rel_thread(np->threads[0], "fork_copyFail");
    rel_proc(np, "fork_copyFail");
    return -1;
  }

  np->sz = p->sz;
  np->parent = p;

  // copy saved user registers.
  *(np->threads[0]->tf) = *(p->threads[0]->tf);

  // Cause fork to return 0 in the child.
  np->threads[0]->tf->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(myproc()->ofile[i])
      np->ofile[i] = filedup(myproc()->ofile[i]);

  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  np->state = RUNNABLE;
  np->threads[0]->state = RUNNABLE;

  rel_thread(np->threads[0], "fork_childSuccess");
  rel_proc(np, "fork_success");

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold p->lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    // this code uses pp->parent without holding pp->lock.
    // acquiring the lock first could cause a deadlock
    // if pp or a child of pp were also in exit()
    // and about to try to lock p.
    if(pp->parent == p){
      // pp->parent can't change between the check and the acquire()
      // because only the parent changes it, and we're the parent.
      logf("reparenting %s (%d)\n", pp->name, pp->pid);
      acq_proc(pp, "reparent_child");
      pp->parent = initproc;
      // we should wake up init here, but that would require
      // initproc->lock, which would be a deadlock, since we hold
      // the lock on one of init's children (pp). this is why
      // exit() always wakes init (before acquiring any locks).
      rel_proc(pp, "reparent_childDone");
    }
  }
}

// Exit the current thread. If main thread, then exits the process
// Does not return.
// An exited thread remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct thread *t = mythread();

  logf("exiting t[%d] on h[%d]\n", t->tid, cpuid());

  if(t->parentProc == initproc)
    panic("init exiting");

  if (t->parentProc && t->parentProc->threads[0] != t) {
    logf("exiting t[%d] on h[%d]\n", t->tid, cpuid());
    acq_thread(t, "exiting");
    t->xstate = status;
    t->state = ZOMBIE;

    // Unlink the thread from parent.
    for (int i = 0; i < NTHREADPERPROC; i++) {
      if (t->parentProc->threads[i] == t) {
        t->parentProc->threads[i] = 0;
      }
    }
    
    unscheduling();
    panic("zombie child thread exit");
  }

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(t->parentProc->ofile[fd]){
      struct file *f = t->parentProc->ofile[fd];
      fileclose(f);
      if (t->parentProc && t->parentProc->threads[0] == t) {
        t->parentProc->ofile[fd] = 0;
      }
    }
  }

  begin_op(ROOTDEV);
  iput(t->parentProc->cwd);
  end_op(ROOTDEV);
  t->parentProc->cwd = 0;

  logf("closed all files for %d\n", t->tid);

  // we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acq_thread(initproc->threads[0], "exit_initProc");
  wakeup1(initproc->threads[0]);
  rel_thread(initproc->threads[0], "exit_initProc");

  logf("woken up initproc for %d's exit\n", t->tid);

  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acq_thread(t, "exit_getParent");
  struct proc *original_parent = t->parentProc->parent;

  rel_thread(t, "exit_getParent");
  logf("found the original parent for %d as %s\n", t->tid, original_parent->name);
  
  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acq_thread(original_parent->threads[0], "exit_parentProcThread0");

  acq_thread(t, "exit_selfBeforeReparent");

  // Give any children to init.
  reparent(t->parentProc);
  logf("freeing %s (tid-%d))\n", t->parentProc->name, t->tid);
  freeproc(t->parentProc);

  // Parent might be sleeping in wait().
  wakeup1(original_parent->threads[0]);

  t->xstate = status;
  t->state = ZOMBIE;

  rel_thread(original_parent->threads[0], "exit_parentProcThread0");

  logf("exited t[%d] on h[%d]\n", t->tid, cpuid());

  // Jump into the scheduler, never to return.
  unscheduling();
  panic("zombie main thread exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  /// TODO : CONVERT THIS ENTIRE THING TO BE HANDLED ON THREADS

  struct proc *np;
  int havekids, pid;
  struct thread *t = mythread();

  // hold t->lock for the whole time to avoid lost
  // wakeups from a child's exit().
  acq_thread(t, "wait");

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      if(np->parent == t->parentProc){
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acq_proc(np, "wait_eachprocess");
        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(t->parentProc->pagetable, addr, (char *)&np->threads[0]->xstate,
                                  sizeof(np->threads[0]->xstate)) < 0) {
            rel_proc(np, "wait_failCopyOut");
            rel_thread(t, "wait_failCopyOut");
            return -1;
          }
          freeproc(np);
          rel_proc(np, "wait_success");
          rel_thread(t, "wait_success");
          return pid;
        }
        rel_proc(np, "wait_endEachprocess");
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || t->parentProc->killed){
      rel_thread(t, "wait_killedOrNoChildren");
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(t, &t->lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct thread *t;
  struct cpu *c = mycpu();
  
  c->thread = 0;
  for(;;){
    // Avoid deadlock by giving devices a chance to interrupt.
    intr_on();

    // Run the for loop with interrupts off to avoid
    // a race between an interrupt and WFI, which would
    // cause a lost wakeup.
    intr_off();

    int found = 0;
    for(t = threads; t < &threads[NTHREAD]; t++) {
      acq_thread(t, "scheduler_before");
      if (t->state == RUNNABLE && t->parentProc->pid == 0 && t->parentProc) {
        // freeThread will change the current state to UNUSED.
        freeThread(t);
      }

      if(t->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        t->state = RUNNING;
        c->thread = t;

        logif(LOG_SCHED, "scheduler to T[%d] on h[%d]\n", mythread()->tid, cpuid());
        logthreadf(t, "scheduling");
        swtch(&c->scheduler, &t->context);

        // Thread is done running for now.
        // It should have changed its t->state before coming back.
        c->thread = 0;

        found = 1;
      }

      // ensure that release() doesn't enable interrupts.
      // again to avoid a race between interrupt and WFI.
      c->intena = 0;

      rel_thread(t, "scheduler_after");
    }
    if(found == 0){
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only t->lock
// and have changed thread->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
// 
// Don't try to acquire parentProc in this method as
// thread exit would also call unscheduling which could
// have delinked the parentProc from the current exiting thread.
void
unscheduling(void)
{
  int intena;
  struct thread *t = mythread();

  if(!holding(&t->lock))
    panic("sched t->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(t->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;

  logif(LOG_SCHED, "T[%d] to scheduler h[%d]\n", mythread()->tid, cpuid());
  if (mythread()->state != ZOMBIE)
    logthreadf(mythread(), "unscheduling");

  swtch(&t->context, &mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct thread *t = mythread();
  acq_thread(t, "yield_before");
  t->state = RUNNABLE;

  unscheduling();
  rel_thread(t, "yield_after");
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding t->lock from scheduler.
  rel_thread(mythread(), "forkret");

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(minor(ROOTDEV));
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct thread *t = mythread();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  if(lk != &t->lock){  //DOC: sleeplock0
    acq_thread(t, "sleep");  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  t->chan = chan;
  t->state = SLEEPING;

  unscheduling();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  if(lk != &t->lock){
    rel_thread(t, "sleep");
    acquire(lk);
  }
}

// Wake up all threads sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct thread *t;

  for(t = threads; t < &threads[NTHREAD]; t++) {
    acq_thread(t, "wakeup_chan");
    if(t->state == SLEEPING && t->chan == chan) {
      t->state = RUNNABLE;
    }
    rel_thread(t, "wakeup_chan");
  }
}

// Wake up p if it is sleeping in wait(); used by exit().
// Caller must hold p->lock.
static void
wakeup1(struct thread *t)
{
  if(!holding(&t->lock))
    panic("wakeup1");
  if(t->chan == t && t->state == SLEEPING) {
    t->state = RUNNABLE;
    t->parentProc->state = RUNNABLE;
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acq_proc(p, "kill_foreach");
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      rel_proc(p, "kill_insleep");
      return 0;
    }
    rel_proc(p, "kill_endForEach");
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
