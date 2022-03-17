# Using a PFS with the Xeon Phi (KNC)

Yes, this is possible, yet not supported and requires a small patch to the `knc-initrd`
script `/bin/init`.

After that, launch the Xeon Phi manually:

```
micctrl --reset mic0
cd /sys/class/mic/mic0
echo /var/mpss/PFS-XeonPhi.img  > virtblk_file
echo "root=/dev/vda console=hvc0 cgroup_disable=memory highres=off noautogroup micpm=cpufreq_on;corec6_off;pc3_on;pc6_off" > cmdline
echo boot:linux:/usr/share/mpss/boot/bzImage-knightscorner:/var/mpss/initrd-knc.cpio.gz  > state
```

## How do I create my own PFS-XeonPhi.img file?

This is the tricky part...

