E_BSD - shrinked kernel config file (FreeBSD 10), compiles in 2.5Mb kernel

add_packages - RC-script, to mount local packages from /etc/packages 
    to /packages with md5-checks
also /etc/rc.initdiskless changed from mkmfs to mount_tmpfs, 
    that makes more sense, thus tmpfs more memory-effective

[Latest image for vbox/vmware/etc](https://drive.google.com/open?id=0ByknFNFrcMHmdkRWUlNZM1NuX0E&authuser=0) (This image is in OVA-format, i.e. you can just open it with VMWare Player or VirtualBox)

FILE SYSTEM FORMAT

    =>     0  163840  md0  BSD  (80M)
           0      16       - free -  (8.0K)
          16   65536    1  !0  (32M)
       65552   49136    2  !0  (24M)
      114688   16384    4  !0  (8.0M)
      131072   32768    5  freebsd-ufs  (16M)

    Disk is being divided in 4 partitions: 

    root@tiny-10R:~ # ls /dev/ufs
        boot  bootfail  config  storage 

        `boot` -- root partition, read only, mounts on start

        `config` -- partition with /etc folder copy, 
        rc.initdiskless copies it in memory /etc folder on init
        by default it is unmounted

        `storage` -- partition for packages, updates
        by default mounted as read-only

        `bootfail` -- partition to failsafe/rescue boot. Consists of
        /dev/ and mfs_failsafe.lz image. Loads automaticaly
        if rootfs fails. For password restore purposes it can
        be booted with vfs.root.mountfrom="ufs:/dev/ufs/bootfail"
        option before kernel booting.


    `save`, `wr` or `commit` -- nanobsd script for save changes 
    from /etc to config-partition
    (nanobsd derivative, will be remastered in future)

    /proot/[a-z] -- mount points for packages. Each package via /etc/rc.d/add_packages
    Script mounts to first free directory.Then links are made between /proot/[a-z] and /packages/

GET.CPP

    Just a programm to mount local packages as replacement of add_packages
    (not fully, rc-part of add_packages must exist)
    compiling: c++ -o name -lmd --static ./get.cpp
    or with -DDEBUG, then output will be more informative (and redundand)

ROOTFS

    There is a way to make possible to mount rootfs form lzma image without actualy
    loading it in RAM. This is slightly documented kernel VFS-feature (since 2010).
    We must use as rootfs boot-slice, e.g. vfs.root.mountfrom="ufs:/dev/ufs/boot" in
    loader.rc or directly in kernel-config. In that slice must present /dev/ directory
    and file .mount.conf, right in the top "/". .mount.conf must contain simple rules
    to mount "next-root".
