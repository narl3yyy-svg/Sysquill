# Sysquill

A modern, live system monitor ("task manager") for Linux, made for KDE Plasma.
It is a native Qt 6 / QML app, so it uses your Plasma theme, colours, fonts and
dark mode automatically.

Everything updates live (every second by default; 0.5 s, 2 s and 5 s are one click away).

## Features

| Page | What you see |
|---|---|
| **Overview** | CPU, memory, GPU and network at a glance, every drive's free space, the busiest processes, host / OS / kernel / uptime |
| **Processes** | Sortable, searchable table: name, PID, user, CPU %, memory, disk read/s, disk write/s, GPU %, threads, state, full command line. Busy cells are tinted. **End process** (polite) and **Force kill**, from the buttons, a right-click menu or the Delete key |
| **CPU** | Total usage graph, per-thread graphs with current clock, average and max speed, package temperature, package power (when unlocked), load average, process/thread count |
| **Memory** | Usage graph, in-use / cache / free bar, available, cached, buffers, shared, dirty; swap usage |
| **Disks** | Every mounted drive: label, mount points, device and parent disk, file system, UUID, model, HDD/SSD, used / free / size bar, live read and write throughput with graph, active time %, and **which processes have files open on that drive** |
| **Network** | Every interface: live download/upload graph, totals, link speed, state, MAC and IP addresses (virtual Docker/bridge/VPN interfaces can be shown with one tick) |
| **GPU** | Intel (i915 and xe), AMD and other kernel-driver GPUs: busy %, clock, max clock, per-engine load (**Render/3D, Video decode/encode, Video enhance, Copy**), which programs use the GPU, and iGPU power when unlocked. Hardware video decoding and encoding (VA-API, Quick Sync) shows up under the *Video* engine |
| **Sensors** | All temperature sensors (CPU cores and package, NVMe, chipset, motherboard, Wi-Fi, and hard drives with `drivetemp`) |

### "Unlock all processes"

Linux only lets a normal user see disk and GPU activity of their **own** processes.
System services (web and media servers, Docker, Samba and so on) run as other users, so for them those
columns show "–" at first.

Click **Unlock all processes** in the top bar and enter your password once. A tiny
helper (`/usr/libexec/sysquill/sysquill-helper`) then runs as root for as long as
Sysquill is open. After that you see everyone's disk I/O, open files per drive, GPU use,
and CPU / iGPU power, and you can end processes that belong to other users. The helper only
reads `/proc` and `/sys` and can only send *end / kill / stop / continue* signals. It quits
when Sysquill closes. The password prompt is the normal KDE (polkit) dialog.

Ending another user's process without unlocking first will also bring up the password prompt.

## Install on Kubuntu / Ubuntu (26.04 or newer)

Open a terminal (Konsole) and run these one at a time.

1. Install the tools needed to build it (this asks for your password):

   ```bash
   sudo apt install git build-essential cmake dpkg-dev file qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml-workerscript
   ```

2. Download the source:

   ```bash
   git clone https://github.com/narl3yyy-svg/Sysquill.git
   ```

3. Build the package (takes a minute or two):

   ```bash
   cd Sysquill && ./scripts/build-deb.sh
   ```

4. Install it:

   ```bash
   sudo apt install ./build/sysquill_0.1.0_amd64.deb
   ```

Sysquill now appears in the application launcher under **System → Sysquill**
(or search for "Sysquill" or "task manager"). You can also start it from a terminal with `sysquill`.

### Updating later

```bash
cd ~/Sysquill && git pull && ./scripts/build-deb.sh && sudo apt install ./build/sysquill_*.deb
```

### Uninstalling

```bash
sudo apt remove sysquill
```

### Ready-made .deb

Every push to GitHub builds the package automatically (Actions tab → latest
*Build .deb* run → *sysquill-deb* artifact). Tagged releases (`v0.1.0`, …) attach the
`.deb` to the release page, so you can skip the build steps and run just step 4.

## Command-line options

```text
sysquill --page disks        open on a page (overview, processes, cpu, memory, disks, network, gpu, sensors)
sysquill --unlock            ask for administrator access right away
sysquill --dump              print everything it measures as JSON and quit (handy for bug reports)
```

## Tips

* **Hard-drive temperatures**: load the kernel's drive temperature driver once with
  `sudo modprobe drivetemp`, and make it permanent with
  `echo drivetemp | sudo tee /etc/modules-load.d/drivetemp.conf`.
* **CPU %** in the process list is a share of the whole machine (like Windows Task
  Manager), so 100 % means every core is busy.
* **Disk read/write per process** counts all drives together. The *Disks* page tells you
  which drive a process has files open on.

## How it works

* C++17 + Qt 6 (Qt Quick / QML), built with CMake, packaged with CPack.
* All measuring happens on a background thread that reads `/proc` and `/sys` directly:
  `/proc/stat`, `/proc/meminfo`, `/proc/<pid>/{stat,io,fd,fdinfo}`, `/proc/diskstats`,
  `/proc/self/mountinfo`, `/proc/net/dev`, `/sys/class/hwmon`, `/sys/class/drm`,
  `/sys/class/powercap`.
* GPU engine load is computed from the kernel's per-client DRM statistics in
  `/proc/<pid>/fdinfo` (the same source `nvtop` and `gputop` use), so no extra
  tools or permissions are needed for your own programs.
* The UI uses the KDE desktop style (`qml6-module-org-kde-desktop`) when it is installed,
  which it is on every Plasma desktop, and falls back to Qt's Fusion style elsewhere.

## Building by hand (developers)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j"$(nproc)"
./build/sysquill               # run without installing
sudo cmake --install build     # or install without a package
```

## License

GNU General Public License v3.0 or later, see [LICENSE](LICENSE).
