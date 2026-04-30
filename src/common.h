#ifndef ZEN_COMMON_H
#define ZEN_COMMON_H

/*
** common.h — Tipos base e configuração.
** Zero dependências STL. Só <cstdint>, <cstddef>, <cstdio>, <cstring>.
*/

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace zen {

/* Configuração — pode ajustar */
constexpr int    kMaxRegs        = 250;   /* registos por frame (Lua usa 249) */
constexpr int    kMaxFrames      = 128;   /* profundidade de calls */
constexpr int    kMaxConstants   = 65536; /* pool de constantes por função */
constexpr int    MAX_GLOBALS     = 512;   /* globals máximos */
constexpr size_t kGCInitThreshold = 1024 * 256;  /* 256KB antes do 1º GC */
constexpr float  kGCGrowFactor   = 2.0f;

/* Forward declarations */
struct VM;
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
