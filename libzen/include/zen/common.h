#ifndef ZEN_COMMON_H
#define ZEN_COMMON_H

/*
** common.h — Base types and VM configuration.
*/

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace zen
{

    /* VM limits */
    constexpr int kMaxRegs = 250;                   /* registers per call frame */
    constexpr int kMaxFrames = 256;                 /* max call depth */
    constexpr int kMaxFiberDepth = 64;              /* max nested fiber resumes (C stack) */
    constexpr int kMaxConstants = 65536;            /* constant pool per function */
    constexpr int MAX_GLOBALS = 1024;               /* global variable slots */
    constexpr size_t kGCInitThreshold = 1024 * 256; /* first GC at 256 KB */
    constexpr float kGCGrowFactor = 2.0f;

    /* Forward declarations */
    class VM;
    struct Obj;
    struct ObjString;
    struct ObjFunc;
    struct ObjUpvalue;
    struct ObjClosure;
    struct ObjFiber;
    struct ObjArray;
    struct ObjMap;
    struct ObjSet;
    struct ObjStructDef;
    struct ObjStruct;
    struct ObjClass;
    struct ObjInstance;
    struct ObjProcess;

} /* namespace zen */

#endif /* ZEN_COMMON_H */
