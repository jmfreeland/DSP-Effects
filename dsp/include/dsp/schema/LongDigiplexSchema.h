#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& longDigiplexSchema()
{
    static const Stage stages[] = {
        { "input", "Left Input", StageKind::kInput, "Right In not used - see doc comment" },
        { "delay", "Delay", StageKind::kFeedback, "0-1.4s, Glide-smoothed changes" },
        { "output", "Output L/R", StageKind::kOutput, "Same delayed signal sent to both channels" },
    };
    static const Connection connections[] = {
        { "input", "delay", nullptr },
        { "delay", "output", nullptr },
        { "delay", "delay", "* Feedback" },
    };
    static const AlgorithmSchema schema = {
        "Long Digiplex",
        "The simplest H3000 algorithm here: one long delay line (up to 1.4s) recirculating through "
        "its own feedback, with the same delayed signal sent to both output channels.",
        stages, connections
    };
    return schema;
}
}
