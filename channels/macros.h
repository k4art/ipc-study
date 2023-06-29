#ifndef IPC_MACROS_H
#define IPC_MACROS_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define IPC_CONCAT_(a_, b_)  a_##b_
#define IPC_CONCAT(a_, b_)   IPC_CONCAT_(a_, b_)
#define IPC_MACRO_VAR(name_) IPC_CONCAT(name_, __LINE__)

#define IPC_DEFER(start_, end_)              \
  for (int IPC_MACRO_VAR(_i_) = (start_, 0); \
       !IPC_MACRO_VAR(_i_);                  \
       (IPC_MACRO_VAR(_i_) += 1, end_))

#ifndef NDEBUG
#define IPC_EOK(expr) do { int ret = (expr); if (ret != 0) { printf("error[%s]: %s\n", #expr, strerror(ret)); abort(); }  } while (0)
#else
#define IPC_EOK(expr) (void) (expr)
#endif

#endif
