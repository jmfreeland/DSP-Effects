class BitReader:
    """LSB-first bit reader matching the PCM80 MIDI Implementation Details
    manual's 'General Bitpacking Method': treat the byte stream as one
    giant little-endian integer, read fields sequentially from bit 0
    upward. Validated against the manual's own worked byte sequence
    (bytes 29 00 3c 24 80 f7 decode to Tempo Rate=41, AR Env=0, Sw1=60,
    Sw2=72 - matches the manual's page-30 walkthrough exactly)."""

    def __init__(self, data: bytes):
        self.value = int.from_bytes(data, "little")
        self.pos = 0
        self.total_bits = len(data) * 8

    def read(self, width: int) -> int:
        v = (self.value >> self.pos) & ((1 << width) - 1)
        self.pos += width
        return v

    def bits_remaining(self) -> int:
        return self.total_bits - self.pos


if __name__ == "__main__":
    # Validate against the manual's own worked example (page 30).
    data = bytes.fromhex("29003c2480f7")
    r = BitReader(data)
    tempo_rate = r.read(9)
    ar_env = r.read(7)
    sw1 = r.read(7)
    sw2 = r.read(7)
    assert tempo_rate == 41, tempo_rate
    assert ar_env == 0, ar_env
    assert sw1 == 60, sw1
    assert sw2 == 72, sw2
    print("bitreader validated OK:", tempo_rate, ar_env, sw1, sw2)
