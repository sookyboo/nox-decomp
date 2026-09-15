#!/usr/bin/env python3
"""Compile a Panic EUD script and package its SCRIPT03 object as NXZ.

The Panic project ships a Windows-only EUD compiler.  This utility keeps that
toolchain optional: Linux callers can invoke it with Wine, while callers that
already have a SCRIPT03 object can use --object without Wine.

The compressor deliberately uses the stable initial NXZ Huffman table and
literal symbols only.  This is larger than an adaptive stream, but produces a
portable stream accepted by the original Nox loader and avoids depending on
an undocumented encoder implementation.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


HUFFMAN_TABLE = (
    (2, 0x00), (3, 0x04), (3, 0x0C), (4, 0x14),
    (4, 0x24), (4, 0x34), (4, 0x44), (4, 0x54),
    (4, 0x64), (4, 0x74), (4, 0x84), (4, 0x94),
    (4, 0xA4), (5, 0xB4), (5, 0xD4), (5, 0xF4),
)


def _alphabet() -> list[int]:
    return (
        list(range(0x100, 0x110))
        + [0x00, 0x20, 0x30, 0xFF]
        + list(range(0x01, 0x20))
        + list(range(0x21, 0x30))
        + list(range(0x31, 0xA0))
        + list(range(0xA0, 0xFF))
        + [0x110, 0x111]
    )


ALPHABET = _alphabet()
ALPHABET_INDEX = {symbol: index for index, symbol in enumerate(ALPHABET)}


class BitWriter:
    def __init__(self) -> None:
        self._value = 0
        self._count = 0
        self.data = bytearray()

    def write(self, value: int, count: int) -> None:
        for shift in range(count - 1, -1, -1):
            self._value = (self._value << 1) | ((value >> shift) & 1)
            self._count += 1
            if self._count == 8:
                self.data.append(self._value)
                self._value = 0
                self._count = 0

    def finish(self) -> bytes:
        if self._count:
            self.write(0, 8 - self._count)
        return bytes(self.data)


def _huffman_group(index: int) -> tuple[int, int]:
    for group, (bits, offset) in enumerate(HUFFMAN_TABLE):
        if offset <= index < offset + (1 << bits):
            return group, bits
    raise ValueError(f"NXZ alphabet index is out of range: {index}")


def compress_nxz(payload: bytes) -> bytes:
    """Return a Nox NXZ stream containing *payload*."""

    writer = BitWriter()
    for byte in payload:
        index = ALPHABET_INDEX[byte]
        group, bits = _huffman_group(index)
        writer.write(group, 4)
        writer.write(index - HUFFMAN_TABLE[group][1], bits)
    return struct.pack("<I", len(payload)) + writer.finish()


def _windows_path(path: Path) -> str:
    """Render an absolute Unix path through Wine's conventional Z: drive."""

    return "Z:" + path.resolve().as_posix()


def _run_compiler(
    source: Path,
    output_object: Path,
    compiler: Path,
    wine: str,
    include_dirs: list[Path],
) -> None:
    workdir = source.parent.resolve()
    compiler_dir = Path(tempfile.mkdtemp(prefix="nox-eud-build-", dir=workdir))
    # eudcc resolves SOURCE relative to the makefile directory.  Stage links
    # there so an arbitrary source path can be compiled without modifying the
    # checked-out EUD project.
    staged_source = compiler_dir / source.name
    staged_source.symlink_to(source)
    for sibling in source.parent.iterdir():
        staged_sibling = compiler_dir / sibling.name
        if not staged_sibling.exists() and not staged_sibling.is_symlink():
            staged_sibling.symlink_to(sibling, target_is_directory=sibling.is_dir())
    lines = [
        "[COMPILE]",
        f'OUT_TARGET = "{_windows_path(output_object)}"',
        f'SOURCE = "../{staged_source.name}"',
    ]
    for include_dir in include_dirs:
        staged_include = compiler_dir / include_dir.name
        staged_include.symlink_to(include_dir.resolve(), target_is_directory=True)
        lines.append(f'ADDPATH = "{staged_include.name}"')

    makefile_path = compiler_dir / "make.txt"
    makefile_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    try:
        command = [wine, str(compiler.resolve()), makefile_path.name]
        result = subprocess.run(command, cwd=compiler_dir, check=False)
        if result.returncode != 0:
            raise RuntimeError(f"EUD compiler exited with status {result.returncode}")
        if not output_object.is_file() or output_object.stat().st_size == 0:
            raise RuntimeError(
                "EUD compiler did not produce a non-empty SCRIPT03 object; "
                "check its diagnostics and include paths"
            )
    finally:
        shutil.rmtree(compiler_dir)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build a Panic EUD source/object file into a Nox .nxz script."
    )
    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument("--source", type=Path, help="EUD C source file")
    input_group.add_argument("--object", type=Path, help="already compiled SCRIPT03 object")
    parser.add_argument("--output", type=Path, required=True, help="output .nxz path")
    parser.add_argument(
        "--compiler",
        type=Path,
        default=Path("build-deps/eud-maps-project/eud_project/eudcc/eudcc.exe"),
        help="bundled eudcc.exe path (used with --source)",
    )
    parser.add_argument("--wine", default="wine", help="Wine executable")
    parser.add_argument(
        "-I", "--include-dir", action="append", type=Path, default=[],
        help="additional EUD compiler include directory; may be repeated",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    if args.object:
        object_path = args.object.resolve()
        if not object_path.is_file():
            raise SystemExit(f"object does not exist: {object_path}")
        payload = object_path.read_bytes()
    else:
        source = args.source.resolve()
        compiler = args.compiler.resolve()
        if not source.is_file():
            raise SystemExit(f"source does not exist: {source}")
        if not compiler.is_file():
            raise SystemExit(f"compiler does not exist: {compiler}")
        if shutil.which(args.wine) is None:
            raise SystemExit(
                f"Wine executable not found: {args.wine}; use --object or install Wine"
            )
        with tempfile.NamedTemporaryFile(
            suffix=".obj", prefix="nox-eud-", dir=output.parent, delete=False
        ) as object_file:
            object_path = Path(object_file.name)
        try:
            _run_compiler(source, object_path, compiler, args.wine, args.include_dir)
            payload = object_path.read_bytes()
        finally:
            object_path.unlink(missing_ok=True)

    if payload[:8] != b"SCRIPT03" or b"CODE" not in payload or b"FUNC" not in payload:
        raise SystemExit(
            "input is not a complete SCRIPT03 object produced by the EUD compiler"
        )
    output.write_bytes(compress_nxz(payload))
    print(f"wrote {output} ({len(payload)} bytes -> {output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"build_eud_nxz: {error}", file=sys.stderr)
        raise SystemExit(1)
