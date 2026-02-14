/* Shim for <alloca.h> — alloca is just a wrapper around __builtin_alloca */
#ifndef PCOS_ALLOCA_SHIM_H
#define PCOS_ALLOCA_SHIM_H
#define alloca(size) __builtin_alloca(size)
#endif
