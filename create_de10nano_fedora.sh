#!/bin/bash

# Automate Media Creation for DE10 Nano SoC Fedora ARM 

# based on fedora /usr/bin/arm-image-installer
# also inspired by https://github.com/sahandKashani/SoC-FPGA-Design-Guide
# https://github.com/sahandKashani/SoC-FPGA-Design-Guide/blob/master/DE0_Nano_SoC/DE0_Nano_SoC_demo/create_linux_system.sh
# Current version
VERSION=1.00.00

# usage message
usage() {
    echo "
Usage: $(basename ${0}) <options>

   --image=IMAGE    - raw image file name
   --media=DEVICE   - media device file (/dev/[sdX|mmcblkX])
   --selinux=ON/OFF - Turn SELinux off/on as needed
   --norootpass     - Remove the root password
   -y		    - Assumes yes, will not wait for confirmation
   --version	    - Display version and exit
   --resizefs	    - Resize root filesystem to fill media device
   --addkey=        - /path/to/ssh-public-key

Example: $(basename ${0}) --image=Fedora-Rawhide.raw --media=/dev/mmcblk0
"
}

# Set some global variables for the command directory, target board directory,
# and valid targets.
DIR=$(dirname $0)

# check the args
while [ $# -gt 0 ]; do
        case $1 in
                --debug)
                        set -x
                        ;;
                -h|--help)
                        usage
                        exit 0
                        ;;
                --image*)
                        if echo $1 | grep '=' >/dev/null ; then
                                IMAGE=$(echo $1 | sed 's/^--image=//')
                        else
                                IMAGE=$2
                                shift
                        fi
                        ;;
                --media*)
                        if echo $1 | grep '=' >/dev/null ; then
                                MEDIA=$(echo $1 | sed 's/^--media=//')
                        else
                                MEDIA=$2
                                shift
                        fi
                        ;;
                --addkey*)
                        if echo $1 | grep '=' >/dev/null ; then
                                SSH_KEY=$(echo $1 | sed 's/^--addkey=//')
                        else
                                SSH_KEY=$2
                                shift
                        fi
                        ;;
                --selinux*)
                        if echo $1 | grep '=' >/dev/null ; then
                                SELINUX=$(echo $1 | sed 's/^--selinux=//')
                        else
                                SELINUX=$2
                                shift
                        fi
                        ;;
                --norootpass)
                        NOROOTPASS=1
                        ;;
                --resizefs)
                        RESIZEFS=1
                        ;;
                --version)
                        echo "$(basename ${0})-"$VERSION""
                        exit 0
                        ;;
                -y)
                        NOASK=1
                        ;;
                *)
                        echo "$(basename ${0}): Error - ${1}"
                        usage
                        exit 1
                        ;;
        esac
        shift
done

quartus_project_name="mksocfpga"

soc_system_dir="$DIR/soc_system"

preloader_dir="$DIR/sw/hps/preloader"
preloader_bin_file="${preloader_dir}/preloader-mkpimage.bin"

uboot_src_dir="$DIR/sw/hps/u-boot"
uboot_src_git_repo="git://git.denx.de/u-boot.git"
uboot_src_git_checkout_commit="master"
uboot_src_make_config_file="socfpga_de10_nano_defconfig"
uboot_script_file="${uboot_src_dir}/u-boot.script"
uboot_img_file="${uboot_src_dir}/u-boot-with-spl.sfp"

linux_dir="$DIR/sw/hps/linux"
linux_src_url="https://www.kernel.org/pub/linux/kernel/v4.x/linux-4.13.13.tar.xz"
linux_rt_patch_url="https://www.kernel.org/pub/linux/kernel/projects/rt/4.13/patches-4.13.13-rt5.tar.xz"
linux_src_dir="${linux_dir}/source"
linux_src_make_config_file="socfpga_de10nano_defconfig"
linux_kernel_mem_arg="1024M"
linux_uImage_file="${linux_src_dir}/arch/arm/boot/uImage"
linux_dtb_file="${soc_system_dir}/soc_system.dtb"
linux_rbf_file="${soc_system_dir}/soc_system.rbf"

sdcard_fat32_dir="$DIR/sdcard/fat32"
sdcard_fat32_rbf_file="${sdcard_fat32_dir}/soc_system.rbf"
sdcard_fat32_uboot_img_file="${sdcard_fat32_dir}/$(basename "${uboot_img_file}")"
sdcard_fat32_uboot_scr_file="${sdcard_fat32_dir}/boot.scr"
sdcard_fat32_uImage_file="${sdcard_fat32_dir}/uImage"
sdcard_fat32_dtb_file="${sdcard_fat32_dir}/soc_system.dtb"

sdcard_dev="$MEDIA"

sdcard_ext3_rootfs_tgz_file="$DIR/sdcard/ext3_rootfs.tar.gz"

sdcard_a2_dir="$DIR/sdcard/a2"
sdcard_a2_preloader_bin_file="${sdcard_a2_dir}/$(basename "${preloader_bin_file}")"

sdcard_partition_size_fat32="32M"
sdcard_partition_size_a2="2M"
sdcard_partition_size_boot="256M"
sdcard_partition_size_swap="512M"
sdcard_partition_size_linux="2048M"

sdcard_partition_number_fat32="1"
sdcard_partition_number_a2="2"
sdcard_partition_number_boot="3"
sdcard_partition_number_ext="4"
sdcard_partition_number_swap="5"
sdcard_partition_number_linux="6"

if [ "$(echo "${sdcard_dev}" | grep -P "/dev/sd\w.*$")" ]; then
    sdcard_dev_fat32_id="${sdcard_partition_number_fat32}"
    sdcard_dev_a2_id="${sdcard_partition_number_a2}"
    sdcard_dev_boot_id="${sdcard_partition_number_boot}"
    sdcard_dev_ext_id="${sdcard_partition_number_ext}"
    sdcard_dev_swap_id="${sdcard_partition_number_swap}"
    sdcard_dev_linux_id="${sdcard_partition_number_linux}"
elif [ "$(echo "${sdcard_dev}" | grep -P "/dev/mmcblk\w.*$")" ]; then
    sdcard_dev_fat32_id="p${sdcard_partition_number_fat32}"
    sdcard_dev_a2_id="p${sdcard_partition_number_a2}"
    sdcard_dev_boot_id="p${sdcard_partition_number_boot}"
    sdcard_dev_ext_id="p${sdcard_partition_number_ext}"
    sdcard_dev_swap_id="p${sdcard_partition_number_swap}"
    sdcard_dev_linux_id="p${sdcard_partition_number_linux}"
fi

sdcard_dev_fat32="${sdcard_dev}${sdcard_dev_fat32_id}"
sdcard_dev_a2="${sdcard_dev}${sdcard_dev_a2_id}"
sdcard_dev_boot="${sdcard_dev}${sdcard_dev_boot_id}"
sdcard_dev_swap="${sdcard_dev}${sdcard_dev_swap_id}"
sdcard_dev_linux="${sdcard_dev}${sdcard_dev_linux_id}"

#sdcard_dev_fat32_mount_point="$(readlink -m "sdcard/mount_point_fat32")"
#sdcard_dev_ext3_mount_point="$(readlink -m "sdcard/mount_point_ext3")"

sdcard_fat32_dir="$(readlink -m "sdcard/fat32")"
#sdcard_boot_uImage_file="sdcard/boot/uImage"
sdcard_boot_uImage_file="sdcard/fat32/uImage"
sdcard_fat32_dtb_file="sdcard/fat32/soc_system.dtb"
sdcard_fat32_rbf_file="sdcard/fat32/soc_system.rbf"
sdcard_fat32_uboot_scr_file="boot.scr"

uboot_script_file="boot.script"

# compile_linux() ##############################################################
compile_linux() {
    # if linux source tree doesn't exist, then download it
    if [ ! -d "${linux_src_dir}" ]; then
        git clone "${linux_src_git_repo}" "${linux_src_dir}"
    fi

    # change working directory to linux source tree directory
    pushd "${linux_src_dir}"

    # compile for the ARM architecture
    export ARCH=arm

    # use cross compiler instead of standard x86 version of gcc
    export CROSS_COMPILE=arm-linux-gnueabihf-

    # clean up source tree
    make distclean

    # checkout the following commit (tested and working):
    git checkout "${linux_src_git_checkout_commit}"

    # configure kernel for socfpga architecture
    make "${linux_src_make_config_file}"

    # compile zImage
    make -j4 zImage

    # compile device tree
    make -j4 "$(basename "${linux_dtb_file}")"

    # copy artifacts to associated sdcard directory
    cp "${linux_zImage_file}" "${sdcard_fat32_zImage_file}"
    cp "${linux_dtb_file}" "${sdcard_fat32_dtb_file}"

    # change working directory back to script directory
    popd
}

# compile_uboot ################################################################
compile_uboot() {

    # delete old artifacts
    rm -rf "${sdcard_fat32_uboot_scr_file}" \
           "${sdcard_fat32_uboot_img_file}"

    # if uboot source tree doesn't exist, then download it
    if [ ! -d "${uboot_src_dir}" ]; then
        git clone "${uboot_src_git_repo}" "${uboot_src_dir}"
    fi

    # change working directory to uboot source tree directory
    pushd "${uboot_src_dir}"

    # use cross compiler instead of standard x86 version of gcc
    export CROSS_COMPILE=arm-linux-gnueabihf-

    # clean up source tree
    make distclean

    # checkout the following commit (tested and working):
    git checkout "${uboot_src_git_checkout_commit}"

    # configure uboot for socfpga_cyclone5 architecture
    make "${uboot_src_make_config_file}"

    # compile uboot
    make -j4

    # create uboot script
    cat <<EOF > "${uboot_script_file}"
################################################################################
echo --- Resetting Env variables ---

# reset environment variables to default
env default -a

echo --- Setting Env variables ---

setenv ethaddr 82:3a:f0:75:4b:99

# sdcard boot partition number
setenv mmcloadpart ${sdcard_partition_number_boot}

# sdcard boot partition number
setenv mmcload_boot_part ${sdcard_partition_number_boot}

# sdcard fat32 partition number
setenv mmcload_fat32_part ${sdcard_partition_number_fat32}

# sdcard linux identifier
setenv mmcroot /dev/mmcblk0p${sdcard_partition_number_linux}

# Set the kernel image
setenv bootimage $(basename ${sdcard_boot_uImage_file});

# address to which the device tree will be loaded
setenv fdtaddr 0x00000100

setenv fpgadata 0x2000000

# Set the devicetree image
setenv fdtimage $(basename ${sdcard_fat32_dtb_file});

# hdmi
setenv HDMI_enable_dvi "no"

setenv hdmi_fdt_mod '\
fdt addr \${fdtaddr}; \
fdt resize; \
fdt mknode /soc framebuffer@3F000000; \
setenv fdt_frag /soc/framebuffer@3F000000; \
fdt set \${fdt_frag} compatible "simple-framebuffer"; \
fdt set \${fdt_frag} reg <0x3F000000 8294400>; \
fdt set \${fdt_frag} format "x8r8g8b8"; \
fdt set \${fdt_frag} width <\${HDMI_h_active_pix}>; \
fdt set \${fdt_frag} height <\${HDMI_v_active_lin}>; \
fdt set \${fdt_frag} stride <\${HDMI_stride}>; \
fdt set /soc stdout-path "display0"; \
fdt set /aliases display0 "/soc/framebuffer@3F000000"; \
sleep 2;'

setenv hdmi_cfg '\
i2c dev 2; \
load mmc 0:\${mmcload_fat32_part} 0x0c300000 STARTUP.BMP; \
load mmc 0:\${mmcload_fat32_part} 0x0c100000 de10_nano_hdmi_config.bin; \
go 0x0C100001; \
dcache flush; \
if test "\${HDMI_enable_dvi}" = "yes"; then \
i2c mw 0x39 0xAF 0x04 0x01; \
fi;'

setenv hdmi_dump_regs '\
i2c dev 2; icache flush; \
load mmc 0:1 0x0c100000 dump_adv7513_regs.bin; \
go 0x0C100001; icache flush;'

setenv hdmi_dump_edid '\
i2c dev 2; icache flush; \
load mmc 0:1 0x0c100000 dump_adv7513_edid.bin; \
go 0x0C100001; icache flush;'

# standard input/output
setenv stderr serial
setenv stdin serial
setenv stdout serial

# save environment to sdcard (not needed, but useful to avoid CRC errors on a new sdcard)
# saveenv

################################################################################
echo --- Programming FPGA ---

# load rbf from FAT partition into memory
fatload mmc 0:1 \${fpgadata} $(basename ${sdcard_fat32_rbf_file});

# program FPGA
fpga load 0 \${fpgadata} \${filesize};

# enable HPS-to-FPGA, FPGA-to-HPS, LWHPS-to-FPGA bridges
bridge enable;

################################################################################
echo --- Booting Linux ---

mmc rescan;

# initialize hdmi adv7513 ic
echo "initialise hdmi ic"
run hdmi_cfg;

# load device tree to memory
load mmc 0:\${mmcload_fat32_part} \${fdtaddr} \${fdtimage};

# modify devicetree according to detected display mode
if test "\${HDMI_status}" = "complete"; then
  echo "add simple frame buffer \${HDMI_h_active_pix}x\${HDMI_v_active_lin}"
#  run hdmi_fdt_mod;
fi;

# load linux kernel image
load mmc 0:\${mmcload_fat32_part} \${loadaddr} \${bootimage};

# set kernel boot arguments, then boot the kernel
setenv bootargs mem=${linux_kernel_mem_arg} console=ttyS0,115200 root=\${mmcroot} rw rootwait;
bootm \${loadaddr} - \${fdtaddr};
EOF

    mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "${quartus_project_name}" -d "${uboot_script_file}" "${sdcard_fat32_uboot_scr_file}"

    # copy artifacts to associated sdcard directory
    cp "${uboot_img_file}" "${sdcard_fat32_uboot_img_file}"

    # change working directory back to script directory
    popd
}

# Script execution #############################################################

# Report script line number on any error (non-zero exit code).
trap 'echo "Error on line ${LINENO}" 1>&2' ERR
set -e

# Create sdcard output directories
mkdir -p "${sdcard_a2_dir}"
mkdir -p "${sdcard_fat32_dir}"

#compile_quartus_project
#compile_preloader
compile_uboot
#compile_linux
#create_rootfs

# Write sdcard if it exists
if [ -z "${sdcard_dev}" ]; then
    echo "sdcard argument not provided => no sdcard written."

elif [ -b "${sdcard_dev}" ]; then
#    partition_sdcard
#    write_sdcard
fi
