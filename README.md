# Jetson Orin Nano Camera Driver Development

Hands-on driver development labs for the NVIDIA Jetson Orin Nano, adapted from [Bootlin's Linux Kernel and Driver Development training](https://bootlin.com/training/kernel/) (originally for BeagleBone Black).

The end goal is to develop an IMX477 camera driver using the Tegra CSI/VI pipeline and V4L2 subsystem. This repo documents the full learning path from first kernel build to camera driver, including every platform-specific adaptation and gotcha encountered along the way.

## Hardware

- **Target**: NVIDIA Jetson Orin Nano Super Developer Kit (p3768-0000 carrier + p3767-0005 module)
- **Host**: Ubuntu 22.04 AMD64 machine (cross-compilation)
- **Peripherals**: Wii nunchuk (I2C joystick), Raspberry Pi IMX477 camera (future)
- **Storage**: SD card (mmcblk0p1)

## Platform: JetPack 6.2.1 / L4T R36.4.4

Pinned to R36.4.4 for driver compatibility.

> **WARNING**: After flashing, do NOT run `sudo apt upgrade` without first pinning NVIDIA packages. An unguarded `apt upgrade` will silently bump the L4T version (e.g. R36.4.4 to R36.4.7), breaking module compatibility with kernel sources. See the Flashing section below.

## Flashing the Jetson

### Downloads

From the [Jetson Linux R36.4.4 page](https://developer.nvidia.com/embedded/jetson-linux-r3644), grab two files:

1. **Driver Package (BSP)** — `Jetson_Linux_r36.4.4_aarch64.tbz2` (~712MB)  
   Contains the kernel Image, UEFI bootloader, OOT modules, device tree blobs, flashing tools, bootloader firmware (MB1/MB2/BPMP/TOS), and NVIDIA userspace drivers. Everything except the Ubuntu root filesystem and source code.

2. **Sample Root Filesystem** — `Tegra_Linux_Sample-Root-Filesystem_R36.4.4_aarch64.tbz2` (~1.5GB)  
   A pre-built Ubuntu 22.04 root filesystem. This extracts as a flat filesystem (`bin/`, `etc/`, `usr/`, etc.), not a structured archive — extract it inside `Linux_for_Tegra/rootfs/`.

You do NOT need "Root Filesystem Sources" (for rebuilding Ubuntu from scratch) or "BSP Sources" (for kernel/driver development — covered separately below).

The BSP and rootfs are separate archives because NVIDIA designed the system so you can swap in any root filesystem (minimal Ubuntu, Yocto, custom distro) and layer their proprietary bits on top. The rootfs is Ubuntu under Canonical's license, NVIDIA's drivers are under NVIDIA's license.

### Flashing steps

```bash
cd ~/jetson-dev

# Extract BSP (creates Linux_for_Tegra/ directory)
tar xf Jetson_Linux_r36.4.4_aarch64.tbz2

# Extract rootfs INTO the BSP directory structure
# The 'p' flag preserves file permissions — without it the Jetson won't boot
cd Linux_for_Tegra/rootfs/
sudo tar xpf ../../Tegra_Linux_Sample-Root-Filesystem_R36.4.4_aarch64.tbz2
cd ..

# Install flash prerequisites on the host (libxml2-utils, simg2img, etc.)
sudo ./tools/l4t_flash_prerequisites.sh

# Inject NVIDIA drivers/firmware into the rootfs
# Without this step you'd boot into plain Ubuntu with no GPU/camera/display support
sudo ./apply_binaries.sh
```

### Put the Jetson in recovery mode

1. Power off the Jetson
2. Short the FC_REC pin to GND on the header under the module (use a jumper)
3. Power on
4. Remove the jumper
5. Connect USB-C from Jetson to Ubuntu host
6. Verify: `lsusb | grep -i nvidia` — should show "NVIDIA Corp. APX"

### Flash command (Orin Nano Super, SD card)

```bash
sudo ./tools/kernel_flash/l4t_initrd_flash.sh --external-device mmcblk0p1 \
  -c tools/kernel_flash/flash_l4t_t234_nvme.xml \
  -p "-c bootloader/generic/cfg/flash_t234_qspi.xml" \
  --showlogs --network usb0 jetson-orin-nano-devkit-super internal
```

Takes 10–20 minutes. After boot, complete OEM setup.

### CRITICAL: Pin NVIDIA packages immediately after first boot

```bash
sudo apt-mark hold nvidia-l4t-*
sudo apt update && sudo apt upgrade -y
```

Verify:

```bash
cat /etc/nv_tegra_release | head -1
# Should show: R36 (release), REVISION: 4.4
```

### Post-flash setup

If running headless (SSH only), disable the desktop to save ~1GB RAM:

```bash
sudo systemctl stop gdm
sudo systemctl disable gdm
```

For passwordless sudo during development, create a file in `/etc/sudoers.d/` (processed last, so it overrides the `%sudo` group rule):

```bash
sudo visudo -f /etc/sudoers.d/dev-nopasswd
# Add: your-username ALL=(ALL) NOPASSWD: ALL
```

## Cross-Compilation Environment

### Toolchain

The Bootlin aarch64 toolchain (GCC 11.3.0, 2022.08 release) works with JetPack 6:

```bash
export CROSS_COMPILE=$HOME/jetson-toolchain/aarch64--glibc--stable-2022.08-1/bin/aarch64-buildroot-linux-gnu-
export ARCH=arm64
```

Save these in a file to source each session:

```bash
cat > ~/jetson-env.sh << 'EOF'
export CROSS_COMPILE=$HOME/jetson-toolchain/aarch64--glibc--stable-2022.08-1/bin/aarch64-buildroot-linux-gnu-
export ARCH=arm64
EOF
```

### Kernel sources

The BSP's `source/` directory is nearly empty after extraction. Sync the actual source from NVIDIA's git:

```bash
cd ~/jetson-dev/Linux_for_Tegra/source
./source_sync.sh -k -t jetson_36.4.4
```

This clones the base kernel (`kernel-jammy-src`), NVIDIA OOT modules (`nvidia-oot`), device trees, GPU driver, display driver, and others.

### Building the kernel

```bash
cd ~/jetson-dev/Linux_for_Tegra/source
make -C kernel
```

The built kernel image lands at `kernel/kernel-jammy-src/arch/arm64/boot/Image`.

### Source layout

JetPack 6 has a three-tree architecture:

- **kernel/kernel-jammy-src/** — base Linux 5.15 kernel (Canonical's Ubuntu kernel)
- **nvidia-oot/** — NVIDIA's out-of-tree modules (GPU, camera, display, sound)
- **nvdisplay/** — display driver (separate build)

NVIDIA moved their drivers out-of-tree in JetPack 6 so they can ship driver updates without rebuilding the kernel, and keep the base kernel closer to upstream. Most NVIDIA modules show `(O)` (out-of-tree) in `lsmod`.

## Lab 2: Hello Module

A minimal kernel module that prints the running kernel version on load and accepts a `who` parameter.

### Makefile for out-of-tree cross-compilation

```makefile
ifneq ($(KERNELRELEASE),)
obj-m := hello_version.o
else
KDIR := $(HOME)/jetson-dev/Linux_for_Tegra/source/kernel/kernel-jammy-src

all:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD)

clean:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean
endif
```

### Build, deploy, test

```bash
make
scp hello_version.ko jetson-orin:~
ssh jetson-orin "sudo insmod ~/hello_version.ko"
ssh jetson-orin "sudo dmesg | tail -3"
# [ 4954.802140] Hello World. You are currently using Linux 5.15.148-tegra.
```

The "module verification failed" warning is normal — the module isn't signed with NVIDIA's key. The kernel taints itself but everything works fine.

Test with parameters:

```bash
ssh jetson-orin "sudo rmmod hello_version"
ssh jetson-orin "sudo insmod ~/hello_version.ko who=Nick"
ssh jetson-orin "sudo dmesg | tail -3"
# Hello Nick. You are currently using Linux 5.15.148-tegra.
```

`/proc/modules` is the raw data source that `lsmod` formats — they show the same info.

## Lab 3: nunchuk I2C Driver

### I2C on the 40-pin header

| Pin | Function | I2C Bus | Controller |
|-----|----------|---------|------------|
| 3 | SDA | i2c-7 | gen8_i2c (i2c@c250000) |
| 5 | SCL | i2c-7 | gen8_i2c (i2c@c250000) |
| 27 | SDA | i2c-1 | gen2_i2c (i2c@c240000) |
| 28 | SCL | i2c-1 | gen2_i2c (i2c@c240000) |

Power: Pin 1 (3.3V), Pin 6 (GND). The nunchuk runs at 3.3V — fully compatible.

### Wiring

| nunchuk | Jetson 40-pin |
|----------|---------------|
| + (PWR) | Pin 1 (3.3V) |
| - (GND) | Pin 6 (GND) |
| d (SDA) | Pin 3 |
| c (SCL) | Pin 5 |

Verify the connection:

```bash
$ i2cdetect -r 7
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
...
50: -- -- 52 -- -- -- -- -- -- -- -- -- -- -- -- --
...
```

### I2C buses on the system

```
i2c-0  3160000.i2c     — on-board peripherals
i2c-1  c240000.i2c     — 40-pin header pins 27/28
i2c-2  3180000.i2c     — camera CSI connector, PMICs
i2c-4  Tegra BPMP I2C  — virtual bus proxied through BPMP co-processor
i2c-5  31b0000.i2c     — internal peripherals
i2c-7  c250000.i2c     — 40-pin header pins 3/5 (the nunchuk)
i2c-9  NVIDIA SOC i2c  — virtual, display/GPU stack
```

The hex addresses are memory-mapped register base addresses for each I2C controller. The BPMP I2C bus is "virtual" — the main CPU sends IPC messages to the BPMP co-processor, which does the actual I2C transactions. From a driver's perspective it's a normal I2C bus (same API), just a different transport underneath.

### Tracing pins through the device tree

Trace the 40-pin header pins back to the I2C controller by grepping through the DTS source files.

**SoC level** (`tegra234.dtsi`) — defines the hardware, disabled by default:

```dts
gen8_i2c: i2c@c250000 {
    compatible = "nvidia,tegra194-i2c";
    reg = <0xc250000 0x100>;
    status = "disabled";
    clock-frequency = <400000>;
    ...
};
```

**Board level** (`tegra234-p3768-0000+p3767-xxxx-nv-common.dtsi`) — enables it:

```dts
hdr40_i2c1: i2c@c250000 {
    status = "okay";
};
```

**Pinmux overlay** (`tegra234-p3767-0000-common-hdr40.dtsi`) — routes SoC signals to physical header pins:

```dts
hdr40-pin3 {
    nvidia,pins = "gen8_i2c_sda_pdd2";
    nvidia,pin-label = "i2c8";
};
hdr40-pin5 {
    nvidia,pins = "gen8_i2c_scl_pdd1";
    nvidia,pin-label = "i2c8";
};
```

The naming convention `gen8_i2c_sda_pdd2` packs the function (`gen8_i2c_sda`) and the physical pin (`pdd2`) together. The `pin-label` is informational (used by `jetson-io.py`), not functional.

Same pattern as the BBB labs (SoC defines hardware → board enables it → pinmux routes signals), but pre-configured by NVIDIA on the Jetson.

### Pinmux

A SoC has hundreds of internal signals but limited physical pins. Most pins can serve multiple functions (I2C, SPI, GPIO, etc.). The pinmux (pin multiplexer) hardware selects which internal signal gets routed to which physical pin. The `nvidia,pins` property tells the pinmux driver which pad to configure and for which function.

### Device Tree Overlay

```dts
/dts-v1/;
/plugin/;

/ {
    overlay-name = "nunchuk I2C Device";
    compatible = "nvidia,p3768-0000+p3767-0005-super",
                 "nvidia,p3768-0000+p3767-0005",
                 "nvidia,tegra234";

    fragment@0 {
        target = <&gen8_i2c>;
        __overlay__ {
            #address-cells = <1>;
            #size-cells = <0>;

            joystick@52 {
                compatible = "nintendo,nunchuk";
                reg = <0x52>;
            };
        };
    };
};
```

Notes:
- `/plugin/;` marks this as an overlay, not a standalone DTS
- Each `fragment` is one modification to the base DT. The `@0` is an index; use `fragment@1`, `@2` etc. for multiple changes.
- `target = <&gen8_i2c>` — find the node with this label in the base DTB and merge the `__overlay__` contents into it
- Root-level `compatible` is a board filter that tells the bootloader when to apply this overlay. Find your board's strings with `cat /sys/firmware/devicetree/base/compatible | tr '\0' '\n'`
- `compatible` inside `joystick@52` is the driver match string
- `#size-cells = <0>` because I2C devices have a single address, not an address range

### Compiling and deploying the overlay

```bash
# Compile (-@ preserves label info needed for overlay resolution)
dtc -@ -I dts -O dtb -o nunchuk-overlay.dtbo nunchuk-overlay.dts

# Deploy
scp nunchuk-overlay.dtbo jetson-orin:~
ssh jetson-orin "sudo cp ~/nunchuk-overlay.dtbo /boot/"
```

### Boot configuration

To load overlays, you MUST add an explicit `FDT` line to `/boot/extlinux/extlinux.conf`. Without it, the bootloader uses the device tree from QSPI flash (baked in during flashing) and ignores `OVERLAYS`.

```
LABEL primary
      MENU LABEL primary kernel
      LINUX /boot/Image
      FDT /boot/dtb/kernel_tegra234-p3768-0000+p3767-0005-nv-super.dtb
      INITRD /boot/initrd
      APPEND ${cbootargs} root=PARTUUID=... rw rootwait rootfstype=ext4 ...
      OVERLAYS /boot/nunchuk-overlay.dtbo
```

Find your DTB filename with `ls /boot/dtb/`.

The system has two copies of the device tree: one in QSPI flash (safe, always there, used by default) and one in `/boot/dtb/` (editable, for development). The `FDT` line switches the bootloader to use the filesystem copy, which enables overlay processing.

### Verify the overlay loaded

```bash
$ find /sys/firmware/devicetree/base -name "*joystick*"
/sys/firmware/devicetree/base/bus@0/i2c@c250000/joystick@52
```

Two sysfs entries appear:
- `/sys/firmware/devicetree/base/.../joystick@52` — raw device tree dump (read-only mirror of the DTB as the bootloader passed it)
- `/sys/bus/i2c/devices/7-0052` — live kernel device object (`struct i2c_client`) created by the I2C subsystem after parsing the DT node. This is what drivers bind to.

### I2C API version difference

Linux 5.15 (L4T R36.4.4) uses the older probe/remove signatures:

```c
static int nunchuk_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
static int nunchuk_remove(struct i2c_client *client)
```

Newer kernels (6.x+) dropped the second probe argument and changed remove to return void. Check your kernel's expected signature with:

```bash
grep -A 20 "struct i2c_driver {" include/linux/i2c.h
```

### nunchuk I2C protocol

**Init**: send `0xf0 0x55`, then `0xfb 0x00` (selects unencrypted mode).

**Read**: send `0x00`, wait 10ms, read 6 bytes.

**Data format** (6 bytes):
- byte 0: joystick X (0–255, center ~128)
- byte 1: joystick Y (0–255, center ~128)
- bytes 2–4: accelerometer data
- byte 5: bit 0 = Z button (active low), bit 1 = C button (active low)

### Testing

```bash
$ ssh jetson-orin "sudo insmod ~/nunchuk.ko && sudo dmesg | tail -3"
nunchuk: module verification failed: signature and/or required key missing - tainting kernel
nunchuk 7-0052: joystick: x=128 y=128 | buttons: z=0 c=0
```

If you get error -121 (EREMOTEIO) on init, the nunchuk is in a bad state from a previous partial init. Unplug it, wait 2 seconds, plug it back in, verify with `i2cdetect -r 7`, then retry.

### Driver sysfs entries

When the driver binds to the device, the kernel automatically creates:

```
/sys/bus/i2c/drivers/nunchuk/
├── 7-0052     → symlink to bound device
├── bind       → write device name to manually bind
├── unbind     → write device name to manually unbind (without rmmod)
├── module     → symlink to /sys/module/nunchuk/
└── uevent     → triggers hotplug event
```

## Lab 4: Input Subsystem

Extend the nunchuk driver to expose buttons and joystick axes to userspace through the Linux input subsystem.

### Prerequisites

Check that the input event interface and joystick device support are enabled:

```bash
zcat /proc/config.gz | grep CONFIG_INPUT_EVDEV
zcat /proc/config.gz | grep CONFIG_INPUT_JOYDEV
```

Both should show `=y` or `=m`. JetPack 6 enables them by default.

Install `evtest` for testing:

```bash
sudo apt install -y evtest
```

### Architecture

The input subsystem has two halves: **device drivers** (talk to hardware, generate events) and **event handlers** (deliver events to userspace via `/dev/input/eventX`). The main event handler is `evdev`, enabled by `CONFIG_INPUT_EVDEV`.

Userspace reads events through standard file operations (`open`, `select`, `read`) on `/dev/input/eventX`. Each read returns a `struct input_event` with a type, code, and value. `evtest` is a simple tool that does exactly this — opens the fd, waits in `select()`, reads events, prints them.

### The three-struct pattern

The polling function receives an `input_dev` (logical device) but needs the `i2c_client` (physical device) to read hardware registers. A private structure bridges them:

```
Physical hardware ← i2c_client ← nunchuk_dev → input_dev → /dev/input/eventX → userspace
```

- `struct i2c_client` — physical I2C device, created by the kernel from the device tree
- `struct input_dev` — logical input device, registered with the input subsystem
- `struct nunchuk_dev` — private glue struct holding an `i2c_client` pointer, attached to the `input_dev` with `input_set_drvdata()` and retrieved in the poll function with `input_get_drvdata()`

This is the same pattern used in every kernel framework (IIO, V4L2, ALSA, etc.) — only the struct names change.

### Event types and codes

Events are organized in two levels:

- **Type** — category of event: `EV_KEY` (buttons/keys), `EV_ABS` (absolute axes), `EV_REL` (relative axes like a mouse)
- **Code** — specific event within that type: `BTN_Z`, `BTN_C`, `ABS_X`, `ABS_Y`

Declare capabilities in probe with `set_bit()`, report events in the poll function with `input_report_key()` / `input_report_abs()`, and finalize each event packet with `input_sync()`.

For buttons, the input subsystem deduplicates — reporting the same state repeatedly only delivers the first change to userspace. For axes, every report is delivered.

### Polling

The nunchuk has no interrupt pin, so polling is the only option. `input_setup_polling()` registers a poll function that the kernel calls on a timer. The CPU sleeps between polls — no busy-waiting.

```c
input_setup_polling(input, nunchuk_poll);
input_set_poll_interval(input, 50);  /* milliseconds */
```

At 50ms intervals with ~1ms of actual I2C work per poll, CPU usage is negligible.

Devices with an interrupt line (like most real sensors) skip polling entirely — the hardware triggers an IRQ, the driver runs once, done.

### Joystick recognition

Linux's `joydev` subsystem decides whether a device is a joystick based on which button codes it declares. To get `/dev/input/jsX` created, declare standard gamepad buttons even if the device doesn't physically have them:

```c
set_bit(BTN_TL, input->keybit);
set_bit(BTN_SELECT, input->keybit);
/* ... etc */
```

These are never reported — they just tell the kernel to classify the device as a joystick.

### Testing

```bash
$ evtest
Available devices:
...
/dev/input/event11:	nunchuk
Select the device event number [0-11]: 11
...
Event: time ..., type 1 (EV_KEY), code 306 (BTN_C), value 1
Event: time ..., -------------- SYN_REPORT ------------
Event: time ..., type 3 (EV_ABS), code 0 (ABS_X), value 254
Event: time ..., type 3 (EV_ABS), code 1 (ABS_Y), value 238
Event: time ..., -------------- SYN_REPORT ------------
```

Verify joystick device was created:

```bash
ls /dev/input/js0
```

### Choosing the right framework

The input subsystem is for human interface devices. Other device types use different frameworks:

| Device type | Framework | Userspace interface |
|-------------|-----------|-------------------|
| Buttons, joysticks, touchscreens | Input | `/dev/input/eventX` |
| Raw sensors (IMU, ADC, temperature) | IIO | `/dev/iio:deviceX` |
| Cameras, video | V4L2 | `/dev/videoX` |
| Audio | ALSA | `/dev/snd/*` |
| Network | netdev | `ethX`, `wlanX` |

The driver patterns are identical across frameworks — same three-struct pattern, same probe/remove lifecycle, same device tree binding. Only the framework-specific API changes.

## Useful commands

```bash
# Check L4T version
cat /etc/nv_tegra_release

# Check kernel version
uname -r

# List loaded modules
lsmod
cat /proc/modules

# I2C
i2cdetect -l                    # list buses
i2cdetect -r 7                  # scan bus 7

# Input devices
evtest                          # list and test input devices
ls /dev/input/                  # see eventX and jsX devices

# Kernel config
zcat /proc/config.gz | grep PATTERN

# Device tree
find /sys/firmware/devicetree/base -name "*pattern*"
dtc -I fs /sys/firmware/devicetree/base/ > /tmp/dts
cat /sys/firmware/devicetree/base/compatible | tr '\0' '\n'

# Searching kernel source
rg "pattern" path/                           # ripgrep (fast)
rg -F "struct i2c_driver {" include/linux/   # -F for fixed strings (no regex)
rg -A 15 "node_name" file.dtsi              # -A shows lines after match
grep -rn "pattern" path/                     # grep (always available)
```

## References

- [Bootlin Linux Kernel Training](https://bootlin.com/training/kernel/)
- [Jetson Linux R36.4.4 Developer Guide](https://docs.nvidia.com/jetson/archives/r36.4.4/DeveloperGuide/index.html)
- [Quick Start (Flashing)](https://docs.nvidia.com/jetson/archives/r36.4.4/DeveloperGuide/IN/QuickStart.html)
- [Working With Sources](https://docs.nvidia.com/jetson/archives/r36.4.4/DeveloperGuide/SD/WorkingWithSources.html)
- [Kernel Customization](https://docs.nvidia.com/jetson/archives/r36.4.4/DeveloperGuide/SD/Kernel/KernelCustomization.html)
- [JetsonHacks Kernel Builder](https://github.com/jetsonhacks/jetson-orin-kernel-builder) (reference for source layout)
