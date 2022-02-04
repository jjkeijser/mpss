# Manycore Processor Software Stack

The Intel Xeon Phi co-processors are configured and controlled via the Intel Manycore Processor Software Stack orMPSS.
The MPSS was available on the Intel website  for both the first generation Xeon Phis (Knights Corner aka `KNC`, 
model `x100` series: 31xx, 51xx, 71xx) and second generation (KnightsLanding aka `KNL`, model `x200` series: 7220 and 7240). 

As Intel dropped support for the Xeon Phi's a while back I decided to upload my patches for the MPSS software stack.
This applies to both MPSS v3.8 (for KNC) and MPSS v4.4 (for KNL).


## Note
Most users of the KNL Xeon Phis use the **processor** version of the second generation Xeon Phi (KNL models 7210, 7230, 7250). 
The KNL processor does not need or benefit from the MPSS stack at all. It is needed only for the PCI-e **co-processor** version,
which is actually pretty rare (models 7220 and 7240), as Intel more or less pulled them from the market before they were widely launched.

Intel released the MPSS code until GPL so we are free to modify it, as long as we post the patches.

## MPSS 3

The latest version of the MPSS stack for KNC released by Intel is v3.8.6 with support up to RedHat Enterprise Linux / CentOS 7.4 
(kernel 3.10.0-693). In the directory `mpss3` you will find patches for the `mpss-modules` package to allow you to use a KNC
co-processor on newer versions of RHEL and CentOS, upto CentOS 7.9 and 8.5.

Note that these patches were originally posted at https://www.nikhef.nl/~janjust/mpss/

## MPSS 4

The latest version of the MPSS stack for KNL released by Intel is v4.4.1 with support up to RedHat Enterprise Linux / CentOS 7.3.
In the directory `mpss4` you will find patches for the `mpss-modules` package to allow you to use a KNL co-processor on newer 
versions of RHEL and CentOS, upto CentOS 7.9 and 8.5. Note that with my patches to `mpss-modules` you can also build the
kernel modules for Ubuntu 18 (kernel 4.15).

Also, in this directory you will find rebuilds of the mpss core packages for CentOS 8.3+, as the `mpss4` packages from Intel 
cannot be easily installed on RHEL/CentOS 8 without modication.
