// Platform contract: a sampling profiler for one thread (diagnostic).
//
// A helper thread stops the sampled thread about every millisecond, reads its
// instruction pointer and lets it run again. Outside the executable (system
// and driver DLLs) it also takes the first stack word that points into the
// executable's code, as a guess at the caller there. The result is a
// histogram of code addresses by module, which scripts/perf/resolve_samples.py
// turns into function names with the linker map.

#ifndef REXBOX_PLATFORM_SAMPLER_H
#define REXBOX_PLATFORM_SAMPLER_H

#include <cstdint>

namespace rexbox::platform
{

// Starts sampling the calling thread. False when sampling already runs or
// the platform has no sampler.
bool StartThreadSampling();

// Stops sampling and writes the histogram to `path`: a header line with the
// sample count, then "<module> <offset in module, hex> <samples>
// [<caller's offset in the executable, hex>]" per address, most samples
// first. Returns the number of samples (0 when no
// sampling ran or the file could not be written).
uint64_t StopThreadSampling(const char* path);

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_SAMPLER_H
