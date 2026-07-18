PATCH_SOURCE_NAMES = {}
for i in range(0, 31):
    PATCH_SOURCE_NAMES[i] = f"MIDI CC{i + 1}"
for i in range(31, 118):
    PATCH_SOURCE_NAMES[i] = f"MIDI CC{i + 2}"
PATCH_SOURCE_NAMES.update({
    118: "Pitch Bend", 119: "Channel Pressure", 120: "Velocity", 121: "Last Note",
    122: "Low Note", 123: "High Note", 124: "Clock Commands", 125: "LFO",
    126: "LFO Sine", 127: "LFO Cosine", 128: "LFO Square", 129: "LFO Sawtooth",
    130: "LFO Pulse", 131: "LFO Triangle", 132: "Left Env Follower", 133: "Right Env Follower",
    134: "AR Env", 135: "Latch", 136: "Timeswitch 1", 137: "Timeswitch 2",
    138: "Composite Timeswitch", 139: "Mono InLvl", 140: "Left InLvl", 141: "Right InLvl",
    142: "FootPedal", 143: "Footswitch 1", 144: "Footswitch 2", 145: "ADJUST",
    146: "Tempo", 254: "On", 255: "Off",
})
for i in range(147, 254):
    PATCH_SOURCE_NAMES[i] = "Reserved"
