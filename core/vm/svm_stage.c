//
// SedonaPro SVM: staged file installation, validation and rollback.
//
// Files used in the working directory (platform mode):
//   <name>.stage     new file uploaded by provisioning
//   <name>.lkg       last-known-good copy kept during an update trial
//   <name>.failed    file removed by a rollback
//   <name>.rejected  staged file refused by validation or rollback
//   svm.trial        marker of an unfinished update trial
//

#include "sedona.h"
#include "svm_config.h"
#include "svm_stage.h"
#include "svm_loop.h"

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// Image header constants, as in scode.h (which defines tables and must not
// be included twice); keep in sync with vmMagic/vmMajorVer/vmMinorVer.
#define SVM_IMAGE_MAGIC 0x5ED0BA07
#define SVM_IMAGE_MAJOR 0x01
#define SVM_IMAGE_MINOR 0x05

#define TRIAL_FILE  "svm.trial"
#define NAME_MAX_LEN 256

static int     trialActive  = 0;
static int64_t trialStartNs = 0;

////////////////////////////////////////////////////////////////
// File helpers
////////////////////////////////////////////////////////////////

static int exists(const char* path)
{
  struct stat st;
  return stat(path, &st) == 0;
}

static void suffixed(char* out, const char* name, const char* suffix)
{
  snprintf(out, NAME_MAX_LEN, "%s%s", name, suffix);
}

int svmFsyncParentDir(const char* path)
{
#ifdef _WIN32
  return 0;
#else
  char dir[NAME_MAX_LEN];
  const char* slash = strrchr(path, '/');
  int fd, r;
  if (slash == NULL) strcpy(dir, ".");
  else if (slash == path) strcpy(dir, "/");
  else
  {
    size_t len = (size_t)(slash - path);
    if (len >= sizeof(dir)) return -1;
    memcpy(dir, path, len);
    dir[len] = '\0';
  }
  fd = open(dir, O_RDONLY | O_DIRECTORY);
  if (fd < 0) return -1;
  r = fsync(fd);
  close(fd);
  return r;
#endif
}

// rename that replaces the target atomically on POSIX
static int replaceFile(const char* from, const char* to)
{
#ifdef _WIN32
  remove(to);
#endif
  return rename(from, to);
}

// Write data to path through path.tmp + fsync + rename.
int svmWriteFileAtomic(const char* path, const char* data, size_t len)
{
  char tmp[NAME_MAX_LEN];
  FILE* f;
  int ok;
  suffixed(tmp, path, ".tmp");
  f = fopen(tmp, "wb");
  if (f == NULL) return -1;
  ok = fwrite(data, 1, len, f) == len && fflush(f) == 0;
#ifndef _WIN32
  ok = ok && fsync(fileno(f)) == 0;
#endif
  ok = (fclose(f) == 0) && ok;
  if (!ok || replaceFile(tmp, path) != 0) { remove(tmp); return -1; }
  svmFsyncParentDir(path);
  return 0;
}

int svmCopyFileAtomic(const char* from, const char* to)
{
  char tmp[NAME_MAX_LEN];
  char buf[8192];
  FILE *in, *out;
  size_t n;
  int ok = 1;

  in = fopen(from, "rb");
  if (in == NULL) return -1;
  suffixed(tmp, to, ".tmp");
  out = fopen(tmp, "wb");
  if (out == NULL) { fclose(in); return -1; }
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
  if (ferror(in)) ok = 0;
  fclose(in);
  ok = ok && fflush(out) == 0;
#ifndef _WIN32
  ok = ok && fsync(fileno(out)) == 0;
#endif
  ok = (fclose(out) == 0) && ok;
  if (!ok || replaceFile(tmp, to) != 0) { remove(tmp); return -1; }
  svmFsyncParentDir(to);
  return 0;
}

static long fileSize(FILE* f)
{
  long size;
  if (fseek(f, 0, SEEK_END) != 0) return -1;
  size = ftell(f);
  if (fseek(f, 0, SEEK_SET) != 0) return -1;
  return size;
}

////////////////////////////////////////////////////////////////
// Validation
////////////////////////////////////////////////////////////////

int svmValidateScode(const char* path, char* why, size_t whyLen)
{
  uint8_t h[16];
  uint32_t u4;
  long size;
  FILE* f = fopen(path, "rb");
  if (f == NULL) { snprintf(why, whyLen, "cannot open"); return -1; }
  size = fileSize(f);
  if (size < 16 || fread(h, 1, 16, f) != 16)
  {
    fclose(f);
    snprintf(why, whyLen, "too short (%ld bytes)", size);
    return -1;
  }
  fclose(f);

  memcpy(&u4, h, 4);
  if (u4 != SVM_IMAGE_MAGIC) { snprintf(why, whyLen, "bad magic 0x%08x", u4); return -1; }
  if (h[4] != SVM_IMAGE_MAJOR || h[5] != SVM_IMAGE_MINOR)
    { snprintf(why, whyLen, "version %d.%d, VM expects %d.%d", h[4], h[5], SVM_IMAGE_MAJOR, SVM_IMAGE_MINOR); return -1; }
  if (h[6] != SCODE_BLOCK_SIZE)
    { snprintf(why, whyLen, "blockSize %d, VM expects %d", h[6], SCODE_BLOCK_SIZE); return -1; }
  if (h[7] != sizeof(void*))
    { snprintf(why, whyLen, "refSize %d, VM expects %d", h[7], (int)sizeof(void*)); return -1; }
  memcpy(&u4, h + 8, 4);
  if ((long)u4 != size)
    { snprintf(why, whyLen, "header size %u != file size %ld", u4, size); return -1; }
  return 0;
}

int svmValidateSab(const char* path, char* why, size_t whyLen)
{
  uint8_t h[8];
  int last;
  long size;
  FILE* f = fopen(path, "rb");
  if (f == NULL) { snprintf(why, whyLen, "cannot open"); return -1; }
  size = fileSize(f);
  if (size < 13 || fread(h, 1, 8, f) != 8)
  {
    fclose(f);
    snprintf(why, whyLen, "too short (%ld bytes)", size);
    return -1;
  }
  fseek(f, -1, SEEK_END);
  last = fgetc(f);
  fclose(f);
  if (memcmp(h, "sapp", 4) != 0) { snprintf(why, whyLen, "bad magic"); return -1; }
  if (h[4] != 0 || h[5] != 0 || h[6] != 0 || h[7] != 3)
    { snprintf(why, whyLen, "unsupported version"); return -1; }
  if (last != '.') { snprintf(why, whyLen, "missing end marker (truncated?)"); return -1; }
  return 0;
}

////////////////////////////////////////////////////////////////
// Trial marker
////////////////////////////////////////////////////////////////

typedef struct
{
  int attempts;
  int scode;     // kits.scode is part of the trial
  int app;       // app.sab is part of the trial
} Trial;

static int readTrial(Trial* t)
{
  FILE* f = fopen(TRIAL_FILE, "r");
  char line[128];
  memset(t, 0, sizeof(*t));
  if (f == NULL) return -1;
  while (fgets(line, sizeof(line), f) != NULL)
  {
    sscanf(line, "attempts=%d", &t->attempts);
    sscanf(line, "scode=%d", &t->scode);
    sscanf(line, "app=%d", &t->app);
  }
  fclose(f);
  return 0;
}

static int writeTrial(const Trial* t)
{
  char buf[128];
  int n = snprintf(buf, sizeof(buf), "attempts=%d\nscode=%d\napp=%d\n",
                   t->attempts, t->scode, t->app);
  return svmWriteFileAtomic(TRIAL_FILE, buf, (size_t)n);
}

static void endTrial(void)
{
  remove(TRIAL_FILE);
  svmFsyncParentDir(TRIAL_FILE);
  trialActive = 0;
}

////////////////////////////////////////////////////////////////
// Rollback
////////////////////////////////////////////////////////////////

static void restoreOne(const char* name)
{
  char lkg[NAME_MAX_LEN], failed[NAME_MAX_LEN], stage[NAME_MAX_LEN], rejected[NAME_MAX_LEN];
  suffixed(lkg, name, ".lkg");
  suffixed(failed, name, ".failed");
  suffixed(stage, name, ".stage");
  suffixed(rejected, name, ".rejected");

  // a pending stage of the failed update must not be installed again
  if (exists(stage)) replaceFile(stage, rejected);

  if (!exists(lkg))
  {
    printf("svm rollback: no %s, keeping %s\n", lkg, name);
    return;
  }
  if (exists(name)) replaceFile(name, failed);
  if (replaceFile(lkg, name) == 0)
    printf("svm rollback: restored %s from %s\n", name, lkg);
  else
    printf("svm rollback: ERROR cannot restore %s: %s\n", name, strerror(errno));
}

static void rollback(const char* scode, const char* app, const char* reason)
{
  Trial t;
  printf("svm rollback: update failed (%s), restoring last-known-good files\n", reason);
  if (readTrial(&t) != 0) { t.scode = 1; t.app = 1; }
  if (t.scode) restoreOne(scode);
  if (t.app)   restoreOne(app);
  svmFsyncParentDir(scode);
  endTrial();
  svmSafebootReset();
  fflush(stdout);
}

////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////

void svmStageProcessStart(const char* scode, const char* app)
{
  Trial t;
  if (!svmConfig.stageRollback || readTrial(&t) != 0) return;

  t.attempts++;
  if (t.attempts > svmConfig.stageTrialAttempts)
  {
    char reason[64];
    snprintf(reason, sizeof(reason), "%d starts without reaching %d s of operation",
             t.attempts - 1, svmConfig.stageTrialSeconds);
    rollback(scode, app, reason);
    return;
  }
  writeTrial(&t);
  trialActive = 1;
  printf("svm update trial: start %d of %d\n", t.attempts, svmConfig.stageTrialAttempts);
}

// Validate one staged file; a bad file is moved to .rejected.
static int stagedOk(const char* name, int isScode)
{
  char stage[NAME_MAX_LEN], why[128];
  suffixed(stage, name, ".stage");
  if (!svmConfig.stageValidate) return 1;
  if ((isScode ? svmValidateScode(stage, why, sizeof(why))
               : svmValidateSab(stage, why, sizeof(why))) == 0)
    return 1;
  printf("svm stage: rejected %s: %s\n", stage, why);
  return 0;
}

static void reject(const char* name)
{
  char stage[NAME_MAX_LEN], rejected[NAME_MAX_LEN];
  suffixed(stage, name, ".stage");
  suffixed(rejected, name, ".rejected");
  if (exists(stage) && replaceFile(stage, rejected) == 0)
    printf("svm stage: moved %s to %s\n", stage, rejected);
}

static void installOne(const char* name)
{
  char stage[NAME_MAX_LEN];
  suffixed(stage, name, ".stage");
  printf("Moving %s to %s\n", stage, name);
  if (replaceFile(stage, name) != 0)
    printf("svm stage: ERROR cannot move %s: %s\n", stage, strerror(errno));
}

// Keep the current file as .lkg unless a trial already holds one.
static void keepLkg(const char* name, int copy)
{
  char lkg[NAME_MAX_LEN];
  suffixed(lkg, name, ".lkg");
  if (exists(lkg) || !exists(name)) return;
  if ((copy ? svmCopyFileAtomic(name, lkg) : replaceFile(name, lkg)) != 0)
    printf("svm stage: WARNING cannot keep %s: %s\n", lkg, strerror(errno));
}

void svmStageInstall(const char* scode, const char* app)
{
  char scodeStage[NAME_MAX_LEN], appStage[NAME_MAX_LEN], lkg[NAME_MAX_LEN];
  int hasScode, hasApp;
  Trial t;

  suffixed(scodeStage, scode, ".stage");
  suffixed(appStage, app, ".stage");
  hasScode = exists(scodeStage);
  hasApp   = exists(appStage);
  if (!hasScode && !hasApp) return;

  // An update is all-or-nothing: one bad file rejects both.
  if ((hasScode && !stagedOk(scode, 1)) || (hasApp && !stagedOk(app, 0)))
  {
    if (hasScode) reject(scode);
    if (hasApp)   reject(app);
    fflush(stdout);
    return;
  }

  if (!svmConfig.stageRollback)
  {
    if (hasScode) installOne(scode);
    if (hasApp)   installOne(app);
    svmFsyncParentDir(scode);
    fflush(stdout);
    return;
  }

  // Start a new trial, or extend the one already in progress
  // (an interrupted install or a second update before acceptance).
  if (readTrial(&t) != 0)
  {
    suffixed(lkg, scode, ".lkg"); remove(lkg);
    suffixed(lkg, app, ".lkg");   remove(lkg);
    t.attempts = 1;
    t.scode = 0;
    t.app = 0;
  }
  // New kits may rewrite app.sab, so the app is always part of the trial.
  t.scode |= hasScode;
  t.app = 1;
  if (writeTrial(&t) != 0)
  {
    printf("svm stage: ERROR cannot write %s, installing without rollback\n", TRIAL_FILE);
    if (hasScode) installOne(scode);
    if (hasApp)   installOne(app);
    fflush(stdout);
    return;
  }

  if (hasScode) { keepLkg(scode, 0); installOne(scode); }
  if (hasApp)   { keepLkg(app, 0);   installOne(app); }
  else          keepLkg(app, 1);
  svmFsyncParentDir(scode);

  trialActive = 1;
  printf("svm update trial: accepted after %d s of operation, up to %d starts\n",
         svmConfig.stageTrialSeconds, svmConfig.stageTrialAttempts);
  fflush(stdout);
}

void svmStageVmStarting(void)
{
  trialStartNs = svmMonotonicNs();
  svmLoopVmStarting();
}

int svmStageTrialActive(void)
{
  return trialActive;
}

int svmStageRollbackOnFailure(const char* scode, const char* app, int code)
{
  char reason[64];
  if (!svmConfig.stageRollback || !trialActive) return 0;
  snprintf(reason, sizeof(reason), "VM error %d", code);
  rollback(scode, app, reason);
  return 1;
}

void svmTrialTick(void)
{
  if (!trialActive) return;
  if (svmMonotonicNs() - trialStartNs < (int64_t)svmConfig.stageTrialSeconds * 1000000000LL)
    return;
  endTrial();
  printf("svm update trial: update accepted\n");
  fflush(stdout);
}
