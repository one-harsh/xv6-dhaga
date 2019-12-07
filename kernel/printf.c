//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}

static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

void print(char *fmt, va_list ap) {
  int i, c;
  char *s;

  if (fmt == 0)
    panic("null fmt");

  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'c':
      consputc(va_arg(ap, int));
      break;
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'u':
      printint(va_arg(ap, int), 10, 0);
      break;
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }
}

// Print to the console. only understands %d, %x, %p, %s.
void
printf(char *fmt, ...) {
  int locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  va_list ap;
  va_start(ap, fmt);
  print(fmt, ap);

  if(locking)
    release(&pr.lock);
}

void logif(int debug_flag, char *fmt, ...) {
  if(debug_flag == 0){
    return;
  }

  int locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  va_list ap;
  va_start(ap, fmt);
  print(fmt, ap);

  if(locking)
    release(&pr.lock);
}

// Logs to console if DEBUGMODE is on.
void logf(char *fmt, ...) {
  int locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  va_list ap;
  va_start(ap, fmt);
  if (DEBUGMODE) {
      print(fmt, ap);
  }

  if(locking)
    release(&pr.lock);
}

void lognoisef(char *fmt, ...) {
  int locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  va_list ap;
  va_start(ap, fmt);
  if (NOISEMODE) {
      print(fmt, ap);
  }

  if(locking)
    release(&pr.lock);
}

// Logs thread's trapframe to console if NOISEMODE is on.
void logthreadf(struct thread *t) {
  lognoisef("\nlogging for thread - %d on %d\n", t->tid, cpuid());
  lognoisef("\nt->tf->epc - %p\n", t->tf->epc);
  lognoisef("t->tf->sp - %p\n", t->tf->sp);
  lognoisef("t->tf->ra - %p\n", t->tf->ra);
  
  lognoisef("\nt->tf->s0 - %p\n", t->tf->s0);
  lognoisef("t->tf->s1 - %p\n", t->tf->s1);
  lognoisef("t->tf->s2 - %p\n", t->tf->s2);
  lognoisef("t->tf->s3 - %p\n", t->tf->s3);
  lognoisef("t->tf->s4 - %p\n", t->tf->s4);
  lognoisef("t->tf->s5 - %p\n", t->tf->s5);
  lognoisef("t->tf->s6 - %p\n", t->tf->s6);
  lognoisef("t->tf->s7 - %p\n", t->tf->s7);
  lognoisef("t->tf->s8 - %p\n", t->tf->s8);
  lognoisef("t->tf->s9 - %p\n", t->tf->s9);
  lognoisef("t->tf->s10 - %p\n", t->tf->s10);
  lognoisef("t->tf->s11 - %p\n\n", t->tf->s11);
}

void
backtrace(void)
{
  register uint64 fp asm("s0");
  uint64 ra, low = PGROUNDDOWN(fp) + 16, high = PGROUNDUP(fp);

  while(!(fp & 7) && fp >= low && fp < high){
    ra = *(uint64*)(fp - 8);
    printf("[<%p>]\n", ra);
    fp = *(uint64*)(fp - 16);
  }
}

void
panic(char *s)
{
  pr.locking = 0;
  int tid = mythread()->tid;
  int cpu = cpuid();
  printf("PANIC: ");
  printf(s);
  printf("\n");
  printf("PANIC in thread - %d on %d\n", tid, cpu);
  backtrace();
  printf("HINT: restart xv6 using 'make qemu-gdb', type 'b panic' (to set breakpoint in panic) in the gdb window, followed by 'c' (continue), and when the kernel hits the breakpoint, type 'bt' to get a backtrace\n");
  panicked = 1; // freeze other CPUs
  for(;;)
    ;
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}
