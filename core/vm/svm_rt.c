//
// SedonaPro SVM: real-time process settings (options rt.*).
//
// Applied to the main (VM) thread at process start, before kits create
// their I/O threads; those threads inherit CPU affinity and scheduling
// policy (pthread default PTHREAD_INHERIT_SCHED).
//

#include "svm_config.h"

#if defined(__linux__)

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static int applyCpus(const char* list)
{
  cpu_set_t set;
  const char* p = list;
  CPU_ZERO(&set);
  while (*p)
  {
    char* end;
    long a = strtol(p, &end, 10), b;
    if (end == p) return -1;
    b = a;
    if (*end == '-')
    {
      p = end + 1;
      b = strtol(p, &end, 10);
      if (end == p) return -1;
    }
    if (a < 0 || b < a || b >= CPU_SETSIZE) return -1;
    for (; a <= b; ++a) CPU_SET((int)a, &set);
    p = end;
    if (*p == ',') ++p;
    else if (*p) return -1;
  }
  return sched_setaffinity(0, sizeof(set), &set);
}

int svmRtApply(void)
{
  int fails = 0;

  if (svmConfig.rtMlockall)
  {
    int flags = MCL_CURRENT | MCL_FUTURE;
#ifdef MCL_ONFAULT
    // lock pages when first touched: thread stacks are not locked in full
    flags |= MCL_ONFAULT;
#endif
    if (mlockall(flags) == 0)
      printf("svm rt: memory locked\n");
    else
    {
      printf("svm rt: mlockall failed: %s (LimitMEMLOCK / CAP_IPC_LOCK)\n", strerror(errno));
      fails++;
    }
  }

  if (svmConfig.rtCpus[0])
  {
    errno = 0;
    if (applyCpus(svmConfig.rtCpus) == 0)
      printf("svm rt: CPU affinity %s\n", svmConfig.rtCpus);
    else
    {
      printf("svm rt: cannot set CPU affinity '%s': %s\n", svmConfig.rtCpus,
             errno ? strerror(errno) : "invalid list");
      fails++;
    }
  }

  if (strcmp(svmConfig.rtSched, "other") != 0)
  {
    struct sched_param sp;
    int policy = strncmp(svmConfig.rtSched, "fifo:", 5) == 0 ? SCHED_FIFO : SCHED_RR;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = atoi(strchr(svmConfig.rtSched, ':') + 1);
    if (sched_setscheduler(0, policy, &sp) == 0)
      printf("svm rt: scheduling %s\n", svmConfig.rtSched);
    else
    {
      printf("svm rt: cannot set scheduling %s: %s (LimitRTPRIO / CAP_SYS_NICE)\n",
             svmConfig.rtSched, strerror(errno));
      fails++;
    }
  }

  if (fails && svmConfig.rtStrict)
  {
    printf("svm rt: rt.strict=1, refusing to start\n");
    return -1;
  }
  return 0;
}

#else

#include <stdio.h>
#include <string.h>

int svmRtApply(void)
{
  if (svmConfig.rtMlockall || svmConfig.rtCpus[0] || strcmp(svmConfig.rtSched, "other"))
  {
    printf("svm rt: rt.* options are not supported on this platform\n");
    return svmConfig.rtStrict ? -1 : 0;
  }
  return 0;
}

#endif
