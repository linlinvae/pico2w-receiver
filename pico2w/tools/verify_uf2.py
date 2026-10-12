"""Check generated RP2350 ARM Secure UF2 and flash reservation without hardware."""
import argparse
import struct
from pathlib import Path

def verify(path):
    data = Path(path).read_bytes()
    assert data and len(data) % 512 == 0, 'Invalid UF2 size'
    count = len(data) // 512
    # Picotool 2.2.0 prepends a ROM-ignore block for RP2350 boot compatibility.
    # UF2_EXTENSION_RP2_IGNORE_BLOCK is defined by the SDK's boot/uf2.h.
    first = struct.unpack_from('<8I', data)
    ignored = 1 if first[7] == 0xE48BFF57 else 0
    real_count = count - ignored
    seen, addresses = set(), []
    for offset in range(0, len(data), 512):
        block = data[offset:offset+512]
        m0, m1, flags, addr, size, number, total, family = struct.unpack_from('<8I', block)
        assert (m0, m1) == (0x0A324655, 0x9E5D5157), 'Bad start magic'
        assert struct.unpack_from('<I', block, 508)[0] == 0x0AB16F30, 'Bad end magic'
        assert flags & 0x2000 and size == 256, 'Missing family or invalid payload'
        if family == 0xE48BFF57:
            assert offset == 0 and flags == 0xA000 and addr == 0x10FFFF00 and number == 0 and total == 2, 'Unexpected absolute block'
            assert struct.unpack_from('<I', block, 288)[0] == 0x9957E304, 'Missing ROM ignore extension'
            assert block[32:288] == bytes([0xEF]) * 256, 'Unexpected ignored payload'
            continue
        assert family == 0xE48BFF59, 'Not RP2350 ARM Secure'
        assert total == real_count and number < total and number not in seen, 'Invalid block numbering'
        assert not flags & 1, 'Unexpected non-flash block'
        assert 0x10000000 <= addr < addr + size <= 0x103FB000, 'Overlaps configuration / Bluetooth banks / erratum sector'
        seen.add(number); addresses.append(addr)
    assert len(set(addresses)) == real_count, 'Duplicate flash addresses'
    assert sorted(addresses) == list(range(0x10000000, max(addresses) + 256, 256)), 'Non-contiguous firmware'
    print(f'UF2: {real_count} firmware blocks + {ignored} ROM-ignore block; RP2350 ARM Secure; flash 0x10000000..0x{max(addresses)+256:08X}; config/bond/erratum storage untouched')

if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('file'); verify(p.parse_args().file)
