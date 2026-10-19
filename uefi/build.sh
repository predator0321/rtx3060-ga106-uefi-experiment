#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail
uefi_root="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(dirname "$uefi_root")"
efi_include="${EFI_INCLUDE:-/usr/include/efi}"
efi_lib="${EFI_LIB:-/usr/lib}"
out="$repo_root/build/uefi"
mkdir -p "$out"
flags=(-std=gnu11 -O2 -fpic -ffreestanding -fno-stack-protector -fno-strict-aliasing
       -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -Wall -Wextra -Werror
       -Wno-unused-parameter -Wno-sign-compare -Wno-misleading-indentation
       -DEFI_FUNCTION_WRAPPER -D__KERNEL__
       -I "$efi_include" -I "$efi_include/x86_64"
       -I "$uefi_root/include" -I "$repo_root/kernel/gpu_unlock")
objects=()
for file in runtime policy hash; do
    gcc "${flags[@]}" -c "$uefi_root/$file.c" -o "$out/$file.o"
    objects+=("$out/$file.o")
done
core=()
for file in unlock_610 gfw_eval prep_610 layout_610 vbios_610; do
    gcc "${flags[@]}" -c "$repo_root/kernel/gpu_unlock/$file.c" -o "$out/$file.o"
    core+=("$out/$file.o")
done
build_one() {
    local name="$1"; shift
    gcc "${flags[@]}" "$@" -c "$uefi_root/entry.c" -o "$out/$name.o"
    ld -nostdlib -shared -Bsymbolic -z noexecstack -L "$efi_lib" -T "$efi_lib/elf_x86_64_efi.lds" \
        "$efi_lib/crt0-efi-x86_64.o" "$out/$name.o" "${objects[@]}" "${core[@]}" \
        -lefi -lgnuefi -o "$out/$name.so"
    if [[ -n "$(nm -u "$out/$name.so")" ]]; then
        echo "Unresolved symbols: refusing EFI output" >&2
        nm -u "$out/$name.so" >&2
        return 1
    fi
    objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
        -j '.rel*' -j '.rela*' -j .reloc -j '.rodata*' \
        --target=efi-app-x86_64 "$out/$name.so" "$out/$name.efi"
}
build_one GA106-PROBE -DGU_PROBE_ONLY
build_one GA106-UNLOCK
sha256sum "$out/GA106-PROBE.efi" "$out/GA106-UNLOCK.efi"
echo 'Built experimental EFI files only; no GPU or boot configuration was changed.'
