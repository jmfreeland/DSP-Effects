#pragma once

#include "ChamberAdapter.h"
#include "ConcertHallAdapter.h"
#include "EngineAdapter.h"
#include "InfiniteAdapter.h"
#include "InverseAdapter.h"
#include "PlateAdapter.h"

#include <algorithm>
#include <array>
#include <memory>

// The list of algorithms the Loom browser plugin offers - a proof-of-
// concept slice covering the five Lexicon reverb cores (see CLAUDE.md's
// "Future direction" note on a single browsable Loom plugin). Adding an
// algorithm means adding its adapter class and one factory line here;
// everything else (parameter registration, working-buffer sizing, the
// picker, panel/diagram wiring) is generic over this list.
namespace loom::browser
{
using AdapterFactory = std::unique_ptr<EngineAdapter> (*)();

struct RegistryEntry
{
    AdapterFactory create;
};

inline constexpr std::array<RegistryEntry, 5> kEngineRegistry {{
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<ConcertHallAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<PlateAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<ChamberAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<InfiniteAdapter>()); } },
    { [] { return std::unique_ptr<EngineAdapter>(std::make_unique<InverseAdapter>()); } },
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
