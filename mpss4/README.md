# MPSS 4

The MPSS 4 stack is intended for use with the second generation Intel Xeon Phi x200
co-processors, known as "Knights Landing" or `KNL`.


For the MPSS 4 stack on RHEL/CentOS 7 only the `mpss-modules` code needs to be updated.
All other packages can be installed from the "stock" Intel MPSS 4.4.1 tarball.

To use the MPSS 4 stack on RHEL/CentOS 8+ extensive updates to nearly all packages are
needed, as the MPSS 4 stack is mostly C++ based. The packages compiled on RHEL/CentOS 7
do not run very well on RHEL/CentOS 8+, especially when it comes to dependencies such
as the Boost libraries.


For each kernel version, the source tree is updated and tagged with that kernel version.
Apart from that, separate RPM spec files and patches are also included in the directories
`spec-files` and `patches`.


## Building the mpss-modules RPM

The way I have built the mpss-modules RPMs on https://www.nikhef.nl/~janjust/mpss/
is as follows
```
  cp spec-files/mpss-modules.spec ~/rpmbuild/SPECS
  cp patches/mpss-modules*patch ~/rpmbuild/SOURCES
  cp mpss-modules.4.4.1.tar.bz2 ~/rpmbuild/SOURCES
  rpmbuild -bb ~/rpmbuild/SPECS/mpss-modules.spec
```

If you wish to build straight from the source tree the procedure will be slightly different:

* check out the mpss-modules code
* execute
```
  cd mpss-modules
  make BUILD_CARD=false KERNEL_SR=/usr/src/kernels/`uname -r`
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


With a small patch to the `mpss-daemon` code this problem is corrected and the right number of
cards is listed again.

Both of these problems were fixed in this version of the `mpss-daemon` source tree.

