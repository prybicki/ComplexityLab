// gcc-array-bounds-fp.h — workaround for a GCC <= 13 -Warray-bounds false positive.
//
// GCC's middle-end miswarns on libstdc++'s std::copy fast path
// (bits/stl_algobase.h: __builtin_memmove of small trivially-copyable ranges).
// It is heavily instantiated by Vulkan-Hpp's uint32 ArrayWrapper copies, so it
// fires across every TU that touches the Vulkan-Hpp API — but it is a false
// positive, not a real out-of-bounds access. Marking the Vulkan headers as
// system does not help: -Warray-bounds is a mid-end warning emitted during
// optimization, after system-header tracking no longer applies.
//
// This header is force-included (-include) as the very first thing in every TU
// so that <algorithm> — and thus the warned std::copy template — is parsed
// inside the diagnostic-ignored region below. The pop immediately restores
// -Warray-bounds, so our own code keeps full coverage for direct array
// accesses; only the std::copy/memmove code path (the FP-prone one) is silenced.
//
// Scoped to GCC < 14 in cmake/Dependencies.cmake; the fix landed in GCC 14, so
// this self-removes once the toolchain is upgraded.

// The same memmove is miswarned under three sibling mid-end flags
// (-Warray-bounds / -Wstringop-overflow / -Wstringop-overread); suppress one and
// GCC emits the next. Ignore all three for this parse only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wstringop-overread"
#include <algorithm>
#pragma GCC diagnostic pop
