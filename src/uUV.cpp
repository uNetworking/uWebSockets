#ifdef USE_MICRO_UV

#include "uUV.h"

uv_loop_t *loops[128];
int loopHead;

uv_poll_cb callbacks[128];
int cbHead;

#endif
