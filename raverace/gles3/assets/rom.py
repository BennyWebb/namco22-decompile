#!/usr/bin/env python3
"""Offline Rave Racer asset compiler/inspector. No emulator integration."""

import argparse
import hashlib
import json
import pathlib
import sys
import numpy as np

VERSION = 1


def digest(data):
    return hashlib.sha256(data).hexdigest()


def encoded(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def require(condition, message):
    if not condition:
        raise ValueError(message)


class ROM:
    def __init__(self, root):
        self.sources = {}

        def read(name, size):
            data = (root / name).read_bytes()
            require(len(data) == size, f"{name}: expected {size} bytes, got {len(data)}")
            self.sources[name] = dict(bytes=size, sha256=digest(data))
            return data

        planes = [
            np.frombuffer(b"".join(read(f"rv1pot{s}{i}.{5-i}{e}", 0x80000) for i in range(4)), np.uint8).astype(
                np.int32
            )
            for s, e in [("l", "b"), ("m", "c"), ("u", "d")]
        ]
        w = planes[0] | planes[1] << 8 | planes[2] << 16
        self.words = (w ^ 0x800000) - 0x800000
        self.cg = np.frombuffer(
            b"".join(
                read(f"rv1cg{i}.{s}", 0x200000) for i, s in enumerate(["1a", "1c", "1d", "1e", "1f", "1j", "1k", "1n"])
            ),
            np.uint8,
        )
        self.tiles = np.frombuffer(read("rv1ccrl.5a", 0x200000), "<u2").astype(np.uint32)
        nib = np.frombuffer(read("rv1ccrh.5c", 0x80000), np.uint8)
        self.attr = np.empty(0x100000, np.uint8)
        self.attr[::2], self.attr[1::2] = nib >> 4, nib & 15
        self.tiles |= (self.attr.astype(np.uint32) & 1) << 16

    def word(self, address):
        require(0 <= address < len(self.words), f"point address outside ROM: {address:#x}")
        return int(self.words[address])

    def model(self, code):
        cursor = self.word(code)
        quads, packets, vertices, indices = [], [], [], []
        for _ in range(4096):
            chunk = self.word(cursor)
            cursor += 1
            if chunk < 0:
                require(chunk == -1, f"object {code}: unexpected terminator {chunk}")
                return dict(
                    code=code,
                    list_address=self.word(code),
                    vertices=vertices,
                    indices=indices,
                    quads=quads,
                    lighting_packets=packets,
                )
            size = self.word(chunk)
            require(0 <= size <= 256, f"object {code}: invalid chunk length {size}")
            pos, end = chunk + 1, chunk + 1 + size
            while pos < end:
                length = self.word(pos)
                base = pos + 1
                require(length in (23, 24, 16, 13), f"object {code}: unknown packet {length} at {pos:#x}")
                require(base + length <= end, f"object {code}: truncated packet at {pos:#x}")
                raw = [self.word(base + i) for i in range(length)]
                if length in (23, 24):
                    offset = 3 if length == 23 else 4
                    uv = [x & 0xFFFFFF for x in raw[offset : offset + 8]]
                    xyz = raw[offset + 8 : offset + 20]
                    first = len(vertices)
                    vertices.extend(
                        [
                            xyz[i * 3 : i * 3 + 3] + [uv[i * 2] & 4095, uv[i * 2 + 1] & 4095, (uv[i] >> 16) & 255]
                            for i in range(4)
                        ]
                    )
                    indices.extend([first, first + 1, first + 2, first, first + 2, first + 3])
                    quads.append(
                        dict(
                            packet_address=pos,
                            format=raw[0] & 0xFFFFFF,
                            flags=raw[1] & 0xFFFFFF,
                            color=raw[2] & 0xFFFFFF,
                            polyshift=raw[3] & 0xFFFFFF if length == 24 else 0,
                            uv_raw=uv,
                            first_vertex=first,
                            lighting_packet_count=len(packets),
                            material=[(uv[1] >> 12) & 15, (raw[2] >> 8) & 127, (uv[0] >> 12) & 15],
                        )
                    )
                else:
                    packets.append(
                        dict(
                            address=pos,
                            length=length,
                            raw=raw,
                            reset=length == 16,
                            normals=raw[4:] if length == 16 else raw[1:],
                        )
                    )
                pos = base + length
                if raw[0] & 0x800000:
                    break  # same early-end semantics as blit_quads
        raise ValueError(f"object {code}: unterminated list")

    def pens(self, bank, cmode, u0, v0, width, height):
        u = (u0 + np.arange(width, dtype=np.uint32)) & 4095
        v = ((v0 + np.arange(height, dtype=np.uint32)) & 4095) | (bank * 4096)
        ix = ((v[:, None] & 0xFFF0) << 4) | ((u[None, :] & 0xFF0) >> 4)
        tile, at = self.tiles[ix], self.attr[ix]
        x = np.broadcast_to(u[None, :] & 15, ix.shape)
        y = np.broadcast_to(v[:, None] & 15, ix.shape)
        x = np.where(at & 4, 15 - x, x)
        y = np.where(at & 2, 15 - y, y)
        xx, yy = np.where(at & 8, y, x), np.where(at & 8, x, y)
        off = tile * 256 + yy * 16 + xx
        raw = np.where(off < len(self.cg), self.cg[np.minimum(off, len(self.cg) - 1)], 0).astype(np.uint32)
        if cmode & 4:
            shift, mask, offset = 2 * (~cmode & 3), 3, 0xEC + ((cmode & 8) << 1)
        elif cmode & 2:
            shift, mask, offset = 4 * (~cmode & 1), 15, 0xE0 + ((cmode & 8) << 1)
        else:
            shift, mask, offset = 0, 255, 0
        return ((offset + ((raw >> shift) & mask)) & 255).astype(np.uint8)


def validate_model(m):
    v, ix = m["vertices"], m["indices"]
    require(len(v) == 4 * len(m["quads"]) and len(ix) == 6 * len(m["quads"]), "buffer lengths")
    require(all(0 <= i < len(v) for i in ix), "index outside vertex buffer")
    for q in m["quads"]:
        n = q["first_vertex"]
        require(ix[n // 4 * 6 : n // 4 * 6 + 6] == [n, n + 1, n + 2, n, n + 2, n + 3], "quad winding")
        require(
            q["material"] == [(q["uv_raw"][1] >> 12) & 15, (q["color"] >> 8) & 127, (q["uv_raw"][0] >> 12) & 15],
            "material association",
        )
        for i in range(4):
            require(v[n + i][3:5] == [q["uv_raw"][2 * i] & 4095, q["uv_raw"][2 * i + 1] & 4095], "UV association")
            require(all(-8388608 <= x <= 8388607 for x in v[n + i][:3]), "coordinate range")
    return len(v), len(ix)
