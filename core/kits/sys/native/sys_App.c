//
// SedonaPro: sys::App natives.
//

#include "sedona.h"
#include "svm_loop.h"

// static long App.scanResync(long nsUntilDeadline, long periodNs)
int64_t sys_App_scanResync(SedonaVM* vm, Cell* params)
{
  int64_t nsUntilDeadline = *(int64_t*)params;
  int64_t periodNs        = *(int64_t*)(params+2);
  (void)vm;
  return svmScanResync(nsUntilDeadline, periodNs);
}
