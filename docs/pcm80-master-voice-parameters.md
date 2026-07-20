# PCM80/81 Master/Voice parameters — known gap, notes toward closing it

Every 4-Voice and 6-Voice algorithm's field groups pair a set of per-voice
"VoiceN" parameters with one "Master" parameter in the same group (e.g.
`Levels Master` alongside `Levels Voice1`-`Voice4`). The MIDI
Implementation Details manual documents each Master field's own Range
Decode (its display formatting) but never publishes the formula for how
a Master value combines with the per-voice values it's meant to scale -
see `Pcm80Preset.h`'s own doc comment for how that gap is currently
handled by the importer (every `EngineAdapter::importPcm80Preset()`
reads only the raw VoiceN fields and ignores Master entirely).

This is not yet reverse-engineered. What follows is domain knowledge
relayed from an outside PCM80/81 expert with real hardware access (credit:
forum handle "oldgearguy"), captured here so it isn't lost before this
gets tackled, not a claim that it's been implemented.

## The 12 Master parameters

Across all algorithms, the full set of Master-scaled parameter types is:
Feedback, X-Feedback, Resonance, Chorus Rate, Chorus Depth, HiCut, LoCut,
Delay Time, Pan, Level, Scale, Cents. (Not every algorithm has all
twelve - each only has the ones its own per-voice parameter set uses.)

## Known behavior

*Most* Master parameters are a simple uniform shift applied to every
voice's own value - e.g. Master Pan set to "2R" shifts all voices' pan
positions two steps right (a preset whose voices were originally centered
around 50L-48R would read as shifted by that same +2 offset once Master
Pan is applied).

Some do not follow that simple-shift pattern - Level is reportedly the
*worst* offender ("behaves oddly," "the weirdest"). Working out its real
formula (and confirming the simple-shift assumption for the others) needs
empirical work directly on PCM80/81 hardware: watching the front-panel
display while turning the Master encoder, counting how many clicks it
takes before a given voice's own displayed value actually changes, and
mapping where the range's "walls" (clamped ends) and "holes" (dead zones/
non-monotonic jumps) are - not something derivable from the MIDI
Implementation Details manual alone.

## Status

Deliberately deferred - see the CLAUDE.md/session guidance that
prioritized getting the *basic* per-voice parameter import correct first
before tackling Master scaling, and Patching (the modulation-routing
system) after that. Until this is implemented, accurate PCM80/81 preset
comparisons should use test presets with all relevant Masters left at
their neutral default (Levels Master +0 dB; DelayTime, Feedback, and
Panning Master 100%) so the un-scaled raw-voice import matches real
hardware behavior.
