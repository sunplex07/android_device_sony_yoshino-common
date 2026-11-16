#!/usr/bin/env python3
# Dependency-free (stdlib only) sanitizer for make-side kernel UAPI headers.
# LOS 23.2 removed the make-side headers_install+clean_headers; the device-local
# Android.mk display fork still consumes $(KERNEL_OBJ)/usr headers via -isystem.
# The vendor clean_headers.sh needs bionic's hermetic python+libclang (broken on
# this Arch host's system python3.14), so we instead strip only the specific
# struct definitions that A16 bionic also provides and that therefore collide
# ("redefinition of 'sigaction'"). Brace-matched removal; leaves everything else.
import sys, os, re

# Only 'struct sigaction' collides: A16 bionic <signal.h> defines it too.
# Do NOT strip sigaltstack/stack_t — bionic USES stack_t but relies on the kernel
# header to define it (stripping it -> "unknown type name 'stack_t'"). This matches
# bionic's own cleaned asm-generic/signal.h, which keeps sigaltstack and drops only sigaction.
STRUCTS = ("sigaction",)

def strip_struct(text, name):
    # match optional leading 'typedef ' + 'struct NAME {' so both
    #   struct NAME { ... };                 (plain struct)
    #   typedef struct NAME { ... } Y;       (struct+typedef)
    # are removed whole.
    pat = re.compile(r'(?:\btypedef\s+)?\bstruct\s+' + re.escape(name) + r'\s*\{')
    m = pat.search(text)
    if not m:
        return text, False
    # brace-match from the opening '{'
    i = text.index('{', m.start())
    depth = 0
    j = i
    while j < len(text):
        c = text[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                break
        j += 1
    # consume everything up to AND including the next ';' after the closing '}'
    # (covers '};' for plain structs and '} typename;' for typedefs)
    k = j + 1
    while k < len(text) and text[k] != ';':
        k += 1
    if k < len(text):
        k += 1
    return text[:m.start()] + text[k:], True

def process(path):
    if not os.path.isfile(path):
        return
    with open(path, 'r', errors='replace') as f:
        text = f.read()
    changed = False
    for s in STRUCTS:
        text, did = strip_struct(text, s)
        changed = changed or did
    if changed:
        with open(path, 'w') as f:
            f.write(text)
        print("stripped conflicting structs from " + path)

if __name__ == '__main__':
    base = sys.argv[1] if len(sys.argv) > 1 else '.'
    inc = os.path.join(base, 'usr', 'include')
    for rel in ('asm/signal.h', 'asm-generic/signal.h', 'linux/elfcore.h'):
        process(os.path.join(inc, rel))
