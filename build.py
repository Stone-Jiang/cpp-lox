#!/usr/bin/env python3
"""
build.py - Platform-independent incremental build script for C++ projects.

Convention: all C++ sources (.cpp) and headers (.h/.hpp) live under src/.
Object files are cached under build/<profile>/; the final executables are
placed at the project root (next to build.py) as "app" (release) and
"app_debug" (debug).

Rebuild rule:
    A .cpp file is recompiled if:
        - its .o file doesn't exist yet, OR
        - the .cpp file's mtime is newer than the .o file, OR
        - any header it depends on (directly or transitively, via local
            #include "...") has an mtime newer than the .o file.
    This means editing a header triggers a rebuild of every .cpp that
    depends on it ("its dependents"), while untouched, independent files
    are skipped.

Usage:
    python3 build.py                  # release build (default): -O2, no debug flags
    python3 build.py --release        # same as default, explicit
    python3 build.py --debug          # -O0 -g build with debug flags
    python3 build.py --both           # build both profiles
    python3 build.py --clean          # remove build artifacts
    python3 build.py -j 8             # parallel compilation, 8 workers

Requires a C++ compiler on PATH: g++ or clang++ (auto-detected), or pass
--cc explicitly.
"""

import argparse
import concurrent.futures
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC_DIR_NAME = "src"
BUILD_DIR_NAME = "build"

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')

# ---------------------------------------------------------------------------
# Flag configuration - edit here to change what each profile compiles with.
# ---------------------------------------------------------------------------

# Flags applied to every build, regardless of profile.
COMMON_FLAGS = ["-Wall", "-Wextra", "-fdiagnostics-color=always"]

# Flags applied only to the release (-O2) profile.
RELEASE_FLAGS = ["-O2", "-fno-rtti", "-Wl,--strip-all", "-Wl,--gc-sections"]

# Flags applied only to the debug (-O0) profile.
DEBUG_FLAGS = ["-O0", "-g", "-g3", "-fno-omit-frame-pointer"]

# ---------------------------------------------------------------------------
# Preprocessor macros - plain names (no -D), written like you'd use them in
# an #ifdef. Each is passed to the compiler as -D<MACRO>.
# ---------------------------------------------------------------------------
RELEASE_MACROS = []

DEBUG_MACROS = ["DEBUG_TRACE_EXECUTION", "DEBUG_PRINT_CODE", "DEBUG_LOG_GC", "DEBUG_VALUE_TABLE"]

# ---------------------------------------------------------------------------
# Output binary names - edit here to rename the final executables.
# ---------------------------------------------------------------------------
EXE_NAME_RELEASE = "main"
EXE_NAME_DEBUG = "debug"

# Two parallel build configurations, assembled from the lists above.
PROFILES = {
    "release": {
        "flags": RELEASE_FLAGS + [f"-D{macro}" for macro in RELEASE_MACROS],
        "desc": "Optimized release build (-O2, no debug flags)",
        "exe_name": EXE_NAME_RELEASE,
    },
    "debug": {
        "flags": DEBUG_FLAGS + [f"-D{macro}" for macro in DEBUG_MACROS],
        "desc": "Debug build (-O0, symbols, assertions on)",
        "exe_name": EXE_NAME_DEBUG,
    },
}


def find_compiler(explicit=None):
    if explicit:
        if shutil.which(explicit):
            return explicit
        sys.exit(f"error: requested compiler '{explicit}' not found on PATH")
    for candidate in ("g++", "clang++", "c++"):
        if shutil.which(candidate):
            return candidate
    sys.exit("error: no C++ compiler (g++/clang++/c++) found on PATH")


def find_sources(src_dir: Path):
    return sorted(p for p in src_dir.rglob("*.cpp"))


def parse_local_includes(file_path: Path):
    """Return the set of quoted (local, non-system) #include targets."""
    targets = []
    try:
        with file_path.open("r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                m = INCLUDE_RE.match(line)
                if m:
                    targets.append(m.group(1))
    except OSError:
        pass
    return targets


def resolve_include(inc_name: str, from_file: Path, src_dir: Path):
    """Resolve a quoted include relative to the including file, then src/."""
    candidates = [from_file.parent / inc_name, src_dir / inc_name]
    for c in candidates:
        if c.exists():
            return c.resolve()
    return None


def transitive_headers(cpp_file: Path, src_dir: Path, cache: dict):
    """
    Return the full set of local headers this .cpp transitively depends on.
    Memoized per header (not per cpp) since headers are shared.
    """
    seen = set()
    stack = [cpp_file]
    result = set()
    while stack:
        current = stack.pop()
        for inc_name in parse_local_includes(current):
            resolved = resolve_include(inc_name, current, src_dir)
            if resolved is None or resolved in seen:
                continue
            seen.add(resolved)
            result.add(resolved)
            stack.append(resolved)
    return result


def needs_rebuild(cpp_file: Path, obj_file: Path, src_dir: Path):
    if not obj_file.exists():
        return True, "no object file yet"

    obj_mtime = obj_file.stat().st_mtime

    if cpp_file.stat().st_mtime > obj_mtime:
        return True, "source changed"

    for header in transitive_headers(cpp_file, src_dir, {}):
        if header.exists() and header.stat().st_mtime > obj_mtime:
            rel = header.relative_to(src_dir.parent) if src_dir.parent in header.parents else header
            return True, f"dependency changed: {rel}"

    return False, "up to date"


def obj_path_for(cpp_file: Path, src_dir: Path, out_dir: Path) -> Path:
    rel = cpp_file.relative_to(src_dir)
    return (out_dir / rel).with_suffix(".o")


def compile_one(compiler, cpp_file, obj_file, src_dir, flags):
    obj_file.parent.mkdir(parents=True, exist_ok=True)
    cmd = [compiler, *COMMON_FLAGS, *flags, "-I", str(src_dir), "-c", str(cpp_file), "-o", str(obj_file)]
    print(f"  CXX  {cpp_file}")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stderr, file=sys.stderr)
        return False
    if proc.stderr.strip():
        print(proc.stderr, file=sys.stderr)  # warnings
    return True


def link(compiler, objects, exe_path, flags):
    exe_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [compiler, *flags, *[str(o) for o in objects], "-o", str(exe_path)]
    print(f"  LINK {exe_path}")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stderr, file=sys.stderr)
        return False
    if proc.stderr.strip():
        print(proc.stderr, file=sys.stderr)
    return True


def build(mode: str, jobs: int, compiler: str, root: Path):
    src_dir = root / SRC_DIR_NAME
    if not src_dir.is_dir():
        sys.exit(f"error: expected sources under '{src_dir}' (convention: all code in src/)")

    profile = PROFILES[mode]
    out_dir = root / BUILD_DIR_NAME / mode
    sources = find_sources(src_dir)
    if not sources:
        sys.exit(f"error: no .cpp files found under {src_dir}")

    print(f"[{mode}] {profile['desc']}")
    print(f"[{mode}] compiler: {compiler}, jobs: {jobs}")

    to_compile = []
    objects = []
    for cpp_file in sources:
        obj_file = obj_path_for(cpp_file, src_dir, out_dir)
        objects.append(obj_file)
        rebuild, reason = needs_rebuild(cpp_file, obj_file, src_dir)
        if rebuild:
            to_compile.append(cpp_file)
            print(f"  [rebuild] {cpp_file}  ({reason})")
        else:
            print(f"  [skip]    {cpp_file}  ({reason})")

    ok = True
    if to_compile:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            futures = {
                pool.submit(
                    compile_one, compiler, cpp_file,
                    obj_path_for(cpp_file, src_dir, out_dir),
                    src_dir, profile["flags"],
                ): cpp_file
                for cpp_file in to_compile
            }
            for fut in concurrent.futures.as_completed(futures):
                if not fut.result():
                    ok = False

    if not ok:
        sys.exit(f"[{mode}] compilation failed")

    exe_path = root / (profile["exe_name"] + (".exe" if sys.platform == "win32" else ""))
    exe_missing = not exe_path.exists()
    if to_compile or exe_missing:
        if not link(compiler, objects, exe_path, profile["flags"]):
            sys.exit(f"[{mode}] link failed")
    else:
        print(f"[{mode}] executable up to date: {exe_path}")

    print(f"[{mode}] done -> {exe_path}")
    return exe_path


def clean(root: Path):
    removed_anything = False

    build_dir = root / BUILD_DIR_NAME
    if build_dir.exists():
        shutil.rmtree(build_dir)
        print(f"removed {build_dir}")
        removed_anything = True

    exe_suffix = ".exe" if sys.platform == "win32" else ""
    for suffixed_name in (EXE_NAME_RELEASE, EXE_NAME_DEBUG):
        exe_path = root / (suffixed_name + exe_suffix)
        if exe_path.exists():
            exe_path.unlink()
            print(f"removed {exe_path}")
            removed_anything = True

    if not removed_anything:
        print("nothing to clean")


def main():
    parser = argparse.ArgumentParser(description="Incremental C++ build script")
    profile_group = parser.add_mutually_exclusive_group()
    profile_group.add_argument("--release", action="store_true",
                                help="-O2 build, no debug flags (default)")
    profile_group.add_argument("--debug", action="store_true",
                                help="-O0 build with debug flags")
    profile_group.add_argument("--both", action="store_true",
                                help="build both release and debug profiles")
    parser.add_argument("--cc", default=None, help="compiler to use (default: auto-detect g++/clang++)")
    parser.add_argument("-j", "--jobs", type=int, default=4, help="parallel compile jobs (default: 4)")
    parser.add_argument("--root", default=".", help="project root containing src/ (default: cwd)")
    parser.add_argument("--clean", action="store_true", help="remove build/ and exit")
    args = parser.parse_args()

    root = Path(args.root).resolve()

    if args.clean:
        clean(root)
        return

    compiler = find_compiler(args.cc)

    if args.both:
        modes = ["release", "debug"]
    elif args.debug:
        modes = ["debug"]
    else:
        modes = ["release"]  # default, and explicit --release

    for mode in modes:
        build(mode, args.jobs, compiler, root)


if __name__ == "__main__":
    main()