#pragma once

#include "BandDelayAdapter.h"
#include "ChamberAdapter.h"
#include "ChorusRvbAdapter.h"
#include "ConcertHallAdapter.h"
#include "DenseRoomAdapter.h"
#include "DiatonicShiftAdapter.h"
#include "DualChmbAdapter.h"
#include "DualDigiplexAdapter.h"
#include "DualInvAdapter.h"
#include "DualPltAdapter.h"
#include "DualShiftAdapter.h"
#include "EngineAdapter.h"
#include "GlideHallAdapter.h"
#include "InfiniteAdapter.h"
#include "InverseAdapter.h"
#include "LayeredShiftAdapter.h"
#include "LongDigiplexAdapter.h"
#include "MBandRvbAdapter.h"
#include "MultiShiftAdapter.h"
#include "PatchFactoryAdapter.h"
#include "PitchCorrectAdapter.h"
#include "PlateAdapter.h"
#include "QuadHallAdapter.h"
#include "Res1PlateAdapter.h"
#include "Res2PlateAdapter.h"
#include "ReverbFactoryAdapter.h"
#include "ReverseShiftAdapter.h"
#include "StereoChmbAdapter.h"
#include "StereoShiftAdapter.h"
#include "StutterAdapter.h"
#include "SweptCombsAdapter.h"
#include "SweptReverbAdapter.h"
#include "TimesqueezeAdapter.h"
#include "UltraTapAdapter.h"
#include "VSOChmbAdapter.h"
#include "VocoderAdapter.h"

#include <algorithm>
#include <array>
#include <memory>

// The list of algorithms the Loom browser plugin offers - all 17
// Lexicon PCM81 algorithms plus the Eventide H3000 algorithms as they're
// ported (see CLAUDE.md's "Future direction" note on a single browsable
// Loom plugin). Adding an algorithm means adding its adapter class and
// one factory line here; everything else (parameter registration,
// working-buffer sizing, the picker, panel/diagram wiring) is generic
// over this list.
namespace loom::browser
{
using AdapterFactory = std::unique_ptr<EngineAdapter> (*)();

struct RegistryEntry
{
    AdapterFactory create;
};

inline constexpr std::array<RegistryEntry, 35> kEngineRegistry {{
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<ConcertHallAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<PlateAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<ChamberAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<InfiniteAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<InverseAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<GlideHallAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<QuadHallAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<ChorusRvbAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<MBandRvbAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<Res1PlateAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<Res2PlateAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<DualChmbAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<DualPltAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<DualInvAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<StereoChmbAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<VSOChmbAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<PitchCorrectAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<DiatonicShiftAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<LayeredShiftAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<DualShiftAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<StereoShiftAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<ReverseShiftAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<SweptCombsAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<SweptReverbAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<ReverbFactoryAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<UltraTapAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<LongDigiplexAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<DualDigiplexAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<PatchFactoryAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<StutterAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<TimesqueezeAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<DenseRoomAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<VocoderAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<MultiShiftAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<BandDelayAdapter>()); } },
}};

inline int engineRegistrySize() { return static_cast<int>(kEngineRegistry.size()); }

inline std::unique_ptr<EngineAdapter> createAdapter(int index)
{
    return kEngineRegistry[static_cast<std::size_t>(index)].create();
}

// The largest working buffer any registered algorithm needs - the
// browser allocates once, this size, and reuses it across switches
// rather than sizing for the sum of all algorithms at once (only one
// engine is ever live).
inline std::size_t maxRequiredWorkingBufferSize()
{
    std::size_t maxSize = 0;
    for (auto& entry : kEngineRegistry)
    {
        auto adapter = entry.create();
        maxSize = std::max(maxSize, adapter->requiredWorkingBufferSize());
    }
    return maxSize;
}
}
