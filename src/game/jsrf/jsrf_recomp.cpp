#include "rejsrf/jsrf_profile.h"

#if defined(REJSRF_HAS_RECOMP)
extern "C" void xbe_entry_point(void);
extern "C" int recomp_dispatch_init(void);
#endif

namespace rejsrf
{

RecompiledFunction RecompiledEntry()
{
#if defined(REJSRF_HAS_RECOMP)
    return &xbe_entry_point;
#else
    return nullptr;
#endif
}

bool InitializeRecompiledDispatch()
{
#if defined(REJSRF_HAS_RECOMP)
    return recomp_dispatch_init() != 0;
#else
    return false;
#endif
}

}  // namespace rejsrf
