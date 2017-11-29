# de10nano-fedora
Automate Media Creation for DE10 Nano SoC Fedora ARM

```bash
Usage: create_de10nano_fedora.sh <options>
   --workspace=PATH - base path for working directories (by default = script path)
   --cleanup        - remove all temporary data from workspace folder and exit
   --image=IMAGE    - raw image file name
   --media=DEVICE   - media device file (/dev/[sdX|mmcblkX])
   --preempt-rt     - build real-time Linux kernel (CONFIG_PREEMPT_RT)
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

warning: this script requires root permissions and a lot of free space (aproximately 2 * sizeof(source image) + 2Gb for compiled kernel and u-boot)
