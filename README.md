<div align="center">

<img src="data/io.github.narl3yyy_svg.Sysquill.svg" alt="Sysquill" width="128" height="128">

# Sysquill

**A fast, modern system monitor for Linux, built for KDE Plasma.**

[![Build](https://github.com/narl3yyy-svg/Sysquill/actions/workflows/build.yml/badge.svg)](https://github.com/narl3yyy-svg/Sysquill/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/narl3yyy-svg/Sysquill?label=release&cacheSeconds=3600)](https://github.com/narl3yyy-svg/Sysquill/releases/latest)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)](LICENSE)
![Qt 6](https://img.shields.io/badge/Qt-6-41cd52?logo=qt&logoColor=white)

[Features](#features) · [Install](#install) · [Usage](#usage) · [How it works](#how-it-works) · [Building](#building-from-source)

</div>

---

Sysquill shows everything your machine is doing in one live view: CPU, memory, every
drive, network, GPU, temperatures and processes. It is a native Qt 6 / QML application,
so it follows your Plasma theme, accent colour, fonts and dark mode automatically, and it
reads `/proc` and `/sys` directly, with no background daemon and no extra tools.

## Features

| Page | What it shows |
|---|---|
| **Overview** | CPU, memory, GPU and network at a glance, free space on every drive, the busiest processes, and host, OS, kernel and uptime. |
| **Processes** | Sortable, searchable table with CPU, memory, disk read/write, GPU, threads, state and full command line. End or force-kill from the toolbar, the context menu or the <kbd>Delete</kbd> key. |
| **CPU** | Total and per-thread usage graphs, clock speeds, package temperature and power, load average, and process and thread counts. |
| **Memory** | Usage history, an in-use / cache / free breakdown, and swap. |
| **Disks** | Every mounted drive with label, UUID, file system, model, HDD/SSD type, capacity, live throughput, active time, and **which processes have files open on it**. |
| **Network** | Per-interface traffic graphs, totals, link speed, MAC and IP addresses. Virtual interfaces (Docker, bridges, VPNs) are hidden unless you ask for them. |
| **GPU** | Intel (i915 / xe), AMD and other DRM GPUs: busy %, clocks, per-engine load (3D, video decode/encode, video enhance, copy), per-process GPU use and iGPU power. |
| **Sensors** | All hardware temperatures: CPU, NVMe, chipset, motherboard, Wi-Fi, and hard drives via `drivetemp`. |

Refresh every 0.5, 1, 2 or 5 seconds, or pause at any time.

### Unlock all processes

Linux only lets a normal user see disk and GPU activity of their **own** processes, so
system services show "–" in those columns at first. Click **Unlock all processes** and
authenticate once through the standard polkit dialog. A small helper then runs as root
until Sysquill closes, and you get:

- disk I/O, open files and GPU use for every process
- CPU and iGPU power readings
- the ability to end processes owned by other users

The helper (`/usr/libexec/sysquill/sysquill-helper`) only reads `/proc` and `/sys` and can
only send end, kill, stop and continue signals.

## Install

Sysquill needs **Kubuntu / Ubuntu 26.04 or newer** (Qt 6.5+).

### From a release (recommended)

Download the `.deb` from the [latest release](https://github.com/narl3yyy-svg/Sysquill/releases/latest) and install it:

```bash
sudo apt install ./sysquill_*_amd64.deb
```

apt installs the Qt packages Sysquill needs automatically.

> [!NOTE]
> If apt ends with *"Download is performed unsandboxed as root … Permission denied"*, the
> install still succeeded. apt's download user just can't read your home folder.

### From source

```bash
git clone https://github.com/narl3yyy-svg/Sysquill.git
cd Sysquill
./scripts/build-deb.sh --install-deps
sudo apt install ./build/sysquill_*_amd64.deb
```

`--install-deps` installs the build tools with apt first; leave it out if you already have them.

**Update:**

```bash
cd ~/Sysquill && git pull && ./scripts/build-deb.sh && sudo apt install ./build/sysquill_*_amd64.deb
```

**Uninstall:**

```bash
sudo apt remove sysquill
```

## Usage

Open **Sysquill** from the application launcher (search "Sysquill" or "task manager"), or
from a terminal:

```bash
sysquill
```

| Option | Description |
|---|---|
| `--page <name>` | Open on a page: `overview`, `processes`, `cpu`, `memory`, `disks`, `network`, `gpu`, `sensors` |
| `--unlock` | Ask for administrator access at startup |
| `--dump` | Print every measurement as JSON and exit (useful for bug reports) |
| `--help` | Show all options |

### Tips

- **Hard-drive temperatures** need the `drivetemp` kernel module:
  ```bash
  echo drivetemp | sudo tee /etc/modules-load.d/drivetemp.conf && sudo modprobe drivetemp
  ```
- **CPU %** in the process list is a share of the whole machine, so 100 % means every core is busy.
- **Per-process disk I/O** counts all drives together. The *Disks* page shows which drive a process is using.

## How it works

- **C++17 and Qt 6 Quick**, built with CMake and packaged as a `.deb` with CPack.
- **A background sampler thread** reads the kernel directly: `/proc/stat`, `/proc/meminfo`,
  `/proc/<pid>/{stat,io,fd,fdinfo}`, `/proc/diskstats`, `/proc/self/mountinfo`,
  `/proc/net/dev`, and `/sys/class/{hwmon,drm,powercap}`.
- **GPU engine load** comes from the kernel's per-client DRM statistics in
  `/proc/<pid>/fdinfo`, the same source `nvtop` and `gputop` use.
- **Native look:** Sysquill uses the KDE desktop style when available and falls back to
  Qt's Fusion style elsewhere.

## Building from source

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j"$(nproc)"
./build/sysquill              # run without installing
```

Every push is built by GitHub Actions, and tagged versions (`v*`) publish the `.deb` to
[Releases](https://github.com/narl3yyy-svg/Sysquill/releases).

## License

Sysquill is free software, licensed under the [GNU General Public License v3.0 or later](LICENSE).
