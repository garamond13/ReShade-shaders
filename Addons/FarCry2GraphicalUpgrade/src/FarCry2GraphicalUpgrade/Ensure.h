#pragma once

#ifdef __cplusplus
#include <cassert>
#else
#include <assert.h>
#endif

#ifdef NDEBUG
#define ensure(always_keep, discard_if_ndebug) always_keep
#else
#define ensure(always_keep, discard_if_ndebug) (assert(always_keep discard_if_ndebug))
#endif