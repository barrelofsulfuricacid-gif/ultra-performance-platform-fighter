#ifndef PF_PROFILE_H
#define PF_PROFILE_H

#ifndef PF_TRACY_ENABLED
#define PF_TRACY_ENABLED 0
#endif

#if PF_TRACY_ENABLED

#include <tracy/TracyC.h>

#define PF_PROFILE_ZONE_BEGIN(context, label) \
    TracyCZoneN(context, label, 1)
#define PF_PROFILE_ZONE_TEXT(context, text, length) \
    TracyCZoneName(context, text, length)
#define PF_PROFILE_ZONE_END(context) TracyCZoneEnd(context)
#define PF_PROFILE_FRAME_MARK() TracyCFrameMark

#else

#define PF_PROFILE_ZONE_BEGIN(context, label) ((void)(label))
#define PF_PROFILE_ZONE_TEXT(context, text, length) \
    ((void)(text), (void)(length))
#define PF_PROFILE_ZONE_END(context) ((void)0)
#define PF_PROFILE_FRAME_MARK() ((void)0)

#endif

#endif
