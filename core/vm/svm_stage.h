//
// SedonaPro SVM: installation of staged files, validation and
// last-known-good rollback (options stage.*, see svm_config.h).
//

#ifndef __SVM_STAGE_H
#define __SVM_STAGE_H

#include <stddef.h>

// Image checks (0 = valid, otherwise why is filled).
int svmValidateScode(const char* path, char* why, size_t whyLen);
int svmValidateSab(const char* path, char* why, size_t whyLen);

// Platform mode: call once at process start, before the first install.
// Counts starts of an unfinished update trial and rolls back when the
// limit is exceeded.
void svmStageProcessStart(const char* scode, const char* app);

// Platform mode: install <scode>.stage / <app>.stage if present.
void svmStageInstall(const char* scode, const char* app);

// Platform mode: the VM is about to run (starts the trial timer).
void svmStageVmStarting(void);

// Platform mode: the VM failed with code. Returns 1 if an update under
// trial was rolled back and the VM should be started again.
int svmStageRollbackOnFailure(const char* scode, const char* app, int code);

// Called from Sys.sleep each scan cycle; accepts the update after
// stage.trial.seconds.
void svmTrialTick(void);

// Non-zero while an update trial is running.
int svmStageTrialActive(void);

// Atomic file helpers shared with the FileStore natives and safeboot.
int svmFsyncParentDir(const char* path);
int svmWriteFileAtomic(const char* path, const char* data, size_t len);
int svmCopyFileAtomic(const char* from, const char* to);

#endif
