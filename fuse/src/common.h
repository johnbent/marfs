#ifndef _MARFS_COMMON_H
#define _MARFS_COMMON_H

/*
This file is part of MarFS, which is released under the BSD license.


Copyright (c) 2015, Los Alamos National Security (LANS), LLC
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-----
NOTE:
-----
MarFS uses libaws4c for Amazon S3 object communication. The original version
is at https://aws.amazon.com/code/Amazon-S3/2601 and under the LGPL license.
LANS, LLC added functionality to the original work. The original work plus
LANS, LLC contributions is found at https://github.com/jti-lanl/aws4c.

GNU licenses can be found at <http://www.gnu.org/licenses/>.


From Los Alamos National Security, LLC:
LA-CC-15-039

Copyright (c) 2015, Los Alamos National Security, LLC All rights reserved.
Copyright 2015. Los Alamos National Security, LLC. This software was produced
under U.S. Government contract DE-AC52-06NA25396 for Los Alamos National
Laboratory (LANL), which is operated by Los Alamos National Security, LLC for
the U.S. Department of Energy. The U.S. Government has rights to use,
reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR LOS
ALAMOS NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is
modified to produce derivative works, such modified software should be
clearly marked, so as not to confuse it with the version available from
LANL.

THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL SECURITY, LLC OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/



// Must come before anything else that might include <time.h>
#include "marfs_base.h"

#include "object_stream.h"      // FileHandle needs ObjectStream

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#  ifdef __cplusplus
extern "C" {
#  endif


typedef uint8_t  FuseContextFlagType;

typedef enum {
   PUSHED_USER  = 0x01,         // push_security user already ran
} FuseContextFlags;

// install this into fuse_get_context()->private_data?
typedef struct {
   FuseContextFlagType  flags;
   uid_t                user;   // if (flags & PUSHED_USER)
} MarFS_FuseContextInfo;



// Human-readable argument to functions with an <is_interactive> parameter
typedef enum {
   MARFS_BATCH       = 0,
   MARFS_INTERACTIVE = 1
} MarFS_Interactivity;


// // Variation on the MarFS_Interactivity
// // Human-readable argument to functions with an <use_iperms> parameter
// typedef enum {
//    B_PERMS      = 0,
//    I_PERMS      = 1
// } MarFS_PermSelect;



// ---------------------------------------------------------------------------
// TRY, etc
//
// Macro-wrappers around common functions allow the fuse code to be a lot
// cleaner.  The return value from every function-call has to be tested,
// and perhaps return an error-code if things aren't right.  These hide the
// test-and-return.
// ---------------------------------------------------------------------------

// Override this, if you have some fuse-handler that wants to do
// something special before any exit.  (See e.g. marfs_open)
#define RETURN(VALUE)  return(VALUE)



// TRY() macros test for return-values, and skip out early if they get a
//       non-zero.  The "do { ... } while()" just makes sure your macro
//       statements will still work correctly inside single-statment
//       conditions, like this:
//
//       if (...)
//         TRY(...);
//       else
//         TRY(...);
//
// NOTE: rc is lexically-scoped.  It's defined at the top of fuse functions
//       via PUSH_USER().  This allows us to use TRY_GE0() on functions
//       whose return value we care about.
//
// NOTE: TRY macros also invert the sign of the return value, as needed for
//       fuse.  This means they shouldn't be used within common functions,
//       which may in turn be wrapped inside TRY() by fuse routines.
//       [see __TRY0()]


#define TRY0(FUNCTION, ...)                                             \
   do {                                                                 \
      /* LOG(LOG_INFO, "TRY0(%s)\n", #FUNCTION); */                     \
      rc = (size_t)FUNCTION(__VA_ARGS__);                               \
      if (rc) {                                                         \
         LOG(LOG_INFO, "ERR TRY0(%s) returning (%ld) '%s'\n\n",         \
             #FUNCTION, rc, strerror(errno));                           \
         /* RETURN(-rc); */ /* negated for FUSE */                      \
         RETURN(-errno); /* negated for FUSE */                         \
      }                                                                 \
   } while (0)

// e.g. open() returns -1 or an fd.
#define TRY_GE0(FUNCTION, ...)                                          \
   do {                                                                 \
      /* LOG(LOG_INFO, "TRY_GE0(%s)\n", #FUNCTION); */                  \
      rc_ssize = (ssize_t)FUNCTION(__VA_ARGS__);                        \
      if (rc_ssize < 0) {                                               \
         LOG(LOG_INFO, "ERR GE0(%s) returning (%d) '%s'\n\n",           \
             #FUNCTION, errno, strerror(errno));                        \
         RETURN(-errno); /* negated for FUSE */                         \
      }                                                                 \
   } while (0)




// FOR INTERNAL USE ONLY.  (Not for calling directly from fuse routines)
// This version doesn't invert the value of the return-code
#define __TRY0(FUNCTION, ...)                                           \
   do {                                                                 \
      LOG(LOG_INFO, "__TRY0(%s)\n", #FUNCTION);                         \
      rc = (size_t)FUNCTION(__VA_ARGS__);                               \
      if (rc) {                                                         \
         LOG(LOG_INFO, "ERR __TRY0(%s) returning (%ld) '%s'\n\n",       \
             #FUNCTION, rc, strerror(errno));                           \
         /* RETURN(rc);*/ /* NOT negated! */                            \
         RETURN(errno);                                                 \
      }                                                                 \
   } while (0)

#define __TRY_GE0(FUNCTION, ...)                                        \
   do {                                                                 \
      LOG(LOG_INFO, "__TRY_GE0(%s)\n", #FUNCTION);                      \
      rc_ssize = (ssize_t)FUNCTION(__VA_ARGS__);                        \
      if (rc_ssize < 0) {                                               \
         LOG(LOG_INFO, "ERR __TRY_GE0(%s) returning (%ld) '%s'\n\n",    \
             #FUNCTION, rc_ssize, strerror(errno));                     \
         /* RETURN(rc);*/ /* NOT negated! */                            \
         RETURN(errno);                                                 \
      }                                                                 \
   } while (0)




// add log items when you enter/exit a function
#define ENTRY()                                                         \
   LOG(LOG_INFO, "entry\n");                                            \
   __attribute__ ((unused)) size_t   rc = 0;                            \
   __attribute__ ((unused)) ssize_t  rc_ssize = 0

#define EXIT()                                  \
   LOG(LOG_INFO, "exit\n");                     \
   LOG(LOG_INFO, "\n")



#define PUSH_USER()                                                     \
   ENTRY();                                                             \
   uid_t saved_euid = -1;                                               \
   TRY0(push_user, &saved_euid)

#define POP_USER()                                                      \
   EXIT();                                                              \
   TRY0(pop_user, &saved_euid);                                         \




#define EXPAND_PATH_INFO(INFO, PATH)   TRY0(expand_path_info, (INFO), (PATH))
#define TRASH_UNLINK(INFO, PATH)       TRY0(trash_unlink,     (INFO), (PATH))
#define TRASH_TRUNCATE(INFO, PATH)     TRY0(trash_truncate,   (INFO), (PATH))
#define TRASH_NAME(INFO, PATH)         TRY0(trash_name,       (INFO), (PATH))

#define STAT_XATTRS(INFO)              TRY0(stat_xattrs, (INFO))
#define STAT(INFO)                     TRY0(stat_regular, (INFO))

#define SAVE_XATTRS(INFO, MASK)        TRY0(save_xattrs, (INFO), (MASK))



// return an error, if all the required permission-flags are not asserted
// in the iperms or bperms of the given NS.
#define CHECK_PERMS(ACTUAL_PERMS, REQUIRED_PERMS)                       \
   do {                                                                 \
      LOG(LOG_INFO, "check_perms req:%08x actual:%08x\n", (REQUIRED_PERMS), (ACTUAL_PERMS)); \
      if (((ACTUAL_PERMS) & (REQUIRED_PERMS)) != (REQUIRED_PERMS))      \
         return -EACCES;   /* should be EPERM? (i.e. being root wouldn't help) */ \
   } while (0)

#define ACCESS(PATH, PERMS)            TRY0(access, (PATH), (PERMS))
#define CHECK_QUOTAS(INFO)             TRY0(check_quotas, (INFO))



// ---------------------------------------------------------------------------
// xattrs
// ---------------------------------------------------------------------------


// These describe xattr keys, and the type of the corresponding values, for
// all the metadata fields in a MarFS_ReservedXattr.  These support a
// generic parser for extracting and parsing xattr data from a metadata
// file (or maybe also from object metadata).
//
// As they are found in stat_xattrs(), each flag is OR'ed into a counter,
// so that has_any_xattrs() can tell you whether specific xattrs were
// found.
//
// NOTE: co-maintain XattrMaskType, ALL_MARFS_XATTRS, MARFS_MD_XATTRS


typedef uint8_t XattrMaskType;  // OR'ed XattrValueTypes

typedef enum {
   XVT_NONE       = 0,          // marks the end of <xattr_specs>

   XVT_PRE        = 0x01,
   XVT_POST       = 0x02,
   XVT_RESTART    = 0x04,
   XVT_SLAVE      = 0x08,
   
} XattrValueType;

#define MARFS_MD_XATTRS   (XVT_PRE | XVT_POST)     /* MD-related XattrValueTypes */
#define MARFS_ALL_XATTRS  (XVT_PRE | XVT_POST | XVT_RESTART | XVT_SLAVE)  /* all XattrValueTypes */





// generic description of one of our reserved xattrs
typedef struct {
   XattrValueType  value_type;
   const char*     key_name;        // does not incl MarFS_XattrPrefix (?)
} XattrSpec;


/// typdef struct MarFS_XattrList {
///   char*                   name;
///   char*                   value;
///   struct MarFS_XattrList* next;
/// } MarFS_XattrList;

// An array of XattrSpecs.  Last one has value_type == XVT_NONE.
// initialized in init_xattr_specs()
extern XattrSpec*  MarFS_xattr_specs;





// ---------------------------------------------------------------------------
// PathInfo
//
// used to accumulate FUSE-support information.
// see expand_path_info(), and stat_xattrs()
//
// NOTE: stat_xattrs() sets the flags in PathInfo.xattrs, for each
//       corresponding xattr that is found.  Use has_any_xattrs() or
//       has_all_xattrs(), with a mask, to check for presence of xattrs you
//       care about, after stat_xattr().  The <mask> is just
//       XattrValueTypes, OR'ed together.
//
//       Because this is C, and not C++, I'm not tracking which xattrs you
//       might be changing and updating.  However, when you call
//       save_xattrs(), you can provide another mask (i.e. just like
//       has_any_xattrs()), and we'll install all the xattrs that your mask
//       selects.  (You could use this to write different xattrs at
//       different times, or to update a specific one several times.)
//
// ---------------------------------------------------------------------------


typedef uint8_t  PathInfoFlagType;
typedef enum {
   PI_RESTART      = 0x01,      // file is incomplete (see stat_xattrs())
   PI_EXPANDED     = 0x02,      // expand_path_info() was called?
   PI_STAT_QUERY   = 0x04,      // i.e. maybe PathInfo.st empty for a reason
   PI_XATTR_QUERY  = 0x08,      // i.e. maybe PathInfo.xattr empty for a reason
   // PI_XATTRS       = 0x08,      // XATTR_QUERY found any MarFS xattrs
   PI_PRE_INIT     = 0x10,      // "pre"  field has been initialized from scratch (unused?)
   PI_POST_INIT    = 0x20,      // "post" field has been initialized from scratch (unused?)
   PI_TRASH_PATH   = 0x80,      // expand_trash_info() was called?
   //   PI_STATVFS      = 0x80,      // stvfs has been initialized from Namespace.fsinfo?
} PathInfoFlagValue;


typedef struct PathInfo {
   MarFS_Namespace*     ns;
   struct stat          st;
   // struct statvfs       stvfs;  // applies to Namespace.fsinfo

   MarFS_XattrPre       pre;
   MarFS_XattrPost      post;
   MarFS_XattrSlave     slave;
   XattrMaskType        xattrs; // OR'ed XattrValueTypes, use has_any_xattrs()

   PathInfoFlagType     flags;

   char                 md_path[MARFS_MAX_MD_PATH]; // full path to MDFS file
   char                 trash_path[MARFS_MAX_MD_PATH];
} PathInfo;



// ...........................................................................
// FileHandle
//
// Fuse open() dynamically-allocates one of these, and stores it in
// fuse_file_info.fh. The FUSE impl gives us state that may be accessed
// across multiple callbacks.  For example, marfs_open() might save info
// needed by marfs_write().
// ...........................................................................

typedef enum {
   FH_READING    = 0x01,        // might someday allow O_RDWR
   FH_WRITING    = 0x02,        // might someday allow O_RDWR
   FH_DIRECT     = 0x04,        // i.e. PathInfo.xattrs has no MD_
} FHFlags;

typedef uint16_t FHFlagType;


// read() can maintain state here
typedef struct {
   // TBD ...
} ReadStatus;


// write() can maintain state here
typedef struct {
   // TBD ...
   RecoveryInfo  rec_info;      // (goes into tail of object)
} WriteStatus;



typedef struct {
   PathInfo      info;          // includes xattrs, MDFS path, etc
   int           md_fd;         // opened for reading meta-data, or data
   FHFlagType    flags;
   ReadStatus    read_status;   // buffer_management, current_offset, etc
   WriteStatus   write_status;  // buffer-management, etc
   ObjectStream  os;            // handle for streaming access to objects
} MarFS_FileHandle;





// // In C++ I'd just steal the templated Pool<T> classes from pftool, to
// // allow us to re-use dynamically-allocate objects.  Here, I'll make a
// // crude stack and functions to alloc/free.
// //
// // Hmmm.  That's going to lead to a locking bottleneck.  Maybe we don't
// // care, in fuse?
// 
// typedef struct Reusable {
//    void*            obj;
//    int              avail;
//    struct Reusable* next;
// } Reusable;
// 
// extern void* alloc_reusable(Reusable** ruse);
// extern void  free_reusable (Reusable** ruse, void* obj);





extern int  push_user();
extern int  pop_user();

// These initialize different parts of the PathInfo struct.
// Calling them redundantly is cheap and harmless.
extern int  expand_path_info (PathInfo* info, const char* path);
extern int  expand_trash_info(PathInfo* info, const char* path);

extern int  stat_regular     (PathInfo* info);
extern int  stat_xattrs      (PathInfo* info);
extern int  save_xattrs      (PathInfo* info, XattrMaskType mask);

extern int  md_exists        (PathInfo* info);

// initialize MarFS_xattr_specs
extern int  init_xattr_specs();

//extern int  has_marfs_xattrs (PathInfo* info);
extern int  has_all_xattrs (PathInfo* info, XattrMaskType mask);
extern int  has_any_xattrs (PathInfo* info, XattrMaskType mask);

extern int  trunc_xattr  (PathInfo* info);

// need the path to initialize info->md_trash_path
extern int  trash_unlink  (PathInfo* info, const char* path);
extern int  trash_truncate(PathInfo* info, const char* path);
extern int  trash_name    (PathInfo* info, const char* path);

extern int  check_quotas  (PathInfo* info);





#  ifdef __cplusplus
}
#  endif


#endif  // _MARFS_COMMON_H
