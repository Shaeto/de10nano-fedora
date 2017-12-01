# de10nano-fedora

## Automate Media Creation for DE0 and DE10 Nano SoC Fedora ARM

```bash
Usage: create_de10nano_fedora.sh <options>
   --workspace=PATH - base path for working directories (by default = script path)
   --cleanup        - remove all temporary data from workspace folder and exit
   --image=IMAGE    - raw image file name
   --media=DEVICE   - media device file (/dev/[sdX|mmcblkX])
   --preempt-rt     - build real-time Linux kernel (CONFIG_PREEMPT_RT)
   --init-hdmi      - init ADV7513 HDMI bridge and show startup logo
   --soc-rbf=RBF    - path to fpga bitstream file
   --soc-dtb=DTB    - path to soc system dtb file
   --selinux=ON/OFF - Turn SELinux off/on as needed
   --norootpass     - Remove the root password
   -y               - Assumes yes, will not wait for confirmation
   --version        - Display version and exit
   --resizefs       - Resize root filesystem to fill media device
   --addkey=        - /path/to/ssh-public-key

Examples:
    sudo ./create_de10nano_fedora.sh --image=Fedora-Rawhide.raw --media=/dev/mmcblk0

Remove all temporary files:
    sudo ./create_de10nano_fedora.sh --cleanup
```

Warning: this script requires root permissions and a lot of free space (aproximately 2 * sizeof(source image) + 2Gb for compiled kernel and u-boot)

This script is making possible to:

* boot Fedora ARM on Terasic DE0 and DE10 Nano Cyclone V SoC boards using original Linux Kernel 4.13.13 (optionally with RT_PREEMPT real linux patch)
* optionally initialize Cyclone V HPS using user specific programming and dtb files
* optionally initialize DE10 Nano ADV7513 HDMI bridge and provide fbdev driver for altera frame reader 1.0/2.0 (based on original altera driver)
* write result image to file or sdcard device

## How to use

1. download [Fedora ARM disk image](https://arm.fedoraproject.org)

```bash
  wget -L "https://download.fedoraproject.org/pub/fedora/linux/releases/27/Spins/armhfp/images/Fedora-Minimal-armhfp-27-1.6-sda.raw.xz"
```

2. unxz image

```bash
  xz -d -c Fedora-Minimal-armhfp-27-1.6-sda.raw.xz > Fedora-Minimal-armhfp-27-1.6-sda.raw
```

3. develop and build cyclone v fpga project using altera quartus
4. build sdcard image

### Example 1

```bash
sudo ./create_de10nano_fedora.sh --image=fedora/Fedora-Minimal-armhfp-27-1.6-sda.raw --media=fedora-27-1.6-de10nano.img
```

### Example 2

```bash
sudo ./create_de10nano_fedora.sh --image=fedora/Fedora-Minimal-armhfp-27-1.6-sda.raw --media=fedora-27-1.6-de10nano.img \
    --soc-rbf=/home/fpga/myproject/hello_world.rbf \
    --soc-dtb=/home/fpga/myproject/hello_workd.dtb
```

### Example 3 for de10 nano + real time linux + [Hostmot2 FPGA](https://github.com/machinekit/mksocfpga) for [MachineKit.io](http://www.machinekit.io)

```bash
sudo ./create_de10nano_fedora.sh --image=fedora/Fedora-Minimal-armhfp-27-1.6-sda.raw --media=fedora-27-1.6-de10nano.img --init-hdmi --selinux=off \
    --soc-rbf=/home/fpga/myproject/DE10_Nano_SoC_DB25.7I76_7I76_7I76_7I76.rbf \
    --soc-dtb=/home/fpga/myproject/soc_system.dtb \
    --preempt-rt
```

## How it works
