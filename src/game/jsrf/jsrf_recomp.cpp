#include "rejsrf/jsrf_profile.h"

#if defined(REJSRF_HAS_RECOMP)
extern "C" rejsrf::RecompiledFunction recomp_lookup(uint32_t xbox_va);
extern "C" int recomp_dispatch_init(void);
#endif

namespace rejsrf
{

RecompiledLookup RecompiledLookupFunction()
{
#if defined(REJSRF_HAS_RECOMP)
    return &recomp_lookup;
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
