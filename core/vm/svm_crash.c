//
// SedonaPro SVM: fatal signal logging (option crash.handler).
//
// Prints the signal, fault address and a native backtrace to stderr, then
// re-raises the signal with the default action, so the exit status, core
// dumps and systemd Restart= behave exactly as without the handler.
// Resolve addresses with: addr2line -f -e svm <address>
//

#include "svm_config.h"

#if defined(__linux__) && defined(__GLIBC__)

#include <execinfo.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static char altStack[64 * 1024];

static void writeStr(const char* s)
{
  ssize_t r = write(STDERR_FILENO, s, strlen(s));
  (void)r;
}

static void writeHex(uintptr_t v)
{
  char buf[2 + sizeof(v) * 2 + 1];
  int i;
  buf[0] = '0'; buf[1] = 'x';
  for (i = 0; i < (int)(sizeof(v) * 2); ++i)
    buf[2 + i] = "0123456789abcdef"[(v >> ((sizeof(v) * 2 - 1 - i) * 4)) & 0xf];
  buf[sizeof(buf) - 1] = '\0';
  writeStr(buf);
}

static void onFatal(int sig, siginfo_t* info, void* ctx)
{
  void* frames[64];
  int n;
  (void)ctx;

  writeStr("\n*** SVM FATAL SIGNAL ");
  writeStr(strsignal(sig));   // glibc: table lookup, no allocation
  writeStr(" fault address ");
  writeHex((uintptr_t)(info ? info->si_addr : 0));
  writeStr("\n*** native backtrace (addr2line -f -e svm <addr>):\n");
  n = backtrace(frames, 64);
  backtrace_symbols_fd(frames, n, STDERR_FILENO);
  writeStr("*** end of backtrace\n");

  // SA_RESETHAND restored the default action; re-raise it.
  raise(sig);
}

void svmCrashHandlerInstall(void)
{
  static const int sigs[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
  struct sigaction sa;
  stack_t ss;
  void* warm[1];
  unsigned i;

  // load libgcc for backtrace() now, not inside the signal handler
  backtrace(warm, 1);

  ss.ss_sp = altStack;
  ss.ss_size = sizeof(altStack);
  ss.ss_flags = 0;
  sigaltstack(&ss, NULL);

  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = onFatal;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
  sigemptyset(&sa.sa_mask);
  for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); ++i)
    sigaction(sigs[i], &sa, NULL);
}

#else

#include <stdio.h>

void svmCrashHandlerInstall(void)
{
  fprintf(stderr, "svm config: crash.handler is not supported on this platform\n");
}

#endif
