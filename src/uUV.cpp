#ifdef USE_MICRO_UV

#include "uUV.h"

uv_loop_t *loops[128];
int loopHead;

// support 128 callbacks maximum
uv_poll_cb callbacks[128];
int cbHead;

int polls;
std::vector<std::pair<uv_handle_t *, uv_handle_cb>> closing;

#endif
