"""Range Decode functions 0-51, transcribed from the PCM80 MIDI
Implementation Details manual, "Unpatchable and Patchable Parameter
Range Decode" section (pages 49-54)."""


def db_list(lo, hi, step=1, off_label="Off"):
    vals = []
    v = lo
    while v <= hi + 1e-9:
        vals.append(round(v, 2))
        v += step
    return vals


def rd0(v):
    return f"(null:{v})"


def rd1(v):
    return f"{v + 40} BPM"


def rd2(v):
    return f"Threshold {v}"


PATCH_SOURCES = None  # filled in by patch_sources.py


def rd3(v):
    from .patch_sources import PATCH_SOURCE_NAMES
    return PATCH_SOURCE_NAMES.get(v, f"Src{v}")


TAP_DURATIONS = ["1/8", "1/7", "1/6", "1/5", "1/4", "1/3", "1/2", "1", "2", "3", "4", "5", "6", "7", "8"]


def rd4(v):
    return TAP_DURATIONS[v] if 0 <= v < len(TAP_DURATIONS) else str(v)


BEAT_VALUES = ["Eighth", "Dotted Eighth", "Quarter", "Dotted Quarter", "Half", "Dotted Half", "Whole"]


def rd5(v):
    return BEAT_VALUES[v] if 0 <= v < len(BEAT_VALUES) else str(v)


TAP_AVERAGES = [2, 3, 4, 5, 6, 7, 8]


def rd6(v):
    return str(TAP_AVERAGES[v]) if 0 <= v < len(TAP_AVERAGES) else str(v)


def rd7(v):
    return f"Low Limit: {v}"


def rd8(v):
    return f"High Limit: {v}"


def rd9(v):
    return f"{v}%"


# Range Decode 10: exact 81-value list transcribed from the manual
# (page 49) - irregular for the first few entries, then regular -1db
# steps from -62db to +12db.
FX_ADJUST_VALUES = (
    ["Off", "-73db", "-69db", "-67db", "-65db", "-63db"]
    + [f"{d}db" for d in range(-62, 0)]
    + ["+0db"]
    + [f"+{d}db" for d in range(1, 13)]
)


def rd10(v):
    return FX_ADJUST_VALUES[v] if 0 <= v < len(FX_ADJUST_VALUES) else str(v)


# Range Decode 11: "161 level and phase assignments" (page 50) - a
# phase-inverted half, then Off, then a phase-normal half. The manual's
# scanned table has some OCR-ambiguous irregular entries near each
# half's start; approximated here as a roughly-linear -85..+0db sweep
# per half (anchored on the confidently-read endpoints), which is close
# but not guaranteed byte-exact to Lexicon's own irregular steps.
def rd11(v):
    if v == 80:
        return "Off"
    if v < 80:
        db = round(-80 * (v / 79))
        return f"{db:+d}db (phase inv)"
    idx = v - 81
    db = round(-85 + 85 * (idx / 79))
    return f"{db:+d}db"


def rd12(v):
    if v == 50:
        return "C"
    if v < 50:
        return f"L{50 - v}"
    return f"R{v - 50}"


CROSSOVER_FREQS_13 = [20, 21, 22, 23, 25, 26, 27, 28, 31, 32, 35, 37, 40, 42, 45, 47, 50, 52, 56, 58, 62, 66, 70, 74,
                      80, 84, 90, 94, 100, 105, 110, 115, 125, 130, 140, 150, 160, 170, 180, 190, 200, 210, 225, 235,
                      250, 265, 285, 300, 320, 340, 360, 380, 400, 425, 450, 475, 500, 525, 550, 600, 625, 675, 700,
                      750, 800, 850, 900, 950, 1000, 1050, 1100, 1200, 1250, 1350, 1400, 1500, 1600, 1700, 1800, 1900,
                      2000, 2150, 2250, 2400, 2550, 2700, 2850, 3000, 3200, 3400, 3600, 3800, 4050, 4300, 4550, 4800,
                      5000, 5250, 5750, 6000, 6250, 6750, 7000, 7500, 8000, 8500, 9000, 9500, 10000, 10500, 11500,
                      12000, 12500, 13500, 14000, 15000, 16000, 17000, 18000, 19000, 20000, "Off"]


def rd13(v):
    return str(CROSSOVER_FREQS_13[v]) if 0 <= v < len(CROSSOVER_FREQS_13) else str(v)


def rd14(v):
    disp = v - 360
    anno = {0: "MONO", 360: "MONO", -360: "MONO", 45: "STEREO", -315: "STEREO", 90: "L-R, R-L", -270: "L-R, R-L",
            135: "R, L INV", -225: "R, L INV", 180: "MONO INV", -180: "MONO INV", 225: "STEREO INV",
            -135: "STEREO INV", 270: "R-L, L-R", -90: "R-L, L-R", 315: "R, L", -45: "R, L"}
    a = anno.get(disp)
    return f"{disp:+d} {a}" if a else f"{disp:+d}"


LOW_RT_MULT = ["0.2X", "0.4X", "0.6X", "0.8X", "1.0X", "1.2X", "1.5X", "2.0X", "3.0X", "4.0X"]


def rd15(v):
    return LOW_RT_MULT[v] if 0 <= v < len(LOW_RT_MULT) else str(v)


def rd16(v, rvbdesign_link=None, rvbdesign_size=None):
    ms_table = [242, 291, 329, 363, 395, 425, 454, 483, 512, 541, 570, 600, 629, 660, 691, 723, 755, 789, 824, 860,
                897, 935, 975, 1017, 1061, 1106, 1154, 1203, 1256, 1311, 1369, 1430, 1495, 1564, 1637, 1715, 1799,
                1888, 1984, 2088, 2200, 2321, 2453, 2598, 2757, 2932, 3126, 3344, 3588, 3864, 4179, 4543, 4967, 5468,
                6069, 6802, 7719, 8897, 10468, 12666, 15963, 21456, 32441, 65393]
    ms = ms_table[v] if 0 <= v < len(ms_table) else 0
    if rvbdesign_link:
        # "integer math formula" per the manual - integer division at
        # each step, not float (verified against both of the manual's
        # own worked examples: size=90,ms=2598->1558; size=269,ms=32441->55149).
        ms = ((rvbdesign_size + 16) // 16) * ms // 10
    ms = (ms // 50 + (1 if ms % 50 else 0)) * 50
    return f"{ms / 1000.0:.2f} sec" if ms >= 1000 else f"{ms} ms"


CROSSOVER_FREQS_17 = [30, 60, 90, 120, 151, 181, 212, 243, 273, 336, 398, 461, 525, 589, 654, 818, 986, 1158, 1333,
                      1513, 1697, 1886, 2079, 2278, 2481, 2691, 2906, 3127, 3355, 3591, 3833, 4084, 4343, 4611, 4888,
                      5177, 5476, 5788, 6113, 6453, 6808, 7181, 7573, 7986, 8423, 8886, 9379, 9906, 10472, 11084,
                      11748, 12476, 13281, 14181, 15201, 16379, 17772, 19476, 21674, 24772, "Off"]


def rd17(v):
    return str(CROSSOVER_FREQS_17[v]) if 0 <= v < len(CROSSOVER_FREQS_17) else str(v)


CROSSOVER_FREQS_18 = [525, 589, 654, 818, 986, 1158, 1333, 1513, 1697, 1886, 2079, 2278, 2481, 2691, 2906, 3127,
                      3355, 3591, 3833, 4084, 4343, 4611, 4888, 5177, 5476, 5788, 6113, 6453, 6808, 7181, 7573, 7986,
                      8423, 8886, 9379, 9906, 10472, 11084, 11748, 12476, 13281, 14181, 15201, 16379, 17772, 19476,
                      21674, 24772, "Off"]


def rd18(v):
    return str(CROSSOVER_FREQS_18[v]) if 0 <= v < len(CROSSOVER_FREQS_18) else str(v)


def rd19(v):
    return f"{(v - 16) * 4:+d}%"


def rd20(v):
    return "On" if v else "Off"


def rd21(v):
    return f"{v * 2} ms"


def rd22(v):
    table = ["Off", -24.0, -18.0, -14.5, -12.0, -10.1, -8.5, -7.2, -6.0, -5.0, -4.0, -3.3, -2.5, -1.8, -1.0, "Full"]
    if 0 <= v < len(table):
        t = table[v]
        return t if isinstance(t, str) else f"{t:+.1f}db" if t != 0 else "+0.0db"
    return str(v)


def rd23(v):
    table = [-100, -93, -87, -80, -73, -67, -60, -53, -47, -40, -33, -27, -20, -13, -7, 0, 7, 13, 20, 27, 33, 40, 47,
             53, 60, 67, 73, 80, 87, 93, 100]
    return f"{table[v]:+d}%" if 0 <= v < len(table) else str(v)


def rd24(v):
    return f"{v} ms"


def rd25(v):
    return str(v)


def rd26(v):
    return f"{4.0 + v * 0.5:.1f} Meters"


def rd27(v):
    return f"{140 + v * 5} ms"


def rd28(v):
    return f"{v * 2}%"


def rd29(v, rvbdesign_link=None, rvbdesign_size=None):
    if rvbdesign_link:
        return str(((rvbdesign_size + 16) * v) // 160)
    return str(v)


def rd30(v):
    return "Off" if v == 0 else f"{v}%"


def rd31(v):
    return "Off" if v == 0 else str(v)


def rd32(v):
    table = [f"{d:+.0f}db" if d != 0 else "+0db" for d in range(-85, 1)]
    return table[v] if 0 <= v < len(table) else str(v)


def rd33(v):
    return f"{v - 40:+d}db"


def rd34(v):
    return f"{v - 100:+d}%"


def rd35(v):
    return f"{v / 100.0:.2f} Hz"


LFO_SHAPES = ["Sine", "Cosine", "Square", "Saw", "Pulse", "Triangle"]


def rd36(v):
    return LFO_SHAPES[v] if 0 <= v < len(LFO_SHAPES) else str(v)


def rd37(v):
    return f"{v + 1}%"


def rd38(v):
    return f"{v * 20} ms"


ENV_MODES = ["Off", "One Shot", "Retrigger", "Repeat"]


def rd39(v):
    return ENV_MODES[v] if 0 <= v < len(ENV_MODES) else str(v)


SWITCH_MODES = ["Off", "Switch", "Ramp"]


def rd40(v):
    return SWITCH_MODES[v] if 0 <= v < len(SWITCH_MODES) else str(v)


def rd41(v):
    return str(v - 121)


def rd42(v):
    table = ["Off"] + CROSSOVER_FREQS_13[:-1]
    return str(table[v]) if 0 <= v < len(table) else str(v)


def rd43(v):
    return f"{v / 10.0:.1f} ms"


CHORUS_RATE_HZ = [0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09, 0.10, 0.11, 0.12, 0.13, 0.14, 0.15,
                   0.16, 0.17, 0.18, 0.19, 0.20, 0.22, 0.24, 0.26, 0.28, 0.30, 0.32, 0.34, 0.36, 0.38, 0.40, 0.42,
                   0.44, 0.46, 0.48, 0.50, 0.52, 0.54, 0.56, 0.58, 0.60, 0.62, 0.64, 0.66, 0.68, 0.70, 0.72, 0.74,
                   0.76, 0.78, 0.80, 0.84, 0.88, 0.92, 0.96, 1.00, 1.05, 1.10, 1.15, 1.20, 1.25, 1.30, 1.35, 1.40,
                   1.45, 1.50, 1.55, 1.60, 1.65, 1.70, 1.75, 1.80, 1.85, 1.90, 1.95, 2.00, 2.05, 2.10, 2.15, 2.20,
                   2.25, 2.30, 2.35, 2.40, 2.45, 2.50, 2.55, 2.60, 2.65, 2.70, 2.75, 2.80, 2.85, 2.90, 2.95, 3.00,
                   3.10, 3.20, 3.30, 3.40, 3.50]


def rd44(v):
    return f"{CHORUS_RATE_HZ[v]:.2f} Hz" if 0 <= v < len(CHORUS_RATE_HZ) else str(v)


def rd45(v):
    return f"note{v}"


def rd46(v):
    return f"{430.0 + v * 0.1:.1f} Hz"


PITCH_KEYS = ["C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B"]


def rd47(v):
    return PITCH_KEYS[v] if 0 <= v < len(PITCH_KEYS) else str(v)


PITCH_SCALES = ["Major", "Harmonic"]


def rd48(v):
    return PITCH_SCALES[v] if 0 <= v < len(PITCH_SCALES) else str(v)


def rd49(v):
    return str(v + 1)


PITCH_RULES = ["Round Down", "Round Up", "Shift Down", "Shift Up"]


def rd50(v):
    return PITCH_RULES[v] if 0 <= v < len(PITCH_RULES) else str(v)


def rd51(v):
    return f"interval{v}"


RANGE_DECODE_FUNCS = {i: globals()[f"rd{i}"] for i in range(0, 52)}


# Numeric decode: a second interpretation of each range decode, parallel
# to the rdN() display-string functions above, for consumers (like the
# Loom browser plugin's PCM80 preset importer) that want an actual
# (value, unit) pair to convert into their own engine's parameter units
# rather than a formatted-for-humans string to re-parse. Each entry is
# fn(raw[, link, size]) -> (float_or_None, unit_str_or_None). None means
# "this raw value has no single-number meaning for this field" (a pure
# enum, a named source, "Off"/mute, etc.) - callers should leave their
# corresponding parameter at its own default rather than guess.
def _nd_percent(v):
    return (float(v), "percent")


def _nd_bipolar_percent(table):
    def fn(v):
        return (float(table[v]), "percent") if 0 <= v < len(table) else (None, None)

    return fn


def _nd_ms(scale):
    def fn(v):
        return (v * scale, "ms")

    return fn


def _nd_passthrough(unit):
    def fn(v):
        return (float(v), unit)

    return fn


def _nd_hz_table(table):
    def fn(v):
        if 0 <= v < len(table):
            t = table[v]
            return (float(t), "hz") if isinstance(t, (int, float)) else (None, None)
        return (None, None)

    return fn


def _nd_db_table(table):
    def fn(v):
        if 0 <= v < len(table):
            t = table[v]
            if isinstance(t, (int, float)):
                return (float(t), "db")
            if t == "Full":
                return (0.0, "db")
            return (None, None)  # "Off"
        return (None, None)

    return fn


def _nd_none(v):
    return (None, None)


def _nd0(v):
    return (None, None)


def _nd1(v):
    return (float(v + 40), "bpm")


def _nd6(v):
    return (float(TAP_AVERAGES[v]), "count") if 0 <= v < len(TAP_AVERAGES) else (None, None)


def _nd9(v):
    return (float(v), "percent")


def _nd10(v):
    if v == 0:
        return (None, None)  # "Off"
    if 1 <= v < len(FX_ADJUST_VALUES):
        s = FX_ADJUST_VALUES[v]
        return (float(s.replace("db", "")), "db")
    return (None, None)


def _nd11(v):
    # Returns (dB, "db_phase") for v<80 (phase-inverted half) so callers
    # that care about polarity can check the unit; (dB, "db") otherwise.
    # v==80 is "Off" (mute).
    if v == 80:
        return (None, None)
    if v < 80:
        db = -80 * (v / 79)
        return (db, "db_phase_inverted")
    idx = v - 81
    db = -85 + 85 * (idx / 79)
    return (db, "db")


def _nd12(v):
    return (float(v - 50) / 50.0, "pan-1to1")


def _nd14(v):
    return (float(v - 360), "degrees")


def _nd15(v):
    if 0 <= v < len(LOW_RT_MULT):
        return (float(LOW_RT_MULT[v].rstrip("X")), "ratio")
    return (None, None)


def _nd16(v, link=None, size=None):
    ms_table = [242, 291, 329, 363, 395, 425, 454, 483, 512, 541, 570, 600, 629, 660, 691, 723, 755, 789, 824, 860,
                897, 935, 975, 1017, 1061, 1106, 1154, 1203, 1256, 1311, 1369, 1430, 1495, 1564, 1637, 1715, 1799,
                1888, 1984, 2088, 2200, 2321, 2453, 2598, 2757, 2932, 3126, 3344, 3588, 3864, 4179, 4543, 4967, 5468,
                6069, 6802, 7719, 8897, 10468, 12666, 15963, 21456, 32441, 65393]
    ms = ms_table[v] if 0 <= v < len(ms_table) else 0
    if link:
        ms = ((size + 16) // 16) * ms // 10
    ms = (ms // 50 + (1 if ms % 50 else 0)) * 50
    return (float(ms), "ms")


def _nd19(v):
    return (float((v - 16) * 4), "percent")


def _nd20(v):
    return (1.0 if v else 0.0, "bool")


def _nd25(v):
    return (float(v), "raw")


def _nd26(v):
    return (4.0 + v * 0.5, "meters")


def _nd27(v):
    return (float(140 + v * 5), "ms")


def _nd29(v, link=None, size=None):
    if link:
        return (float(((size + 16) * v) // 160), "raw")
    return (float(v), "raw")


def _nd30(v):
    return (None, None) if v == 0 else (float(v), "percent")


def _nd31(v):
    return (None, None) if v == 0 else (float(v), "raw")


def _nd33(v):
    return (float(v - 40), "db")


def _nd34(v):
    return (float(v - 100), "percent")


def _nd35(v):
    return (v / 100.0, "hz")


def _nd37(v):
    return (float(v + 1), "percent")


def _nd41(v):
    return (float(v - 121), "raw")


def _nd42(v):
    table = [None] + CROSSOVER_FREQS_13[:-1]
    return (float(table[v]), "hz") if 0 <= v < len(table) and table[v] is not None else (None, None)


def _nd43(v):
    return (v / 10.0, "ms")


def _nd44(v):
    return (float(CHORUS_RATE_HZ[v]), "hz") if 0 <= v < len(CHORUS_RATE_HZ) else (None, None)


def _nd49(v):
    return (float(v + 1), "raw")


NUMERIC_DECODE_FUNCS = {
    0: _nd0, 1: _nd1, 2: _nd_passthrough("raw"), 3: _nd_none, 4: _nd_none, 5: _nd_none, 6: _nd6,
    7: _nd_passthrough("raw"), 8: _nd_passthrough("raw"), 9: _nd9, 10: _nd10, 11: _nd11, 12: _nd12,
    13: _nd_hz_table(CROSSOVER_FREQS_13), 14: _nd14, 15: _nd15, 16: _nd16,
    17: _nd_hz_table(CROSSOVER_FREQS_17), 18: _nd_hz_table(CROSSOVER_FREQS_18), 19: _nd19, 20: _nd20,
    21: _nd_ms(2), 22: _nd_db_table(["Off", -24.0, -18.0, -14.5, -12.0, -10.1, -8.5, -7.2, -6.0, -5.0,
                                       -4.0, -3.3, -2.5, -1.8, -1.0, "Full"]),
    23: _nd_bipolar_percent([-100, -93, -87, -80, -73, -67, -60, -53, -47, -40, -33, -27, -20, -13, -7, 0,
                              7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 100]),
    24: _nd_ms(1), 25: _nd25, 26: _nd26, 27: _nd27, 28: lambda v: (float(v * 2), "percent"),
    29: _nd29, 30: _nd30, 31: _nd31,
    32: _nd_db_table(list(range(-85, 1))),
    33: _nd33, 34: _nd34, 35: _nd35, 36: _nd_none, 37: _nd37, 38: _nd_ms(20), 39: _nd_none, 40: _nd_none,
    41: _nd41, 42: _nd42, 43: _nd43, 44: _nd44, 45: _nd_none, 46: _nd_passthrough("hz"), 47: _nd_none,
    48: _nd_none, 49: _nd49, 50: _nd_none, 51: _nd_none,
}
