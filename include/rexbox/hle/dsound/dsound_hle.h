// HLE module: DirectSound (DSOUND): voices and streams implemented on a host audio mixer.
//
// The library is statically linked into each title, so its functions are bound
// by symbol name to title-specific guest addresses through a title profile
// (see rexbox/runtime/hle_registry.h).

#ifndef REXBOX_HLE_DSOUND_DSOUND_HLE_H
#define REXBOX_HLE_DSOUND_DSOUND_HLE_H

namespace rexbox::runtime
{
class HleRegistry;
}

namespace rexbox::hle::dsound
{

void RegisterDsoundHle(runtime::HleRegistry& registry);

}  // namespace rexbox::hle::dsound

#endif  // REXBOX_HLE_DSOUND_DSOUND_HLE_H
