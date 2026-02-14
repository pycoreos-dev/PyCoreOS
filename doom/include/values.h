/* Shim for <values.h> — provides MAXINT etc. */
#ifndef PCOS_VALUES_SHIM_H
#define PCOS_VALUES_SHIM_H

#ifndef MAXINT
#define MAXCHAR  ((char)0x7f)
#define MAXSHORT ((short)0x7fff)
#define MAXINT   ((int)0x7fffffff)
#define MAXLONG  ((long)0x7fffffff)
#define MINCHAR  ((char)0x80)
#define MINSHORT ((short)0x8000)
#define MININT   ((int)0x80000000)
#define MINLONG  ((long)0x80000000)
#endif

#endif
