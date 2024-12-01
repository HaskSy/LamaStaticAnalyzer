/**
 * @file Runtime.h
 * @brief This file adds definitions for runtime functions that are not defined
 * as public or essential for runtime
 *
 */
#pragma once

#include <cstddef>

extern "C" {
// NOLINTBEGIN
#include "runtime_common.h"
#include "gc.h"

void* Belem(void* p, int i);
void* Bsta(void* v, int i, void* x);
void* Bstring(void* p);
int   Llength(void* p);
int   Lread();
int   LtagHash(char*);
int   Btag(void* d, int t, int n);
void* Lstring(void* p);
int   Bstring_patt(void* x, void* y);
int   Bstring_tag_patt(void* x);
int   Barray_tag_patt(void* x);
int   Bsexp_tag_patt(void* x);
int   Bboxed_patt(void* x);
int   Bunboxed_patt(void* x);
int   Bclosure_tag_patt(void* x);
int   Barray_patt(void* d, int n);

// NOTE(zelourses): There is no single note about insides of Lama GC and _how_ the must work. Because
//  these two variable are having `size_t` type in source code. BUT. Inside gc functions it casts to:
// - `size_t *`
// - `void *`
// - `void **`
//
// How? Why? Is that's an UB? I think yes, it's an UB. But, we believe that people on Lama runtime and GC side
// know what they are doing and can prove that these casts are essential
// see [`Stack`] where they are initialized
extern size_t *__gc_stack_top, *__gc_stack_bottom;

// init function for lama gc
void __init();

// NOLINTEND
}
