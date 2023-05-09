#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "printf.h"

#define ASSERT(CONDITION)                                       \
        if (CONDITION) { } else {                               \
                panic ("assertion `"#CONDITION"' failed.");   \
        }

#define NOT_REACHED() panic ("executed an unreachable statement");

#define UNUSED __attribute__ ((unused))
#define NO_RETURN __attribute__ ((noreturn))
#define NO_INLINE __attribute__ ((noinline))

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *) 0)->MEMBER)

#endif