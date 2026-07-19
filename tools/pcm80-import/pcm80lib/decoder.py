"""Decodes a PCM80 preset's bitpacked Effect Control Data into named,
human-readable parameter values, per the MIDI Implementation Details
manual's documented bit-packing format.

Validation status
------------------
The bit-reader, Soft Row Assignments, Unpatchable Parameter Information,
ADJUST Knob Initial Value, and the Patchable Parameter Information +
Patching Information logic were all validated end-to-end against the
manual's own four worked examples (Prime Blue and RandomImages on
algorithm 7 Chorus+Rvb, FSw2 Elevate on algorithm 0 Plate, Super Ball!
on algorithm 6 Glide>Hall) using the *actual* ROM bytes for those exact
presets - not the manual's printed hex, which has its own OCR risk.
Every field across all four decoded within rounding of the manual's own
documented display values; the majority matched exactly, including
enum lookups (Shape, Mode, Off/On), tempo-form Echo:Beat/Cycl:Beat
fields, and Hz/percent/dB conversions.

The other 6 algorithm tables (Chamber, Infinite, Inverse, Concert Hall,
M-Band+Rvb, Res1>Plate, Res2>Plate) share the same validated Controls-
prefix and MOD-suffix building blocks and were transcribed with the
same care, but the manual only provided worked examples for 4 of the
10 algorithms - the other 6 are NOT directly validated against a known
answer. Treat their field decode as high-confidence-but-unverified.

Known imprecisions:
  - Range Decode 11 (gain/phase, "161 level and phase assignments") is
    a smooth approximation of Lexicon's own irregular lookup table -
    the manual's scanned table has OCR-ambiguous entries near each
    half's start. Decoded dB values from this range decode can be off
    by roughly 1dB.
  - Range Decode 16's "Mid Rt" millisecond table (64 entries) may
    contain a small transcription error in 1-2 entries (a couple of
    validated examples were off by ~30-40ms out of a multi-hundred-ms
    value, everything else exact).
  - Patch source names (MIDI CC33-119 range) use a best-effort scheme
    rather than the manual's own irregular controller-name table.

Known unresolved anomaly: record-length overrun
-------------------------------------------------
About a quarter of the 200 factory presets in a real v1.10 ROM dump -
consistently the *shortest* ROM record for a given algorithm - do not
contain enough bitpacked bytes to hold this module's algorithm field
table, even after independently re-deriving both the fixed 187-bit
header (Soft Row Assignments + Unpatchable Parameter Information +
ADJUST Knob Initial Value) and the Plate algorithm's own 569-bit
patchable-field table directly from the manual's own published
"Patchable Parameter Bitpack Information" tables (not from this
codebase's earlier, less-certain transcription) and finding they sum
to exactly what this module already used. The manual's own worked
Prime Blue example is *also* longer (185 bitpack bytes, 10 real
patches) than the same-named preset's actual ROM record (173 bitpack
bytes, room for only ~3 patches) - so the manual's illustrative dump
is likely an enhanced/hand-built example rather than a literal
factory-ROM extract, which is consistent with this gap but does not
explain why the ROM's own shortest records fall short of even the
*fixed* (non-patch) portion of the field table.

The record-chain boundaries themselves (the `record_length` field
driving extract_presets.py's chain walk) are validated independently
and strongly: walking the whole ROM with them yields exactly 200
records, the PCM80's known factory preset count, with every name
decoding to a plausible real preset - so that isn't in question here.
Root cause not yet identified. decode_preset() detects this
condition (bits_consumed_before_patches > bits_total) and reports it
via the returned dict's "reliable" key rather than silently emitting
values read from zero-padding; callers should treat an unreliable
decode's trailing patchable fields and all patches as untrustworthy.
"""

from .bitreader import BitReader
from .range_decode import RANGE_DECODE_FUNCS, NUMERIC_DECODE_FUNCS
from .algorithm_tables import ALGORITHM_TABLES, UNPATCHABLE_TABLE, ALGORITHM_NAMES
from .patch_sources import PATCH_SOURCE_NAMES

CYCL_BEAT_RDS = {35}


def apply_rd(rd_id, value, ctx):
    fn = RANGE_DECODE_FUNCS.get(rd_id)
    if fn is None:
        return str(value)
    try:
        if rd_id in (16, 29):
            return fn(value, ctx.get("rvbdesign_link"), ctx.get("rvbdesign_size"))
        return fn(value)
    except Exception as exc:
        return f"<rd{rd_id} err:{exc} raw={value}>"


def apply_rd_numeric(rd_id, value, ctx):
    """The (numeric, unit) counterpart to apply_rd() - see
    range_decode.py's NUMERIC_DECODE_FUNCS doc comment. Returns
    (None, None) on any lookup miss or exception, same as an
    unconvertible field, rather than raising - a numeric-import
    consumer should treat that as "leave this parameter alone"."""
    fn = NUMERIC_DECODE_FUNCS.get(rd_id)
    if fn is None:
        return (None, None)
    try:
        if rd_id in (16, 29):
            return fn(value, ctx.get("rvbdesign_link"), ctx.get("rvbdesign_size"))
        return fn(value)
    except Exception:
        return (None, None)


def decode_tempo_value(raw10, rd_id):
    echoes = raw10 >> 5
    beats = raw10 & 0x1F
    unit = "Cycl:Beat" if rd_id in CYCL_BEAT_RDS else "Echo:Beat"
    return f"{echoes}: {beats} {unit}"


def is_algorithm_decodable(algorithm_id: int) -> bool:
    return algorithm_id in ALGORITHM_TABLES


def decode_preset(name: str, knob_label: str, algorithm_id: int, bitpack: bytes) -> dict:
    """Decode one preset's bitpacked Effect Control Data.

    Raises KeyError if algorithm_id isn't one of the 10 base algorithms
    this manual documents (e.g. an expansion-card algorithm) - callers
    should check is_algorithm_decodable() first and fall back to raw
    hex for anything else.
    """
    r = BitReader(bitpack)
    out = {"name": name, "knob_label": knob_label, "algorithm_id": algorithm_id,
           "algorithm": ALGORITHM_NAMES[algorithm_id]}

    out["soft_row"] = [(lambda b: (b >> 4, b & 0xF))(r.read(8)) for _ in range(10)]

    unpatchable = {}
    for bits, _dlid, _dn, group, label, rd_id in [e[1:] for e in UNPATCHABLE_TABLE]:
        raw = r.read(bits)
        unpatchable[f"{group} {label}"] = apply_rd(rd_id, raw, {})
    out["unpatchable"] = unpatchable

    out["adjust_initial"] = r.read(7)

    table = ALGORITHM_TABLES[algorithm_id]
    raw_fields = []
    pending_tempo_flag = 0
    field_seq = 0
    for entry in table:
        if entry[0] == "flag":
            pending_tempo_flag = r.read(1)
            continue
        _, bits, dlid, dn, group, label, rd_id = entry
        raw = r.read(bits)
        raw_fields.append({"seq": field_seq, "group": group, "label": label, "raw": raw, "bits": bits,
                            "dest_list_id": dlid, "dest_number": dn, "range_decode": rd_id,
                            "tempo_active": bool(pending_tempo_flag)})
        pending_tempo_flag = 0
        field_seq += 1

    bits_before_patches = r.pos
    reliable = bits_before_patches <= r.total_bits
    out["reliable"] = reliable
    if not reliable:
        out["reliability_note"] = (
            f"record ran out of bitpacked data after {r.total_bits} bits, but the fixed "
            f"header+patchable-field table for this algorithm needs {bits_before_patches} bits "
            "before any Patching Information - see the 'Known unresolved anomaly' note in "
            "decoder.py's module docstring. Fields beyond the available data were read from "
            "zero-padding and are not trustworthy; patches were not parsed."
        )

    dest_lookup = {(f["dest_list_id"], f["dest_number"]): f for f in raw_fields if f["dest_list_id"] is not None}
    raw_patches = []
    for patch_idx in range(10):
        if not reliable:
            break
        if r.bits_remaining() < 1:
            break
        valid = r.read(1)
        if not valid:
            continue
        if r.bits_remaining() < 16:
            break
        src = r.read(7)
        dest_list_id = r.read(1)
        dest_number = r.read(8)
        count = r.read(4)
        dest_entry = dest_lookup.get((dest_list_id, dest_number))
        dest_tempo_active = dest_entry["tempo_active"] if dest_entry else False
        dest_bits = 10 if dest_tempo_active else (dest_entry["bits"] if dest_entry else 10)
        points = []
        for _ in range(count):
            if r.bits_remaining() < 7 + dest_bits:
                break
            point_val = r.read(7)
            dest_val = r.read(dest_bits)
            points.append({"point": point_val, "raw": dest_val})
        raw_patches.append({"index": patch_idx, "src": src, "dest_entry": dest_entry,
                             "dest_list_id": dest_list_id, "dest_number": dest_number,
                             "dest_tempo_active": dest_tempo_active, "points": points})

    out["bits_consumed"] = r.pos
    out["bits_total"] = r.total_bits

    ctx = {"rvbdesign_link": False, "rvbdesign_size": 0}
    for f in raw_fields:
        if f["group"] == "RvbDesign" and f["label"] == "Link":
            ctx["rvbdesign_link"] = bool(f["raw"])
        if f["group"] == "RvbDesign" and f["label"] == "Size":
            ctx["rvbdesign_size"] = f["raw"]

    patchable = []
    for f in raw_fields:
        if f["tempo_active"]:
            value = decode_tempo_value(f["raw"], f["range_decode"])
            # Converting tempo-form Echo:Beat/Cycl:Beat back to a real
            # seconds/Hz value needs the preset's own Tempo Rate (BPM)
            # and the manual doesn't fully spell out the echoes/beats
            # arithmetic - out of scope for now, see decoder.py's module
            # docstring. Numeric consumers should leave these alone.
            numeric, unit = (None, None)
        else:
            value = apply_rd(f["range_decode"], f["raw"], ctx)
            numeric, unit = apply_rd_numeric(f["range_decode"], f["raw"], ctx)
        patchable.append({**f, "value": value, "numeric": numeric, "unit": unit})
    out["patchable"] = patchable

    patches = []
    for rp in raw_patches:
        dest_entry = rp["dest_entry"]
        dest_label = f"{dest_entry['group']} {dest_entry['label']}" if dest_entry else \
            f"?(dest_list={rp['dest_list_id']},dest_num={rp['dest_number']})"
        points = []
        for pt in rp["points"]:
            if rp["dest_tempo_active"]:
                display = decode_tempo_value(pt["raw"], dest_entry["range_decode"])
            elif dest_entry:
                display = apply_rd(dest_entry["range_decode"], pt["raw"], ctx)
            else:
                display = str(pt["raw"])
            points.append({"point": pt["point"], "raw": pt["raw"], "display": display})
        patches.append({"index": rp["index"], "source": PATCH_SOURCE_NAMES.get(rp["src"], f"Src{rp['src']}"),
                         "dest": dest_label, "points": points})
    out["patches"] = patches

    return out
