#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable: 4244 4245 4267 4310 4324 4334 4702 4996)
#endif

/* Standard libraries used by Lua core */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include all Lua core files as C++ */
#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "ldo.c"
#include "ltable.c"
#include "lstring.c"
#include "lapi.c"
#include "lvm.c"

/* Standard libraries */
#include "lauxlib.c"
#include "lbaselib.c"
#include "lcorolib.c"
#include "ldblib.c"
#include "lmathlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
#include "liolib.c"
#include "loadlib.c"
#include "linit.c"

#ifdef _MSC_VER
  #pragma warning(pop)
#endif
