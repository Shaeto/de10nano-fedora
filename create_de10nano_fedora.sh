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
   --workspace=PATH - base path for working directories (by default = script path)
   --cleanup        - remove all temporary data from workspace folder and exit
   --image=IMAGE    - raw image file name
   --media=DEVICE   - media device file (/dev/[sdX|mmcblkX])
   --preempt-rt     - build real-time Linux kernel (CONFIG_PREEMPT_RT)
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
DIR=$(readlink -m $(dirname $0))
WORKSPACE="$DIR"
PREEMPT_RT=0
CLEANUP=0

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
        --workspace*)
            if echo $1 | grep '=' >/dev/null ; then
                WORKSPACE=$(echo $1 | sed 's/^--workspace=//')
            else
                WORKSPACE=$2
                shift
            fi
            ;;
        --cleanup)
            CLEANUP=1
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
        --preempt-rt)
            PREEMPT_RT=1
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

uboot_src_dir="$WORKSPACE/sw/hps/u-boot"
uboot_src_git_repo="git://git.denx.de/u-boot.git"
uboot_src_git_checkout_commit="master"
uboot_src_make_config_file="socfpga_de10_nano_defconfig"
uboot_script_file="${uboot_src_dir}/u-boot.script"
uboot_img_file="${uboot_src_dir}/u-boot-with-spl.sfp"

linux_dir="$WORKSPACE/sw/hps/linux"
linux_src_version="4.13.13"
linux_src_dir="${linux_dir}/linux-${linux_src_version}"

linux_src_file="linux-${linux_src_version}.tar.xz"
linux_src_url="https://www.kernel.org/pub/linux/kernel/v4.x/${linux_src_file}"
linux_src_crc="sha256sums.asc"
linux_src_crc_url="https://www.kernel.org/pub/linux/kernel/v4.x/${linux_src_crc}"

linux_rt_patch="patch-${linux_src_version}-rt5.patch.xz"
linux_rt_patch_url="https://www.kernel.org/pub/linux/kernel/projects/rt/4.13/${linux_rt_patch}"
linux_rt_patch_crc="sha256sums.asc"
linux_rt_patch_crc_url="https://www.kernel.org/pub/linux/kernel/projects/rt/4.13/${linux_rt_patch_crc}"

linux_src_make_config_file="socfpga_de10nano_defconfig"
linux_src_make_config_file_path="$DIR/contrib/${linux_src_make_config_file}"
linux_src_altvipfb_driver_path="$DIR/contrib/drivers/altvipfb.c"
linux_src_altvipfb_config_patch_path="$DIR/contrib/drivers/altvipfb.patch"
linux_kernel_mem_arg="1024M"
linux_uImage_file="${linux_src_dir}/arch/arm/boot/uImage"
linux_dtb_file="${linux_src_dir}/arch/arm/boot/dts/socfpga_cyclone5_de10_nano.dtb"
linux_dtb_file_src_path="$DIR/contrib/socfpga_cyclone5_de10_nano.dts"
linux_dtb_file_patch_path="$DIR/contrib/socfpga_cyclone5_de10_nano.dts.patch"

sdcard_fat32_dir="$WORKSPACE/sdcard/fat32"
sdcard_boot_dir="$WORKSPACE/sdcard/boot"
sdcard_root_dir="$WORKSPACE/sdcard/root"
sdcard_volumes_info="$WORKSPACE/sdcard/os.volumes"
sdcard_fat32_rbf_file="${sdcard_fat32_dir}/soc_system.rbf"
sdcard_fat32_uboot_img_file="${sdcard_fat32_dir}/$(basename "${uboot_img_file}")"
sdcard_fat32_uboot_scr_file="${sdcard_fat32_dir}/boot.scr"
sdcard_fat32_uImage_file="${sdcard_fat32_dir}/uImage"
sdcard_fat32_dtb_file="${sdcard_fat32_dir}/socfpga_cyclone5_de10_nano.dtb"

sdcard_dev="$MEDIA"

sdcard_ext3_rootfs_tgz_file="$DIR/sdcard/ext3_rootfs.tar.gz"

image_mnt_dir="$WORKSPACE/mnt"

sdcard_a2_dir="$WORKSPACE/sdcard/a2"
sdcard_a2_preloader_bin_file="${sdcard_a2_dir}/$(basename "${uboot_img_file}")"

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

pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd "$@" > /dev/null
}

get_media_volume_info() {
    prep_sector_size=512

    prep_fat32_start=0
    prep_fat32_end=0
    prep_fat32_size=0
    prep_fat32_fsize=""
    prep_fat32_uuid=""

    prep_boot_start=0
    prep_boot_end=0
    prep_boot_size=0
    prep_boot_fsize=""
    prep_boot_uuid=""

    prep_root_start=0
    prep_root_end=0
    prep_root_size=0
    prep_root_fsize=""
    prep_root_uuid=""

    prep_swap_start=0
    prep_swap_end=0
    prep_swap_size=0
    prep_swap_fsize=""
    prep_swap_uuid=""

    prep_a2_start=0
    prep_a2_end=0
    prep_a2_size=0

    while read v
    do
	IFS=' ' read -a vol <<< "$v"

        vol_start=0
        vol_end=0
        vol_size=0
        vol_vsize=""
        vol_type=""

        if [ ${#vol[@]} -eq 6 ] ; then
            vol_start=${vol[1]}
            vol_end=${vol[2]}
            vol_size=${vol[3]}
            vol_fsize=${vol[4]}
            vol_type=${vol[5]}
        elif [ ${#vol[@]} -eq 7 ] ; then
            vol_start=${vol[2]}
            vol_end=${vol[3]}
            vol_size=${vol[4]}
            vol_fsize=${vol[5]}
            vol_type=${vol[6]}
        fi

        if [ "${vol[1]}" = "*" -a "$vol_type" = "83" ] ; then
            prep_boot_start=$vol_start
            prep_boot_end=$vol_end
            prep_boot_size=$vol_size
            prep_boot_fsize=$vol_fsize
        elif [ "$vol_type" = "c" ] ; then
            prep_fat32_start=$vol_start
            prep_fat32_end=$vol_end
            prep_fat32_size=$vol_size
            prep_fat32_fsize=$vol_fsize
        elif [ "$vol_type" = "82" ] ; then
            prep_swap_start=$vol_start
            prep_swap_end=$vol_end
            prep_swap_size=$vol_size
            prep_swap_fsize=$vol_fsize
        elif [ "$vol_type" = "83" ] ; then
            prep_root_start=$vol_start
            prep_root_end=$vol_end
            prep_root_size=$vol_size
            prep_root_fsize=$vol_fsize
        elif [ "$vol_type" = "a2" ] ; then
            prep_a2_start=$vol_start
            prep_a2_end=$vol_end
            prep_a2_size=$vol_size
        fi
    done <<< \
	$(fdisk -l "$1" | grep -e "^.*[[:digit:]]\+\([[:blank:]]\+\|[[:blank:]]\+\*[[:blank:]]\+\)[[:blank:]]\+[[:digit:]]\+[[:blank:]]\+[[:digit:]]\+[[:blank:]]\+[[:digit:]]\+[[:blank:]]\+[0-9\.]\+[GMKT]\?.*$" | \
	    sed -e "s/^.*\([[:digit:]]\+\)\([[:blank:]]\+\|[[:blank:]]\+\*[[:blank:]]\+\)[[:blank:]]\+\([[:digit:]]\+\)[[:blank:]]\+\([[:digit:]]\+\)[[:blank:]]\+\([[:digit:]]\+\)[[:blank:]]\+\([0-9\.]\+[GMKT]\?\)[[:blank:]]\+\([[:alnum:]]\+\).*$/\1\2 \3 \4 \5 \6 \7/" | \
	    awk 'BEGIN { FS = "[ \t]+" } { for(i=1;i<=NF;i++){printf "%s ", $i}; printf "\n" }')
}

# check media file, copy fat32, boot and root volumes
prepare_media_files() {
    if [ ! -f "$IMAGE" ] ; then
        echo
        echo "please provide path to fedora arm .raw image file (unpacked) using --image option"
        echo
        usage
        exit 1
    fi

    get_media_volume_info "$IMAGE"

    if [ $prep_fat32_size -le 0 -o $prep_boot_size -le 0 -o $prep_root_size -le 0 ] ; then
	echo "Failed to detect fedora source image volumes [$IMAGE]"
	exit 1
    fi

    echo -en "Found Fedora volumes in [$IMAGE]\nFAT32: $prep_fat32_fsize\nBOOT: $prep_boot_fsize\nROOT: $prep_root_fsize\n"
    if [ $prep_swap_size -gt 0 ] ; then
	echo "SWAP: $prep_swap_fsize"
    fi

    umount "${image_mnt_dir}" > /dev/null 2>&1 || :

    echo "Mounting FAT32 volume"
    mount_dev=$(losetup --show -o $(($prep_fat32_start*${prep_sector_size})) -r --sizelimit $(($prep_fat32_size*${prep_sector_size})) -f "$IMAGE")
    mount "${mount_dev}" -t vfat -o ro "${image_mnt_dir}"

    echo "Copying content of the FAT32 volume to ${sdcard_fat32_dir}"
    rm -rf "${sdcard_fat32_dir}"/*
    cp -a "${image_mnt_dir}"/* "${sdcard_fat32_dir}/"
    umount "${image_mnt_dir}"
    losetup -d "${mount_dev}"

    echo "Mounting BOOT volume"
    mount_dev=$(losetup --show -o $(($prep_boot_start*${prep_sector_size})) -r --sizelimit $(($prep_boot_size*${prep_sector_size})) -f "$IMAGE")
    mount "${mount_dev}" -o ro "${image_mnt_dir}"
    
    echo "Copying content of the BOOT volume to ${sdcard_boot_dir}"
    rm -rf "${sdcard_boot_dir}"/*
    cp -a "${image_mnt_dir}"/* "${sdcard_boot_dir}/"
    prep_boot_uuid=$(blkid -s UUID -o value "${mount_dev}")
    umount "${image_mnt_dir}"
    losetup -d "${mount_dev}"

    echo "Mounting ROOT volume"
    mount_dev=$(losetup --show -o $(($prep_root_start*${prep_sector_size})) -r --sizelimit $(($prep_root_size*${prep_sector_size})) -f "$IMAGE")
    mount "${mount_dev}" -o ro "${image_mnt_dir}"

    echo "Copying content of the ROOT volume to ${sdcard_root_dir}"
    rm -rf "${sdcard_root_dir}"/*
    cp -a "${image_mnt_dir}"/* "${sdcard_root_dir}/"
    prep_root_uuid=$(blkid -s UUID -o value "${mount_dev}")
    umount "${image_mnt_dir}"
    losetup -d "${mount_dev}"

    if [ $prep_swap_size -gt 0 ] ; then
	echo "Checking SWAP volume"
	mount_dev=$(losetup --show -o $(($prep_swap_start*${prep_sector_size})) -r --sizelimit $(($prep_swap_size*${prep_sector_size})) -f "$IMAGE")
	prep_swap_uuid=$(blkid -s UUID -o value "${mount_dev}")
	losetup -d "${mount_dev}"
    fi

    echo -en "boot volume uuid=$prep_boot_uuid\nroot volume uuid=$prep_root_uuid\n"
    if [ "$prep_swap_uuid" ] ; then
	echo "swap volume uuid=$prep_swap_uuid"
    fi

    # save volumes info
    echo "boot.uuid=$prep_boot_uuid" > "${sdcard_volumes_info}"
    echo "boot.size=$(($prep_boot_size*${prep_sector_size}))" >> "${sdcard_volumes_info}"
    echo "root.uuid=$prep_root_uuid" >> "${sdcard_volumes_info}"
    echo "root.size=$(($prep_root_size*${prep_sector_size}))" >> "${sdcard_volumes_info}"
    if [ "$prep_swap_uuid" ] ; then
	echo "swap.uuid=$prep_root_uuid" >> "${sdcard_volumes_info}"
	echo "swap.size=$((${prep_swap_size}*${prep_sector_size}))" >> "${sdcard_volumes_info}"
    fi
    echo "fat32.size=$(($prep_fat32_size*${prep_sector_size}))" >> "${sdcard_volumes_info}"
}

# compile_linux() ##############################################################
compile_linux() {
# if linux source tree doesn't exist, then download it
    if [ $PREEMPT_RT -ne 0 ] ; then
        if [ -d "${linux_src_dir}" -a ! -f "${linux_src_dir}/.de10nano_rt" ] ; then
            echo "Removing ${linux_src_dir}"
            rm -rf "${linux_src_dir}"
        fi
    else
        if [ -d "${linux_src_dir}" -a ! -f "${linux_src_dir}/.de10nano" ] ; then
            echo "Removing ${linux_src_dir}"
            rm -rf "${linux_src_dir}"
        fi
    fi

    if [ ! -d "${linux_src_dir}" ]; then
        echo "downloading ${linux_src_crc} from ${linux_src_crc_url}"
        curl -s "${linux_src_crc_url}" | grep "${linux_src_file}" > "${linux_dir}/${linux_src_crc}.kernel"
        if [ ! -f "${linux_dir}/${linux_src_file}" ] ; then
            echo "downloading ${linux_src_url}"
            curl "${linux_src_url}" > "${linux_dir}/${linux_src_file}"
        fi
        echo "validating linux kernel source file ${linux_dir}/${linux_src_file}"
        pushd "${linux_dir}"
        sha256sum --status -c "${linux_dir}/${linux_src_crc}.kernel"
        if [ $? -ne 0 ] ; then
            echo "${linux_dir}/${linux_src_file} is broken please delete it and start script again"
            exit 255
        fi
        xz -d -c "${linux_dir}/${linux_src_file}" | tar -xf -
        popd
        if [ $PREEMPT_RT -ne 0 ] ; then
            echo "downloading ${linux_rt_patch_crc} from ${linux_rt_patch_crc_url}"
            curl -s "${linux_rt_patch_crc_url}" | grep "${linux_rt_patch}" > "${linux_dir}/${linux_rt_patch_crc}.rt-patch"

            if [ ! -f "${linux_dir}/${linux_rt_patch}" ] ; then
                echo "downloading ${linux_rt_patch_url}"
                curl "${linux_rt_patch_url}" > "${linux_dir}/${linux_rt_patch}"
            fi
            echo "validating kernel rt-preempt patch file ${linux_dir}/${linux_rt_patch}"
            pushd "${linux_dir}"
            sha256sum --status -c "${linux_dir}/${linux_rt_patch_crc}.rt-patch"
            if [ $? -ne 0 ] ; then
                echo "${linux_dir}/${linux_rt_patch} is broken please delete it and start script again"
                exit 255
            fi
            xz -d -c "${linux_dir}/${linux_rt_patch}" | patch -d "${linux_src_dir}" -p1
            popd
        fi
        cp -f "${linux_src_make_config_file_path}" "${linux_src_dir}/arch/arm/configs/"
        cp -f "${linux_src_altvipfb_driver_path}" "${linux_src_dir}/drivers/video/fbdev/"
        patch -d "${linux_src_dir}" -p1 < "${linux_src_altvipfb_config_patch_path}"
        cp -f "${linux_dtb_file_src_path}" "${linux_src_dir}/arch/arm/boot/dts/"
        patch -d "${linux_src_dir}" -p1 < "${linux_dtb_file_patch_path}"

        if [ $PREEMPT_RT -ne 0 ] ; then
            touch "${linux_src_dir}/.de10nano_rt"
        else
            touch "${linux_src_dir}/.de10nano"
        fi
    fi

# change working directory to linux source tree directory
    pushd "${linux_src_dir}"

# compile for the ARM architecture
    export ARCH=arm

# use cross compiler instead of standard x86 version of gcc
    export CROSS_COMPILE=arm-none-eabi-

# set kernel load address
    export LOADADDR=0x8000

# clean up source tree
    make distclean

# configure kernel for socfpga architecture
    make "${linux_src_make_config_file}"

# compile zImage
    make -j4 uImage

# compile device tree
    make -j4 "$(basename "${linux_dtb_file}")"

# copy artifacts to associated sdcard directory
    cp "${linux_uImage_file}" "${sdcard_fat32_uImage_file}"
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
    export CROSS_COMPILE=arm-none-eabi-

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
setenv bootimage $(basename ${sdcard_fat32_uImage_file});

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
    cp "${uboot_img_file}" "${sdcard_a2_preloader_bin_file}"

# change working directory back to script directory
    popd
}

# partition_sdcard() ###########################################################
partition_sdcard() {
    # manually partitioning the sdcard
        # sudo fdisk /dev/sdx
            # use the following commands
            # n p 3 <default> 4095  t   a2 (2048 is default first sector)
            # n p 1 <default> +32M  t 1  b (4096 is default first sector)
            # n p 2 <default> +512M t 2 83 (69632 is default first sector)
            # w
        # result
            # Device     Boot Start     End Sectors  Size Id Type
            # /dev/sdb1        4096   69631   65536   32M  b W95 FAT32
            # /dev/sdb2       69632 1118207 1048576  512M 83 Linux
            # /dev/sdb3        2048    4095    2048    1M a2 unknown
        # note that you can choose any size for the FAT32 and Linux partitions,
        # but the a2 partition must be 1M.

    # automatically partitioning the sdcard
    # wipe partition table
    sudo dd if="/dev/zero" of="${sdcard_dev}" bs=512 count=1

    # create partitions
    # no need to specify the partition number for the first invocation of
    # the "t" command in fdisk, because there is only 1 partition at this
    # point
    echo -e "n\np\n3\n\n4095\nt\na2\nn\np\n1\n\n+${sdcard_partition_size_fat32}\nt\n1\nb\nn\np\n2\n\n+${sdcard_partition_size_linux}\nt\n2\n83\nw\nq\n" | sudo fdisk "${sdcard_dev}"

    # create filesystems
    sudo mkfs.vfat "${sdcard_dev_fat32}"
    sudo mkfs.ext3 -F "${sdcard_dev_ext3}"
}

# partition_file() ###########################################################
partition_file() {
    part_file_fat32_size=$(grep "fat32\.size" "${sdcard_volumes_info}" | sed -e "s/.*=\([0-9]\+\)$/\1/")
    part_file_boot_size=$(grep "boot\.size" "${sdcard_volumes_info}" | sed -e "s/.*=\([0-9]\+\)$/\1/")
    part_file_boot_uuid=$(grep "boot\.uuid" "${sdcard_volumes_info}" | sed -e "s/.*=\([[:alnum:]-]\+\)$/\1/")
    part_file_root_size=$(grep "root\.size" "${sdcard_volumes_info}" | sed -e "s/.*=\([0-9]\+\)$/\1/")
    part_file_root_uuid=$(grep "root\.uuid" "${sdcard_volumes_info}" | sed -e "s/.*=\([[:alnum:]-]\+\)$/\1/")
    part_file_swap_size=$(grep "swap\.size" "${sdcard_volumes_info}" | sed -e "s/.*=\([0-9]\+\)$/\1/")
    part_file_swap_uuid=$(grep "swap\.uuid" "${sdcard_volumes_info}" | sed -e "s/.*=\([[:alnum:]-]\+\)$/\1/")

    if [ -z "${part_file_swap_size}" ] ; then
	# 256MB
	part_file_swap_size=268435456
    fi

    part_file_size=$((${part_file_fat32_size}+${part_file_boot_size}+${part_file_root_size}+${part_file_swap_size}+2097152+33554432))
    part_file_size=$((${part_file_size}/1048576))

    echo "Creating image file ${sdcard_dev} size ${part_file_size}M"
    dd if=/dev/zero bs=1M count=${part_file_size} status=none > "${sdcard_dev}"

    echo "Partitioning file ${sdcard_dev}"
    sdcard_partition_size_fat32="$((${part_file_fat32_size}/1048576))M"
    sdcard_partition_size_boot="$((${part_file_boot_size}/1048576))M"
    sdcard_partition_size_root="$((${part_file_root_size}/1048576))M"
    sdcard_partition_size_swap="$((${part_file_swap_size}/1048576))M"

    make_fat32_seq="n\np\n1\n\n+${sdcard_partition_size_fat32}\nt\nc\n"
    make_a2_seq="n\np\n2\n\n+${sdcard_partition_size_a2}\nt\n2\na2\n"
    make_boot_seq="n\np\n3\n\n+${sdcard_partition_size_boot}\nt\n3\n83\na\n3\n"
    make_extpart_seq="n\ne\n\n\n"
    make_swap_seq="n\n\n+${sdcard_partition_size_swap}\nt\n5\n82\n"
    #make_root_seq="n\n\n+${sdcard_partition_size_root}\nt\n6\n83\n"
    make_root_seq="n\n\n\nt\n6\n83\n"

    echo -e "${make_fat32_seq}${make_a2_seq}${make_boot_seq}${make_extpart_seq}${make_swap_seq}${make_root_seq}w\nq\n" | fdisk "${sdcard_dev}" > /dev/null

    get_media_volume_info "${sdcard_dev}"

    if [ $prep_fat32_size -le 0 -o $prep_boot_size -le 0 -o $prep_root_size -le 0 -o $prep_swap_size -le 0 -o $prep_a2_size -le 0 ] ; then
        echo "Failed to detect new volumes in [${sdcard_dev}]"
        exit 1
    fi

    echo "Formatting FAT32"
    mount_dev=$(losetup --show -o $(($prep_fat32_start*${prep_sector_size})) --sizelimit $(($prep_fat32_size*${prep_sector_size})) -f "${sdcard_dev}")
    mkfs -t vfat "${mount_dev}" > /dev/null
    echo "Transferring data from ${sdcard_fat32_dir}"
    mount -t vfat "${mount_dev}" "${image_mnt_dir}"
    cp -a "${sdcard_fat32_dir}"/* "${image_mnt_dir}"
    umount "${image_mnt_dir}"
    losetup -d "${mount_dev}"

    echo "Formatting BOOT"
    mount_dev=$(losetup --show -o $(($prep_boot_start*${prep_sector_size})) --sizelimit $(($prep_boot_size*${prep_sector_size})) -f "${sdcard_dev}")
    mke2fs -q -t ext2 -U "${part_file_boot_uuid}" "${mount_dev}"
    echo "Transferring data from ${sdcard_boot_dir}"
    mount "${mount_dev}" "${image_mnt_dir}"
    cp -a "${sdcard_boot_dir}"/* "${image_mnt_dir}"
    umount "${image_mnt_dir}"
    losetup -d "${mount_dev}"

    echo "Formatting SWAP"
    mount_dev=$(losetup --show -o $(($prep_swap_start*${prep_sector_size})) --sizelimit $(($prep_swap_size*${prep_sector_size})) -f "${sdcard_dev}")
    if [ "${part_file_swap_uuid}" ] ; then
	mkswap -f -U "${part_file_swap_uuid}" "${mount_dev}" > /dev/null
    else
	mkswap -f "${mount_dev}" > /dev/null
    fi
    losetup -d "${mount_dev}"

    echo "Formatting ROOT"
    mount_dev=$(losetup --show -o $(($prep_root_start*${prep_sector_size})) --sizelimit $(($prep_root_size*${prep_sector_size})) -f "${sdcard_dev}")
    mke2fs -q -t ext4 -O ^has_journal -U "${part_file_root_uuid}" "${mount_dev}"
    echo "Transferring data from ${sdcard_root_dir}"
    mount "${mount_dev}" "${image_mnt_dir}"
    cp -a "${sdcard_root_dir}"/* "${image_mnt_dir}"
    umount "${image_mnt_dir}"
    losetup -d "${mount_dev}"

    echo "Installing Cyclone V U-Boot+SPL"
    mount_dev=$(losetup --show -o $(($prep_a2_start*${prep_sector_size})) --sizelimit $(($prep_a2_size*${prep_sector_size})) -f "${sdcard_dev}")
    dd status=none if="${sdcard_a2_preloader_bin_file}" of="${mount_dev}"
    losetup -d "${mount_dev}"    

    echo
    echo "${sdcard_dev} is ready, now you can write it to sdcard using dd or arm-image-installer"
    echo
}

# Script execution #############################################################

if [ $(/usr/bin/id -un) != "root" ] ; then
    echo "please run $0 under root permissions"
    exit 255
fi

# Report script line number on any error (non-zero exit code).
trap 'echo "Error on line ${LINENO}" 1>&2' ERR
set -e

if [ $CLEANUP -ne 0 ] ; then
    echo "Cleaning up workspace $WORKSPACE"
    if [ -d "$WORKSPACE/sw" ] ; then
	rm -rf "$WORKSPACE/sw"
	echo "Removed $WORKSPACE/sw"
    fi
    if [ -d "$WORKSPACE/sdcard" ] ; then
	rm -rf "$WORKSPACE/sdcard"
	echo "Removed $WORKSPACE/sdcard"
    fi
    if [ -d "$WORKSPACE/mnt" ] ; then
	umount "$WORKSPACE/mnt" > /dev/null 2>&1 || :
	rm -rf "$WORKSPACE/mnt"
	echo "Removed $WORKSPACE/mnt"
    fi
    exit 0
fi

required_utils="/usr/bin/mkimage
/usr/sbin/mkfs.vfat
/usr/sbin/blkid
/usr/sbin/losetup
/usr/bin/curl
/usr/sbin/fdisk
/usr/bin/sha256sum
/usr/bin/patch
/usr/bin/arm-none-eabi-gcc"

have_required_tools=1
for ru in ${required_utils} ; do
    if [ ! -x "$ru" ] ; then
	echo "$ru - is not available"
	have_required_tools=0
    fi
done

if [ $have_required_tools -eq 0 ] ; then
    echo "please install all required tools!"
    exit 255
fi

# Create sdcard output directories
mkdir -p "${sdcard_a2_dir}"
mkdir -p "${sdcard_fat32_dir}"
mkdir -p "${sdcard_root_dir}"
mkdir -p "${sdcard_boot_dir}"
mkdir -p "${image_mnt_dir}"

#compile_quartus_project
prepare_media_files
compile_uboot
compile_linux

# Write sdcard if it exists
if [ -z "${sdcard_dev}" ]; then
    echo "sdcard argument not provided => no sdcard written."
elif [ -b "${sdcard_dev}" ]; then
    echo "sdcard direct partitioning is not implemented please run with --media=output-image-file.img"
#    partition_sdcard
elif [ -z $(echo "${sdcard_dev}" | grep -e "^\/dev\/") ]; then
    partition_file
fi