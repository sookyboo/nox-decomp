# Compatibility path resolution

The Linux compatibility layer owns translation of Windows-style game paths.
`dos_to_unix()` converts backslashes to separators, and `casepath()` walks
each directory component using a case-insensitive comparison before rebuilding
the path with the filesystem's actual spelling. This is required because the
original game supplies Windows-style paths while Linux filesystems are usually
case-sensitive.

`compat_fopen()` is the normal stdio asset-loading entry point and
`compat_open()` covers lower-level reads. Both normalize and case-fold their
paths before attempting the real libc operation. `external_compat_casepath()`
is also used by movie loading and exposes the same normalization for callers
that need a resolved path.

The resolver treats the final component as a file when it already exists, so
`SoundSet.bin` resolves to an extracted `soundset.bin` without attempting to
open the file as a directory. If the final component does not exist, its
requested spelling is retained after existing parent directories are resolved;
this preserves save-file creation through `compat_open()`.

`tests/compat_path_test.c` creates a synthetic `PrimaryInstall/DataFiles`
fixture, then exercises the normalizer, `compat_fopen()`, and `compat_open()`
using mismatched casing and Windows separators. This covers the file-resolution
portion of `1247869`; solo startup state selection and LAN socket registration
remain normal-target integration coverage.

`compat_open()` is also the owning compatibility boundary for save writes. It
must case-fold existing parent directories while preserving the requested new
leaf filename, so a save can be created when the map/save directory spelling
differs from the spelling stored in the save state. `tests/save_casepath_test.c`
creates this situation and verifies a write/read round trip through
`compat_open()`; full save serialization and map restoration remain game
integration coverage.

The 32-bit test target uses `_FILE_OFFSET_BITS=64` so its directory-stream ABI
matches the large-file Linux toolchain used to enumerate the fixture.
