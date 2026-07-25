# U-Boot for the OnePlus 15 (kaanapali)

Mainline U-Boot for the OnePlus 15 (CPH2749, Qualcomm SM8850 "Kaanapali",
board name "infiniti"), run as an Android boot image chainloaded by ABL.
The stock bootloader chain is never modified.

Branch layout:

- `master` — untouched upstream U-Boot (the exact base of this port).
- `oneplus-15` — the board port: clock driver, UFS PHY/host support,
  board devicetree, and `qcom_infiniti_defconfig`. Default branch.

What works today: video console on the panel, UFS in read-only mode,
GPT parsing, and booting NixOS via the sysboot/extlinux path with a raw
initrd. **The storage stack is read-only by design** — write and erase
paths are compiled out because a bad storage write on this device is
unrecoverable.

Build: standard upstream flow —
`make qcom_infiniti_defconfig && make CROSS_COMPILE=aarch64-linux-gnu-`.

Provenance: clock register data follows Linux's
`drivers/clk/qcom/gcc-kaanapali.c` (see the driver header); the UFS PHY
headers are byte-identical imports from Linux v7.0 with their original
license and copyright retained. This port is developed alongside the
Linux tree at `infiniti-mainline/linux`.

Licensing follows upstream U-Boot (GPL-2.0+ with per-file SPDX).
See CONTRIBUTING.md before sending changes.
