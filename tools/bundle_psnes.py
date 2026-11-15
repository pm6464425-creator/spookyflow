#!/usr/bin/env python3
"""Bundle the pinned official PS3 pSNES 7.2 release inside SpookyFlow.
Retail PS3 PKG AES-CTR is decoded; the original SELF stays unmodified.
"""
import argparse
import hashlib
import shutil
import struct
import subprocess
from pathlib import Path

PKG_SHA256 = '9c7a5f3eae36b08ef6fa0715b61daee343160eac33c3d03fa30ccbe41fc8f7fc'
EBOOT_SHA256 = '6a0c3f62c9b4c45715e0041de9b7688652614e9933d0e0a4d9c6f6ccb0ebd707'
PKG_RETAIL_AES = '2e7b71d7c9c9a14ea3221f188828b8f8'
PKG_BYTES = 2979792
PKG_COUNT = 63

def bundle(pkg, out, header):
    data = Path(pkg).read_bytes()
    if hashlib.sha256(data).hexdigest() != PKG_SHA256 or len(data) != PKG_BYTES:
        raise ValueError('Incorrect pSNES PS3 release (size/SHA256)')
    if data[:4] != b'\x7fPKG' or data[4:8] != b'\x80\x00\x00\x01':
        raise ValueError('Expected PS3 finalized PKG')
    count = struct.unpack_from('>I', data, 0x14)[0]
    total = struct.unpack_from('>Q', data, 0x18)[0]
    base, clen = struct.unpack_from('>QQ', data, 0x20)
    cid = data[0x30:0x60].split(b'\x00')[0]
    if (count != PKG_COUNT or total != len(data) or base + clen > len(data)
            or cid != b'UP0001-PSNE00001_00-0000000000000000'):
        raise ValueError('Wrong PS3 package header/Content ID')
    decrypted = subprocess.run(
        ['openssl', 'enc', '-d', '-aes-128-ctr', '-nopad', '-K', PKG_RETAIL_AES,
         '-iv', data[0x70:0x80].hex()],
        input=data[base:base+clen], capture_output=True, check=True).stdout
    if len(decrypted) != clen:
        raise ValueError('Truncated decrypted PKG')
    items = []
    seen = set()
    for i in range(count):
        no, nl, fo, fl, typ, _ = struct.unpack_from('>IIQQII', decrypted, i*32)
        if not nl or nl > 256 or no < count*32 or no+nl > clen or fo+fl > clen:
            raise ValueError('Corrupt PKG entry '+str(i))
        name = decrypted[no:no+nl].decode('ascii')
        if (name in seen or '\\' in name or
                any(p in ('', '.', '..') for p in name.split('/'))):
            raise ValueError('Unsafe or duplicate PKG path '+name)
        seen.add(name)
        if not (name in ('ICON0.PNG', 'PARAM.SFO', 'USRDIR') or name.startswith('USRDIR/')):
            raise ValueError('Unexpected relative path '+name)
        kind = typ & 0xff
        if kind not in (1, 3, 4):
            raise ValueError('Unknown PKG entry type '+str(kind))
        items.append((name, kind, decrypted[fo:fo+fl]))
    files = [(name, data) for name, kind, data in items if kind != 4]
    if len(files) != 55 or len(seen) != 63:
        raise ValueError('Unexpected pSNES file count')
    contents = dict(files)
    if hashlib.sha256(contents.get('USRDIR/EBOOT.BIN', b'')).hexdigest() != EBOOT_SHA256:
        raise ValueError('Original PS3 pSNES EBOOT checksum mismatch')
    if not contents['PARAM.SFO'].startswith(b'\x00PSF') or b'PSNE00001' not in contents['PARAM.SFO']:
        raise ValueError('Wrong pSNES SFO')
    out = Path(out)
    if out.exists():
        shutil.rmtree(out)
    for name, data in files:
        dest = out/name
        dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(data)
    header = Path(header)
    header.parent.mkdir(parents=True,exist_ok=True)
    lines = [
        '/* Generated from official SHA256-pinned PS3 pSNES 7.2; DO NOT EDIT. */',
        '#ifndef SF_PSNES_FILELIST_H',
        '#define SF_PSNES_FILELIST_H',
        'typedef struct {const char *relative; unsigned long long size;} SfPsnesFile;',
        'static const SfPsnesFile sf_psnes_files[] = {'
    ]
    for name, data in files:
        lines.append('    {"%s", %dULL},' % (name,len(data)))
    lines.extend([
        '};',
        '#define SF_PSNES_FILE_COUNT (sizeof(sf_psnes_files)/sizeof(sf_psnes_files[0]))',
        '#endif'
    ])
    header.write_text('\n'.join(lines)+'\n',encoding='ascii')
    print('Embedded verified pSNES PS3 v7.2:',len(files),'files,',
          sum(len(data) for name,data in files),'bytes')

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--pkg',required=True)
    parser.add_argument('--out',required=True)
    parser.add_argument('--header',required=True)
    args=parser.parse_args()
    bundle(args.pkg,args.out,args.header)
