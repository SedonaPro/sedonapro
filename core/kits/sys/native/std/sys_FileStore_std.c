//
// Copyright (c) 2009 Tridium, Inc.
// Licensed under the Academic Free License version 3.0
//
// History:
//   19 May 09  Brian Frank  Move methods from sys_File_std.c
//

#include "sedona.h"

#include <errno.h>       // for errno, strerror
#include <unistd.h>

#ifndef _WIN32
 #include <sys/stat.h>   // for file mode defns
#endif

#include "svm_config.h"
#include "svm_stage.h"

// =========================================================================== //
// save.atomic: files opened with mode "w" whose name matches
// save.atomic.suffixes are written to <name>.tmp and moved over <name>
// only after a successful flush + fsync + close (and, for *.sab, a format
// check).  A crash or failed save leaves the previous file intact.
// =========================================================================== //

#define ATOMIC_MAX   16
#define ATOMIC_NAME  256

typedef struct
{
  FILE* fp;
  char  target[ATOMIC_NAME];
  char  tmp[ATOMIC_NAME + 8];
} AtomicFile;

static AtomicFile atomicFiles[ATOMIC_MAX];

static int endsWith(const char* s, const char* suffix)
{
  size_t a = strlen(s), b = strlen(suffix);
  return a >= b && strcmp(s + a - b, suffix) == 0;
}

static FILE* atomicOpen(const char* name)
{
  int i;
  FILE* fp;
  if (strlen(name) >= ATOMIC_NAME) return NULL;
  for (i = 0; i < ATOMIC_MAX; ++i)
    if (atomicFiles[i].fp == NULL) break;
  if (i == ATOMIC_MAX)
  {
    printf("save.atomic: too many open files, writing %s directly\n", name);
    return NULL;
  }
  strcpy(atomicFiles[i].target, name);
  snprintf(atomicFiles[i].tmp, sizeof(atomicFiles[i].tmp), "%s.tmp", name);
  fp = fopen(atomicFiles[i].tmp, "wb");
  if (fp == NULL)
  {
    printf("fopen('%s', 'wb') failed, errno=%d (%s)\n", atomicFiles[i].tmp, errno, strerror(errno));
    return (FILE*)-1;
  }
  atomicFiles[i].fp = fp;
  return fp;
}

static AtomicFile* atomicFind(FILE* fp)
{
  int i;
  for (i = 0; i < ATOMIC_MAX; ++i)
    if (atomicFiles[i].fp == fp) return &atomicFiles[i];
  return NULL;
}

// Returns 1 if fp was an atomic file (result in *ok), 0 otherwise.
static int atomicClose(FILE* fp, int* ok)
{
  AtomicFile* a = atomicFind(fp);
  char why[128];
  int good;
  if (a == NULL) return 0;

  good = fflush(fp) == 0;
#ifndef _WIN32
  good = (fsync(fileno(fp)) == 0) && good;
#endif
  good = (fclose(fp) == 0) && good;
  a->fp = NULL;

  if (good && endsWith(a->target, ".sab") && svmValidateSab(a->tmp, why, sizeof(why)) != 0)
  {
    printf("save.atomic: %s not installed: %s\n", a->target, why);
    good = 0;
  }
#ifdef _WIN32
  if (good) remove(a->target);
#endif
  if (good && rename(a->tmp, a->target) != 0)
  {
    printf("save.atomic: cannot rename %s: %s\n", a->tmp, strerror(errno));
    good = 0;
  }
  if (good)
    svmFsyncParentDir(a->target);
  else
    remove(a->tmp);
  *ok = good;
  return 1;
}

// Define this to implement "scheme" convention for specifying kit/manifest DB locations
// 2011-11-14 Moved scheme implementation hooks to FileStore.sedona
//#define IMPL_SCHEME_CONVENTION

// =========================================================================== //
#ifdef IMPL_SCHEME_CONVENTION

#include "stdlib.h"
#include "string.h"


char fullpath[1024];   // buf to hold complete path
char  schemestr[32];   // buf to hold scheme string

#define SCHEME_DELIM (':')


#define MAX_NUM_SCHEMES 2
const char* FSschemes[MAX_NUM_SCHEMES]     = { "m", "k", };
const char* FSschemePaths[MAX_NUM_SCHEMES] = { "manifests/", "kits/", };


// --------------------------------------------------------------------------- //
// FSexpandFilePath
//     - captures scheme name, if any, in ord & constructs full local path
// --------------------------------------------------------------------------- //
char* FSexpandFilePath( const char* ord )
{
  char *delim, *rem, *pathptr, *dash;
  int s;

  if (ord==NULL) return NULL;

  delim = strchr(ord, SCHEME_DELIM);

  // If no scheme, assume whole ord is file path
  if (delim==NULL) return (char*)ord;

  // Scheme found!
  rem = delim+1;

  // Copy scheme string into separate buf - should check for string length overrun eventually
  strncpy(schemestr, ord, delim-ord);
  schemestr[delim-ord] = '\0';    // make sure it gets null term.

  //
  // If scheme is in map then expand full path and copy into buf;
  //   otherwise just return remainder of ord after scheme delimiter 
  //
  for (s=0; s<MAX_NUM_SCHEMES; s++)
  {
    if (strcmp(FSschemes[s], schemestr)==0)
      break;
  }

  // Point to beginning of path buf
  pathptr = fullpath;

  // If we found our scheme...
  if (s<MAX_NUM_SCHEMES)
  {
    // Copy the path for this scheme
    strcpy(pathptr, FSschemePaths[s]);     // strcpy copies null char too
    pathptr += strlen(FSschemePaths[s]);   // ptr moves to char after path ends (i.e. null)

    // Next add the kit name (i.e. filename up to dash)
    dash = strchr(rem, '-');
    if (dash!=NULL)
    {
      strncpy(pathptr, rem, dash-rem);
      pathptr += dash-rem;
      *pathptr++ = '/';
    }
  }


  // Finally, copy filename itself
  strcpy(pathptr, rem);   

  // DIAG
  printf("\t** FS serving file: %s\n", fullpath);
  // DIAG

  return fullpath;
}

#endif
// =========================================================================== //


#ifdef _WIN32
#include <windows.h>
#endif

// int FileStore.doSize(Str name)
Cell sys_FileStore_doSize(SedonaVM* vm, Cell* params)
{
  // The FileStore API indicates that this native call should not
  // cause the file to be opened. We'll do our best.
 #ifdef IMPL_SCHEME_CONVENTION
  const char* name = FSexpandFilePath(params[0].aval);
 #else
  const char* name = params[0].aval;
 #endif

  Cell result;
#ifdef _WIN32
  BOOL fOk;
  WIN32_FILE_ATTRIBUTE_DATA fileInfo;

  if (name == NULL) return negOneCell;
  fOk = GetFileAttributesEx(name, GetFileExInfoStandard, (void*)&fileInfo);
  if (!fOk) return negOneCell;
  result.ival = fileInfo.nFileSizeLow;
#elif defined(_POSIX_SOURCE)
  struct stat statInfo;
  if (name == NULL || stat(name, &statInfo)) return negOneCell;
  result.ival = statInfo.st_size;
#else
  // Our best was not good enough. Open the file...
  FILE* fp;
  fp = fopen(name, "rb");
  if (fp == NULL) return negOneCell;

  // seek to end to get file size
  fseek(fp, 0, SEEK_END);
  result.ival = ftell(fp);
  fclose(fp);
#endif

  return result;
}

// Obj FileStore.doOpen(Str name, Str mode)
Cell sys_FileStore_doOpen(SedonaVM* vm, Cell* params)
{
 #ifdef IMPL_SCHEME_CONVENTION
  const char* name = FSexpandFilePath(params[0].aval);
 #else
  const char* name = params[0].aval;
 #endif

  const char* mode = params[1].aval;
  const char* fopenMode;
  Cell result;
  FILE* fp;

  // sanity check arguments
  if (name == NULL || mode == NULL) return nullCell;

  // sanity check mode
  if (mode[1] != '\0') return nullCell;
  switch (mode[0])
  {
    case 'm':
      // create file in case it doesn't exist yet
      fp = fopen(name, "a+b");
      if (fp == NULL) return nullCell;
      fclose(fp);
      fopenMode = "r+b";
      break;
    case 'r':
      fopenMode = "rb";
      break;
    case 'w':
      fopenMode = "wb";
      break;
    default:  return nullCell;
  }


  //
  // If opening for writing: Create directories if they don't exist
  //
  if ( (mode[0]=='m') || (mode[0]=='w') )
  {  
    char *nxtdir, sepch, *sep;

    // Find first path sep (either type) skipping leading sep if any
    sep = strchr(name, '/');
    if (sep==NULL) sep = strchr(name, '\\');

    // If so, step through dir string and try to create dir(s) 
    while (sep!=NULL) 
    {
      sepch = *sep;          // cache sep char
      *sep = '\0';           // replace sep with null term

      // create dir (should be NOP if dir exists)
#ifdef _WIN32
      _mkdir((const char*)name);
#else
      mkdir((const char*)name, S_IRWXU | S_IRWXG | S_IRWXO);
#endif

      *sep = sepch;          // restore sep char
      nxtdir = sep+1;        // starting point for next search

      // find next path sep
      sep = strchr(nxtdir, '/');
      if (sep==NULL) sep = strchr(nxtdir, '\\');
    }
  }

  if (mode[0] == 'w' && svmConfigAtomicName(name))
  {
    FILE* afp = atomicOpen(name);
    if (afp == (FILE*)-1) return nullCell;
    if (afp != NULL)
    {
      result.aval = afp;
      return result;
    }
  }

  result.aval = fopen(name, fopenMode);

  // DIAG
  if (result.aval==NULL)
    printf("fopen('%s', '%s') failed, errno=%d (%s)\n", name, fopenMode, errno, strerror(errno));
  // DIAG

  return result;
}




// int FileStore.doRead(Obj)
Cell sys_FileStore_doRead(SedonaVM* vm, Cell* params)
{
  FILE* fp  = (FILE*)params[0].aval;
  Cell result;

  // sanity check arguments. FileStore requires -1 return value on error/eof.
  if ((fp == NULL) || ((result.ival = fgetc(fp)) == EOF))
	  return negOneCell;
  else
    return result;
}

// int FileStore.doReadBytes(Obj, byte[], int, int)
Cell sys_FileStore_doReadBytes(SedonaVM* vm, Cell* params)
{
  FILE* fp  = (FILE*)params[0].aval;
  uint8_t* buf = (uint8_t*)params[1].aval;
  int32_t  off = params[2].ival;
  int32_t  len = params[3].ival;
  Cell result;

  // sanity check arguments. FileStream requires -1 on eof/error.
  if ((fp == NULL) || feof(fp) || ferror(fp)) return negOneCell;

  buf = buf + off;

  result.ival = fread(buf, 1, len, fp);
  return result;
}

// bool FileStore.doWrite(Obj, int)
Cell sys_FileStore_doWrite(SedonaVM* vm, Cell* params)
{
  FILE* fp  = (FILE*)params[0].aval;
  int32_t b = params[1].ival;
  int32_t r;

  // sanity check arguments
  if (fp == NULL ) return negOneCell;

  r = fputc(b, fp);

  return r == b ? trueCell : falseCell;
}

// bool FileStore.doWriteBytes(Obj, byte[], int, int)
Cell sys_FileStore_doWriteBytes(SedonaVM* vm, Cell* params)
{
  FILE* fp  = (FILE*)params[0].aval;
  uint8_t* buf = (uint8_t*)params[1].aval;
  int32_t  off = params[2].ival;
  int32_t  len = params[3].ival;
  int32_t  r;

  // sanity check arguments
  if (fp == NULL ) return negOneCell;

  buf = buf + off;

  r = fwrite(buf, 1, len, fp);

  return r == len ? trueCell : falseCell;
}

// int FileStore.doTell(Obj)
Cell sys_FileStore_doTell(SedonaVM* vm, Cell* params)
{
  FILE* fp = (FILE*)params[0].aval;
  Cell r;

  // sanity check arguments
  if (fp == NULL) return negOneCell;

  r.ival = ftell(fp);

  return r;
}

// bool FileStore.doSeek(Obj, int)
Cell sys_FileStore_doSeek(SedonaVM* vm, Cell* params)
{
  FILE* fp = (FILE*)params[0].aval;
  int32_t pos = params[1].ival;
  int32_t  r;

  // sanity check arguments
  if (fp == NULL) return negOneCell;

  r = fseek(fp, pos, SEEK_SET);

  return r == 0 ? trueCell : falseCell;
}

// void FileStore.doFlush(Obj)
Cell sys_FileStore_doFlush(SedonaVM* vm, Cell* params)
{
  FILE* fp = (FILE*)params[0].aval;

  // sanity check arguments
  if (fp == NULL) return negOneCell;

  fflush(fp);

  return nullCell;
}

// void FileStore.doFsync(Obj)
Cell sys_FileStore_doFsync(SedonaVM* vm, Cell* params)
{
  FILE* fp = (FILE*)params[0].aval;

  // sanity check arguments
  if (fp == NULL) return negOneCell;

  fflush(fp);

#ifndef _WIN32
  int ifd = fileno(fp);
  fsync(ifd);
#ifdef __linux__
  syncfs(ifd);
  sync();
#endif
#endif

  return nullCell;
}


// bool FileStore.doClose(Obj)
Cell sys_FileStore_doClose(SedonaVM* vm, Cell* params)
{
  FILE* fp = (FILE*)params[0].aval;

  // sanity check arguments
  if (fp == NULL) return negOneCell;

  {
    int ok;
    if (atomicClose(fp, &ok))
      return ok ? trueCell : falseCell;
  }

  if (fclose(fp) != 0)
  {
    printf("ERROR: Cannot close file\n");
    return falseCell;
  }

  return trueCell;
}


// save.keep: before "app.sab -> app.sab.bak" shift the existing backups
// app.sab.bak -> .bak.1 -> ... -> .bak.<keep-1>; the oldest one is dropped.
static void rotateBackups(const char* from, const char* to)
{
  char a[ATOMIC_NAME + 8], b[ATOMIC_NAME + 8];
  struct stat st;
  int i, keep = svmConfig.saveKeep;

  if (keep <= 1 || !endsWith(from, ".sab") || !endsWith(to, ".bak")) return;
  if (strlen(to) >= ATOMIC_NAME || stat(from, &st) != 0 || stat(to, &st) != 0) return;

  for (i = keep - 2; i >= 1; --i)
  {
    sprintf(a, "%s.%d", to, i);
    sprintf(b, "%s.%d", to, i + 1);
    if (stat(a, &st) == 0 && rename(a, b) != 0)
      printf("save.keep: cannot rename %s -> %s: %s\n", a, b, strerror(errno));
  }
  sprintf(b, "%s.1", to);
  // hard link keeps <to> in place, so a backup exists at every moment
  remove(b);
  if (link(to, b) != 0 && rename(to, b) != 0)
    printf("save.keep: cannot keep %s as %s: %s\n", to, b, strerror(errno));
  svmFsyncParentDir(to);
}

// static bool FileStore.rename(Str from, Str to)
Cell sys_FileStore_rename(SedonaVM* vm, Cell* params)
{
 #ifdef IMPL_SCHEME_CONVENTION
  const char* from = FSexpandFilePath(params[0].aval);
  const char* to   = FSexpandFilePath(params[1].aval);
 #else 
  const char* from = params[0].aval;
  const char* to   = params[1].aval;
 #endif 

  int r;
  struct stat statBuf;

  if (svmConfig.saveAtomic)
  {
    char why[128];
    // never let an invalid app file replace a good backup
    if (svmConfigAtomicName(from) && endsWith(from, ".sab") && stat(to, &statBuf) == 0 &&
        svmValidateSab(from, why, sizeof(why)) != 0)
    {
      printf("save.atomic: keeping %s, %s is invalid: %s\n", to, from, why);
      return falseCell;
    }
    rotateBackups(from, to);
#ifndef _WIN32
    // POSIX rename replaces the target atomically
    r = rename(from, to);
    if (r == 0) svmFsyncParentDir(to);
    return r == 0 ? trueCell : falseCell;
#endif
  }

  rotateBackups(from, to);
  if ((stat(to, &statBuf) == 0) && (remove(to) != 0))
    return falseCell;

  r = rename(from, to);
  return r == 0 ? trueCell : falseCell;
}


// static bool FileStore.remove(Str filename)
Cell sys_FileStore_remove(SedonaVM* vm, Cell* params)
{
 #ifdef IMPL_SCHEME_CONVENTION
  const char* filename = FSexpandFilePath(params[0].aval);
 #else
  const char* filename = params[0].aval;
 #endif

  Cell ret = trueCell;
  struct stat statBuf;

  if ((stat(filename, &statBuf) == 0) && (remove(filename) != 0))
     ret = falseCell;

  return ret;
}


