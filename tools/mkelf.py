#!/usr/bin/env python
"""Build a Sony S1Boot ELF image (kernel + ramdisk + cmdline) for Xperia Miro."""
import optparse
import os
import struct


def align_up(val, boundary):
    return (val + boundary - 1) & ~(boundary - 1)


def make_sony_elf(kernel_data, ramdisk_data, cmdline_str,
                  kernel_addr=0x00208000, ramdisk_addr=0x01200000):
    cmdline_bytes = cmdline_str.encode('ascii') + b'\x00'

    ph0_off = 0x1000
    ph1_off = align_up(ph0_off + len(kernel_data), 4096)
    ph2_off = align_up(ph1_off + len(ramdisk_data), 512)

    e_ident = b'\x7fELF\x01\x01\x01a' + b'\x00' * 8
    ehdr = struct.pack(
        '<16sHHIIIIIHHHHHH',
        e_ident,
        2,              # ET_EXEC
        40,             # EM_ARM
        1,              # EV_CURRENT
        kernel_addr,    # e_entry
        52,             # e_phoff
        0,              # e_shoff
        0,              # e_flags
        52,             # e_ehsize
        32,             # e_phentsize
        3,              # e_phnum
        0,              # e_shentsize
        0,              # e_shnum
        0,              # e_shstrndx
    )
    ph0 = struct.pack('<IIIIIIII', 1, ph0_off, kernel_addr, kernel_addr,
                      len(kernel_data), len(kernel_data), 0x00000000, 0)
    ph1 = struct.pack('<IIIIIIII', 1, ph1_off, ramdisk_addr, ramdisk_addr,
                      len(ramdisk_data), len(ramdisk_data), 0x80000000, 0)
    ph2 = struct.pack('<IIIIIIII', 4, ph2_off, 0, 0,
                      len(cmdline_bytes), len(cmdline_bytes), 0x20000000, 0)

    out = bytearray(align_up(ph2_off + len(cmdline_bytes), 4096))
    out[0:52] = ehdr
    out[52:84] = ph0
    out[84:116] = ph1
    out[116:148] = ph2
    out[ph0_off:ph0_off + len(kernel_data)] = kernel_data
    out[ph1_off:ph1_off + len(ramdisk_data)] = ramdisk_data
    out[ph2_off:ph2_off + len(cmdline_bytes)] = cmdline_bytes
    return bytes(out)


def main():
    parser = optparse.OptionParser()
    parser.add_option('--kernel', dest='kernel')
    parser.add_option('--ramdisk', dest='ramdisk')
    parser.add_option('--cmdline', dest='cmdline', default='')
    parser.add_option('--kernel-addr', dest='kernel_addr', default='0x00208000')
    parser.add_option('--ramdisk-addr', dest='ramdisk_addr', default='0x01200000')
    parser.add_option('-o', '--output', dest='output')
    opts, _ = parser.parse_args()

    if not opts.kernel or not opts.ramdisk or not opts.output:
        parser.error('kernel, ramdisk, and output are required')

    with open(opts.kernel, 'rb') as fk:
        kdata = fk.read()
    with open(opts.ramdisk, 'rb') as fr:
        rdata = fr.read()

    elf = make_sony_elf(
        kdata,
        rdata,
        opts.cmdline,
        int(opts.kernel_addr, 0),
        int(opts.ramdisk_addr, 0),
    )
    with open(opts.output, 'wb') as fo:
        fo.write(elf)


if __name__ == '__main__':
    main()
