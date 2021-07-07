# MPSS 3

The MPSS 3 stack is intended for use with the first generation Intel Xeon Phi x100
co-processors, known as "Knights Corner" or `KNC`.

For the MPSS 3 stack on RHEL/CentOS 7 only the `mpss-modules` code needs to be updated.
All other packages can be installed from the "stock" Intel MPSS 3.8.6 tarball.

To use the MPSS 3 stack on RHEL/CentOS 8+ an update to the `mpss-modules` code as well
as to the `mpss-daemon` code is required. 
All other packages can be installed from the "stock" Intel MPSS 3.8.6 tarball, though
some packages refer to `/usr/bin/python`, which is not present on RHEL/CentOS 8.
These packages should be manually fixed to point to `/usr/bin/python2.7`.


For each kernel version, the source tree is updated and tagged with that kernel version.
Apart from that, separate RPM spec files and patches are also included in the directories
`spec-files` and `patches`.


## Building the mpss-modules RPM

The way I have built the mpss-modules RPMs on https://www.nikhef.nl/~janjust/mpss/
is as follows
```
  cp spec-files/mpss-modules.spec ~/rpmbuild/SPECS
  cp patches/mpss-modules*patch ~/rpmbuild/SOURCES
  cp mpss-modules.3.8.6.tar.bz2 ~/rpmbuild/SOURCES
  rpmbuild -bb ~/rpmbuild/SPECS/mpss-modules.spec
```

If you wish to build straight from the source tree the procedure will be slightly different:

* check out the mpss-modules code
* execute
```
  cd mpss-modules
  make MIC_CARD_ARCH=k1om KERNEL_VERSION=<kernel-version> prefix=/usr sysconfdir=/etc
```

Note that this also works on Ubuntu 18 with the 4.15-generic kernel. The `mpss-modules` code
has not been ported to newer kernels than the one from RHEL 8.3.


## mpss-daemon patches for RHEL/CentOS 8

For RHEL/CentOS 8+ a patch to the `mpss-daemon` code is required, as the code made an assumption
about the Linux `sysfs` filesystem that is no longer valid for kernel 4+.


With RHEL/CentOS 8 and Linux kernel 4.18 the sysfs interface changed quite a bit. This can cause 
some odd things to happen if you have more than one Xeon Phi inserted in your system. In CentOS 7,
the devices in the sysfs directory /sys/class/mic were always listed low-to-high. In CentOS 8, 
however, the order is random:
```
  # cd /sys/class/mic
  # find .
  .
  ./ctrl
  ./mic1
  ./scif
  ./mic0
```
This causes the micctrl command to choke on the output and to list the mic0 twice: once as a 
configured devices and once as "present-but-not-configured". Needless to say, this causes some
errors when stopping and starting the cards.


With a small patch to the `mpss-deamon` code this problem is corrected and the right number of
cards is listed again.

A second issue with the micctrl command is that it has hardcoded references to the /sbin/ifup and
/sbin/ifdown commands built in.


Both of these problems were fixed in this version of the `mpss-daemon` source tree.

