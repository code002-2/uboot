# U-Boot for the Nubia Red Magic 11S Pro+ (NX809J)

Mainline U-Boot for the Nubia Red Magic 11S Pro+ (NX809J, Qualcomm SM8850
"Kaanapali"), run as an Android boot image chainloaded by ABL. The stock
bootloader chain is never modified.

This is a full-history fork of
[`infiniti-mainline/u-boot`](https://github.com/infiniti-mainline/u-boot), the
Kaanapali/SM8850 port written for the OnePlus 15 ("infiniti"): upstream U-Boot
plus the Kaanapali clock driver, UFS PHY/host support, the hyp-SMMU driver and
that board. Same silicon, so all of it applies here unchanged; what this fork
adds is the NX809J board — its devicetree, defconfig and environment.

Branch layout:

| Branch | What it is |
| --- | --- |
| `main` | the port: upstream + kaanapali SoC support + the NX809J board. Default. |
| `oneplus-15` | the same tip, under the name the upstream fork used. |
| `master` | untouched upstream U-Boot, the base of the port (`8dbe57ba648`). |

## Why U-Boot is the console

This board has no reachable console otherwise: no UART pads are brought out,
the stock bootargs say `qcom_geni_serial.con_enabled=0`, the USB gadget never
enumerates in a bring-up kernel, and a Linux DRM console needs MDSS plus its
power domains and NoC paths — which on this device means reprogramming a
display pipeline the bootloader is still scanning out.

ABL, however, leaves the DPU scanning out of the splash buffer at
`0xfc800000`. So U-Boot draws its console straight into that buffer with
`CONFIG_VIDEO_SIMPLE` and **no display driver at all**, and
`board/qualcomm/nx809j.env` points `stdout`/`stderr` at `vidconsole`. That is
the entire reason this port exists.

## Build

Standard upstream flow:

```sh
make qcom_nx809j_defconfig
make -j"$(nproc)" CROSS_COMPILE="ccache aarch64-linux-gnu-"
```

`CROSS_COMPILE` may contain spaces — that is U-Boot's own documented ccache
idiom (see `board/qualcomm/dragonboard820c/readme.txt`).

The result is packaged into an Android boot image by the build repo
(https://github.com/code002-2/nubia-nx809j-linux, `uboot/pack.sh`), which also
holds the CI, the boot_b staging image build and the initramfs. This fork is
built there from a pinned commit, exactly as pushed.

## What works today

* **Video console on the panel** — the U-Boot banner and the full bootcmd
  output appear on the screen, with no serial port and no display driver.
* **UFS read, GPT parsing** — `part number scsi N <name>` resolves partitions
  and `sysboot` reads a filesystem off a partition.
* **Booting Linux from a staging FAT** in the inactive `boot_b` slot, via
  `sysboot`/extlinux.
* The storage stack is **read-only by design**: `scsi_write()`/`scsi_erase()`
  are removed in `drivers/scsi/scsi.c` and their blk ops are never linked in,
  so `blk_write()`/`blk_erase()` return `-ENOSYS` at runtime. `CONFIG_CMD_SCSI`
  is enabled only for `scsi scan` / `scsi info` / `scsi read`.

## Things this port learned the hard way

Each of these cost a flash cycle and a screen reading, and each one fails
*silently* on a board whose only output is a panel.

**`scsi N` is not UFS LUN N.** `drivers/scsi/scsi.c` assigns each block
device's `devnum` automatically (`blk_create_devicef(..., -1, ...)`), one per
detected LUN, in LUN order — so if any LUN fails detection the numbering
shifts. `boot_b` is LUN 4 by the Android `sdX` naming and came up as
`scsi 5`. Never hardcode it: look the partition up per device and try every
device.

**The staging FAT must use 4096-byte sectors.** UFS LUNs report a 4096-byte
logical block size, and `fs/fat/fat.c` compares that against the FAT's sector
size. With `CONFIG_FS_FAT_HANDLE_SECTOR_SIZE_MISMATCH` off it only *logs* the
mismatch and then reads as if the sizes matched; with it on, the emulation
path is unreliable enough that a plain text `extlinux.conf` parsed with garbage
lines, and a 52 MB kernel image loaded through it crashed the moment it ran.
`CONFIG_FS_FAT_HANDLE_SECTOR_SIZE_MISMATCH=y` is set here as a safety net, but
the staging image is created with 4096-byte sectors (see the build repo's
`bootb/mkfat.sh`, which is not optional decoration).

**There is no hush shell.** `CONFIG_HUSH_PARSER`, `CMD_TEST`, `CMD_LS` and
`CMD_FAT` are off, so `bootcmd` is a linear list with no `if`/`test`/`ls`.
Fallbacks work by letting a failing command print its error and fall through —
and because `sysboot` does not return when it boots something, everything after
it is the failure path.

**The internal devicetree carries a `/memory` node**, unlike the OnePlus 15
port. If ABL ever starts U-Boot without a usable devicetree,
`board_fdt_blob_setup()` calls `panic("No valid memory ranges found!")` from
`fdtdec_setup()` — before the video console exists, i.e. a silent hang. The
ranges are ABL's own, read back from this device's `/sys/firmware/fdt`.

**A crash leaves no readable evidence**, because the vendor memory-dump screen
replaces the whole panel. The failure path therefore dumps the previous boot's
`ramoops@e2000000` console log (hex with an ASCII column from `md.b`) after
every `sysboot` attempt has returned, so the log of a crash the operator was
not watching is still there to read.

## NX809J specifics

* Panel: `bf375_rm692h5_6p8_magic_dsc_cmd`, 1216x2688 — the geometry the
  console assumes, taken from the panel's own devicetree node.
* RAM: 24 GiB in a fragmented map, with the bulk of it **above 4 GiB**
  (`0x880000000`-`0xc00000000`); ABL hands over roughly twenty usable ranges.
  U-Boot relocates near the top of that span and `LMB` allocates the load
  addresses from it.
* The splash carve-out is `0xfc800000 + 0x2b00000`.

## Provenance and licensing

Clock register data follows Linux's `drivers/clk/qcom/gcc-kaanapali.c` (see the
driver header); the UFS PHY headers are byte-identical imports from Linux with
their original license and copyright retained. The Kaanapali SoC support and
the OnePlus 15 board come from `infiniti-mainline/u-boot`.

Licensing follows upstream U-Boot (GPL-2.0+ with per-file SPDX).
