#!/usr/bin/env python3
"""Host only. Compiles native guard, fake wrapper tests and primer syntax; no adb."""
from pathlib import Path
import importlib.util
import os
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]

def run(args):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True)

with tempfile.TemporaryDirectory(prefix="modem-robust-host-") as name:
    work = Path(name)
    cc, cxx = os.environ.get("CC", "cc"), os.environ.get("CXX", "c++")
    flags = ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-deprecated-declarations"]
    run([cc, "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror", HERE / "fake_ta.c", "-o", work / "fake.so"])
    run([cxx, *flags, "-DMODEM_GUARD_HOST", HERE / "modem_guard.cpp", "-ldl", "-lcrypto", "-o", work / "guard-compile-only"])
    run([cxx, *flags, "-Wno-unused-function", HERE / "host_test.cpp", "-ldl", "-lcrypto", "-o", work / "host-test"])
    run([work / "host-test", work / "fake.so"])
    (work / "sys").mkdir()
    (work / "sys/system_properties.h").write_text(
        "#pragma once\n#define PROP_VALUE_MAX 92\n"
        "int __system_property_get(const char*, char*);\n"
        "int __system_property_set(const char*, const char*);\n")
    # Also syntax-check the Android build of the guard (property API stubbed).
    run([cxx, *flags, "-fsyntax-only", "-I", work, HERE / "modem_guard.cpp"])
    primer = HERE.parent / "attach-primer"
    run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only", "-I", work,
         "-I", primer / "include", primer / "wds_attach_primer.c"])
    print("PASS full guard host build, Android debug branch + primer strict syntax")
    patcher = ROOT / "vendor/sony/yoshino-common/modem_switcher_patch.py"
    blob = ROOT / "vendor/sony/yoshino-common/proprietary/bin/sony-modem-switcher"
    if patcher.exists() and blob.exists():
        spec = importlib.util.spec_from_file_location("patcher", patcher)
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        import hashlib
        original = blob.read_bytes(); result = module.patch(original)
        assert hashlib.sha256(result).hexdigest() == module.AFTER
        changed = {i for i, (a, b) in enumerate(zip(original, result)) if a != b}
        allowed = {i for offset, _, _ in module.REPLACEMENTS for i in range(offset, offset + 4)}
        assert changed <= allowed and changed
        for invalid in [result, original[:-1], b"corrupt" + original[7:]]:
            try: module.patch(invalid)
            except ValueError: pass
            else: raise AssertionError("precondition accepted corrupt/already-patched input")
        run(["python3", patcher, blob, work / "patched-switcher"])
        assert blob.read_bytes() == original
        copy = work / "inplace-test"
        copy.write_bytes(original)
        refused = subprocess.run(["python3", str(patcher), str(copy), str(copy)], capture_output=True)
        assert refused.returncode != 0 and copy.read_bytes() == original
        print("PASS optional binary patch: output hash, six sites, invalid/repeated/in-place inputs refused")
    else:
        print("SKIP optional binary recipe: optional patch or original blob absent")
print("ALL HOST CHECKS PASSED")
