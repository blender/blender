#include <string>

#define BRUSH_CHANNEL_MAKE_NAMES

#ifdef BRUSH_CHANNEL_DEFINE_TYPES
#  undef BRUSH_CHANNEL_DEFINE_TYPES
#endif

static std::basic_string<char> brush_channel_idnames[] = {
#include "intern/brush_channel_define.h"
};
