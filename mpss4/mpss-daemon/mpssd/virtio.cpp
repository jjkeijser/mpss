/*
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include "virtio.h"

#include "mpssd.h"
#include "utils.h"

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <linux/if_arp.h>
#include <linux/if_tun.h>
#include <linux/sockios.h>
#include <mutex>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/unistd.h>

extern "C" unsigned int if_nametoindex (const char *ifname);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define ACCESS_ONCE(x) (*(volatile decltype(x) *)&(x))

#define GSO_ENABLED		0
#define MAX_GSO_SIZE		(64 * 1024)
#define ETH_H_LEN		14
#define MAX_NET_PKT_SIZE	(_ALIGN_UP(MAX_GSO_SIZE + ETH_H_LEN, 64))
#define MIC_DEVICE_PAGE_END	0x1000

#ifndef VIRTIO_NET_HDR_F_DATA_VALID
#define VIRTIO_NET_HDR_F_DATA_VALID	2	/* Csum is valid */
#endif


__thread struct mpssd_virtio_log virtio_log;


static struct virtcons_dev_page_t virtcons_dev_page_init()
{

	virtcons_dev_page_t value = {};

	value.dd.type = VIRTIO_ID_CONSOLE;
	value.dd.num_vq = ARRAY_SIZE(value.vqconfig);
	value.dd.feature_len = sizeof(value.host_features);
	value.dd.config_len = sizeof(value.cons_config);
	value.vqconfig[0].num = htole16(MIC_VRING_ENTRIES);
	value.vqconfig[1].num = htole16(MIC_VRING_ENTRIES);

	return value;
}

static struct virtnet_dev_page_t virtnet_dev_page_init()
{
	virtnet_dev_page_t value = {};

	value.dd.type = VIRTIO_ID_NET;
	value.dd.num_vq = ARRAY_SIZE(value.vqconfig);
	value.dd.feature_len = sizeof(value.host_features);
	value.dd.config_len = sizeof(value.net_config);
	value.vqconfig[0].num = htole16(MIC_VRING_ENTRIES);
	value.vqconfig[1].num = htole16(MIC_VRING_ENTRIES);

#if GSO_ENABLED
	value.host_features = htole32(
		1 << VIRTIO_NET_F_CSUM |
		1 << VIRTIO_NET_F_GSO |
		1 << VIRTIO_NET_F_GUEST_TSO4 |
		1 << VIRTIO_NET_F_GUEST_TSO6 |
		1 << VIRTIO_NET_F_GUEST_ECN);
#else
	value.host_features = 0;
#endif

	return value;
};

static virtblk_dev_page_t virtblk_dev_page_init(bool read_only)
{
	virtblk_dev_page_t value = {};

	value.dd.type = VIRTIO_ID_BLOCK;
	value.dd.num_vq = ARRAY_SIZE(value.vqconfig);
	value.dd.feature_len = sizeof(value.host_features);
	value.dd.config_len = sizeof(value.blk_config);
	value.vqconfig[0].num = htole16(MIC_VRING_ENTRIES);
	value.host_features = htole32(1 << VIRTIO_BLK_F_SEG_MAX);
	if (read_only)
		value.host_features |= htole32(1 << VIRTIO_BLK_F_RO);
	value.blk_config.seg_max = htole32(MIC_VRING_ENTRIES - 2);
	value.blk_config.capacity = htole64(0);

	return value;
}

void
mic_device_context::init()
{
	virtnet_dev_page = virtnet_dev_page_init();
	virtcons_dev_page = virtcons_dev_page_init();
}

void
mic_device_context::shutdown()
{
	m_ss.request_shutdown();
	m_vnet_thread.join();
	m_vcon_thread.join();
	for (auto &t : m_vblk_vector)
		t.join();
}

static int
tap_configure(struct mic_info *mic, const std::string& dev)
{
	pid_t pid;
	int ret = 0;
	const char *ifargv[7];

	pid = fork();
	if (pid == 0) {
		ifargv[0] = "ip";
		ifargv[1] = "link";
		ifargv[2] = "set";
		ifargv[3] = dev.c_str();
		ifargv[4] = "up";
		ifargv[5] = NULL;
		ret = execvp("ip", const_cast<char**>(ifargv));
		if (ret < 0)
			return ret;
	}
	if (pid < 0) {
		mpssd_log(PERROR, "fork failed errno %s", strerror(errno));
		return ret;
	}
	ret = waitpid(pid, NULL, 0);
	if (ret < 0) {
		mpssd_log(PERROR, "waitpid failed errno %s", strerror(errno));
		return ret;
	}
	mpssd_log(PINFO, "DONE!");
	return 0;
}

static void set_ip_address_for_mic_interface(short type, struct mic_info *mic, const std::string& ip,
const std::string& netmask, const std::string& dev)
{
	struct ifreq ifr = {0};

	if (!dev.empty()) {
		strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ - 1);
		ifr.ifr_name[IFNAMSIZ - 1] = '\0';
	}

	mpssd_log(PINFO, "setting IP address: %s, netmask: %s for interface %s", ip.c_str(), netmask.c_str(), dev.c_str());

	int soc = socket(type, SOCK_DGRAM, 0);
	int result = -1;
	if (type == AF_INET) {
		struct sockaddr_in ip4_addr = {0};
		struct sockaddr_in netmask_addr = {0};

		ip4_addr.sin_family = AF_INET;
		inet_pton(AF_INET, ip.c_str(), &ip4_addr.sin_addr);
		ifr.ifr_addr = *(struct sockaddr *)&ip4_addr;

		result = ioctl(soc, SIOCSIFADDR, &ifr);
		if (result != 0) {
			mpssd_log(PERROR, "cannot set IP address for interface %s %s", dev.c_str(), strerror(errno));
		}

		netmask_addr.sin_family = AF_INET;
		inet_pton(AF_INET, netmask.c_str(), &netmask_addr.sin_addr);
		ifr.ifr_netmask = *(struct sockaddr *)&netmask_addr;
		result = ioctl(soc, SIOCSIFNETMASK, &ifr);
		if (result != 0) {
			mpssd_log(PERROR, "cannot set netmask for interface %s %s", dev.c_str(), strerror(errno));
		}
	} else if (type == AF_INET6) {
		struct in6_ifreq ifr_inet6 = {0};

		int index = ioctl(soc, SIOGIFINDEX, &ifr);
		if (index != 0) {
			mpssd_log(PERROR, "cannot get the index of the interface %s %s", dev.c_str(), strerror(errno));
		}
		inet_pton(AF_INET6, ip.c_str(), &ifr_inet6.ifr6_addr);
		ifr_inet6.ifr6_ifindex = ifr.ifr_ifindex;

		char *end = NULL;
		long value = strtol(netmask.c_str(), &end, 0);
		if (*end != '\0') {
			mpssd_log(PERROR, "wrong value for IPv6 netmask for interface %s %s", dev.c_str(), strerror(errno));
		} else {
			ifr_inet6.ifr6_prefixlen = (int) value;
		}

		result = ioctl(soc, SIOCSIFADDR, &ifr_inet6);
		if (result != 0) {
			mpssd_log(PERROR, "cannot set IP address for interface %s %s", dev.c_str(), strerror(errno));
		}
	}
	close(soc);
}

static void configure_host_network(struct mic_info *mic, const std::string& dev)
{
	mpss_host_network_type host_type = mic->config.net.host_type;
	mpss_host_network_type host_type_inet6 = mic->config.net.host_type_inet6;
	if (host_type == mpss_host_network_type::STATIC || host_type_inet6 == mpss_host_network_type::STATIC) {
		if (host_type == mpss_host_network_type::STATIC) {
			set_ip_address_for_mic_interface(AF_INET, mic, mic->config.net.host_setting.ip,
				mic->config.net.host_setting.netmask, dev);
		}
		if (host_type_inet6 == mpss_host_network_type::STATIC) {
			set_ip_address_for_mic_interface(AF_INET6, mic, mic->config.net.host_setting_inet6.ip,
				mic->config.net.host_setting_inet6.netmask, dev);
		}
	} else if (host_type == mpss_host_network_type::BRIDGE) {
		struct ifreq ifr;
		int ifindex = if_nametoindex(dev.c_str());

		int soc = socket(AF_INET, SOCK_DGRAM, 0);

		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, mic->config.net.bridge_name.c_str(), IFNAMSIZ - 1);
		ifr.ifr_name[IFNAMSIZ - 1] = '\0';

		ifr.ifr_ifindex = ifindex;
		int result = ioctl(soc, SIOCBRADDIF, &ifr);
		if (result != 0) {
			mpssd_log(PERROR, "cannot add interface to bridge: %s", strerror(errno));
		}
		close(soc);
	} else if  (host_type == mpss_host_network_type::NONE) {
		mpssd_log(PINFO, "skip setting host network configuration");
	} else {
		mpssd_log(PERROR, "Invalid type of network");
	}
}

static int tun_open()
{
	mpss_elist mpss_logs;

	int fd = open("/dev/net/tun", O_RDWR);
	if ((fd < 0) && (errno == ENODEV)) {
		mpssd_log(PWARN, "Could not open /dev/net/tun. Try to modprobe it.");
		if (exec_command("exec bash -c 'modprobe tun 2>&1'", mpss_logs)) {
			mpssd_elist_log(mpss_logs, &mpsslog);
		} else {
			mpssd_log(PINFO, "Tun loaded successfully.");
			fd = open("/dev/net/tun", O_RDWR);
		}
	}

	return fd;
}

static int tun_alloc(struct mic_info *mic, std::string& dev)
{
	struct ifreq ifr;
	int fd, err;
#if GSO_ENABLED
	unsigned offload;
#endif
	fd = tun_open();
	if (fd < 0) {
		mpssd_log(PERROR, "Could not open /dev/net/tun %s", strerror(errno));
		return fd;
	}

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_VNET_HDR;
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", dev.c_str());

	err = ioctl(fd, TUNSETIFF, (void *)&ifr);
	if (err < 0) {
		mpssd_log(PERROR, "TUNSETIFF failed %s", strerror(errno));
		close(fd);
		return err;
	}

#if GSO_ENABLED
	offload = TUN_F_CSUM | TUN_F_TSO4 | TUN_F_TSO6 | TUN_F_TSO_ECN;

	err = ioctl(fd, TUNSETOFFLOAD, offload);
	if (err < 0) {
		mpssd_log(PERROR, "TUNSETOFFLOAD failed %s", strerror(errno));
		close(fd);
		return err;
	}
#endif

	// set MAC address
	if (!mic->config.net.host_mac.address().empty()) {
		mpssd_log(PINFO, "Set host MAC address to %s", mic->config.net.host_mac.address().c_str());

		ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

		if (!mic->config.net.host_mac.to_mac_48((uint8_t*) ifr.ifr_hwaddr.sa_data)) {
			mpssd_log(PWARN, "Could not parse host MAC address");
			goto done;
		}

		err = ioctl(fd, SIOCSIFHWADDR, &ifr);
		if (err < 0) {
			mpssd_log(PWARN, "Could not set host MAC address");
			goto done;
		}
	}

done:
	dev = ifr.ifr_name;
	mpssd_log(PINFO, "Created TAP %s", dev.c_str());

	configure_host_network(mic, dev);

	return fd;
}

#define NET_FD_VIRTIO_NET 0
#define NET_FD_TUN 1
#define MAX_NET_FD 2

static void set_dp(struct mic_info *mic, int type, void *dp, size_t size, int idx = -1)
{
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;

	if ((type != VIRTIO_ID_BLOCK && idx != -1) ||
	    (type == VIRTIO_ID_BLOCK && idx < 0)) {
		mpssd_log(PERROR, "wrong index for requested type");
		return;
	}

	switch (type) {
	case VIRTIO_ID_CONSOLE:
		mpssdi->mic_console.console_dp = dp;
		mpssdi->mic_console.dp_size = size;
		return;
	case VIRTIO_ID_NET:
		mpssdi->mic_net.net_dp = dp;
		mpssdi->mic_net.dp_size = size;
		return;
	case VIRTIO_ID_BLOCK:
		mpssdi->mic_virtblk[idx].block_dp = dp;
		mpssdi->mic_virtblk[idx].dp_size = size;
		return;
	}
	mpssd_log(PERROR, "%d not found", type);
	assert(0);
}

static void *get_dp(struct mic_info *mic, int type, int idx = -1)
{
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;

	if ((type != VIRTIO_ID_BLOCK && idx != -1) ||
	    (type == VIRTIO_ID_BLOCK && idx < 0)) {
		mpssd_log(PERROR, "wrong index for requested type");
		return NULL;
	}

	switch (type) {
	case VIRTIO_ID_CONSOLE:
		return mpssdi->mic_console.console_dp;
	case VIRTIO_ID_NET:
		return mpssdi->mic_net.net_dp;
	case VIRTIO_ID_BLOCK:
        /* add extra if() check to keep centos 8 C++ happy */
		if (idx >= 0) return mpssdi->mic_virtblk[idx].block_dp;
	}
	mpssd_log(PERROR, "%d not found", type);
	assert(0);
	return NULL;
}

static struct mic_device_desc *get_device_desc(struct mic_info *mic, int type)
{
	struct mic_device_desc *d;
	int i;
	void *dp = get_dp(mic, type);
	if (!dp)
		return NULL;

	for (i = sizeof(struct mic_bootparam); i < PAGE_SIZE;
		i += mic_total_desc_size(d)) {
		d = (struct mic_device_desc *) ((char *) dp + i);

		/* End of list */
		if (d->type == 0)
			break;

		if (d->type == -1)
			continue;

		mpssd_log(PINFO, "virtio_device %d: %s (desc %p)",
			i, get_virtio_device_name(d->type), d);

		if (d->type == type) {
			mpssd_log(PINFO, "return %p", d);
			return d;
		}
	}
	mpssd_log(PERROR, "%d not found", type);
	return NULL;
}

/* See comments in vhost.c for explanation of next_desc() */
static unsigned next_desc(struct vring_desc *desc)
{
	unsigned int next;

	if (!(le16toh(desc->flags) & VRING_DESC_F_NEXT))
		return -1U;
	next = le16toh(desc->next);
	return next;
}

/* Sum up all the IOVEC length */
static ssize_t
sum_iovec_len(struct mic_copy_desc *copy)
{
	ssize_t sum = 0;
	unsigned int i;

	for (i = 0; i < copy->iovcnt; i++)
		sum += copy->iov[i].iov_len;
	return sum;
}

static inline void verify_out_len(struct mic_info *mic,
	struct mic_copy_desc *copy)
{
	if (copy->out_len != sum_iovec_len(copy)) {
		mpssd_log(PERROR, "BUG copy->out_len 0x%x len 0x%zx",
			copy->out_len, sum_iovec_len(copy));
		assert(copy->out_len == sum_iovec_len(copy));
	}
}

/* Display an iovec */
static void
disp_iovec(struct mic_info *mic, struct mic_copy_desc *copy)
{
	unsigned int i;

	for (i = 0; i < copy->iovcnt; i++)
		mpssd_log(PINFO, "copy->iov[%d] addr %p len 0x%zx",
			i, copy->iov[i].iov_base, copy->iov[i].iov_len);
}

static inline __u16 read_avail_idx(struct mic_vring *vr)
{
	return ACCESS_ONCE(vr->info->avail_idx);
}

static inline void txrx_prepare(int type, bool tx, struct mic_vring *vr,
				struct mic_copy_desc *copy, ssize_t len)
{
	copy->vr_idx = tx ? 0 : 1;
	copy->update_used = true;
	if (type == VIRTIO_ID_NET)
		copy->iov[1].iov_len = len - sizeof(struct virtio_net_hdr);
	else
		copy->iov[0].iov_len = len;
}

/* Central API which triggers the copies */
static int
mic_virtio_copy(struct mic_info *mic, int fd,
		struct mic_vring *vr, struct mic_copy_desc *copy)
{
	int ret;

	ret = ioctl(fd, MIC_VIRTIO_COPY_DESC, copy);
	if (ret) {
		mpssd_log(PERROR, "errno %s ret %d", strerror(errno), ret);
	}
	return ret;
}

static __inline__ unsigned _vring_size(unsigned int num, unsigned long align)
{
	return ((sizeof(struct vring_desc) * num + sizeof(__u16) * (3 + num)
				+ align - 1) & ~(align - 1))
		+ sizeof(__u16) * 3 + sizeof(struct vring_used_elem) * num;
}

static void
add_virtio_device(struct mic_info *mic, struct mic_device_desc *dd, int idx = -1)
{
	int fd, err, timeout_count = 15;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;

	if ((dd->type != VIRTIO_ID_BLOCK && idx != -1) ||
	    (dd->type == VIRTIO_ID_BLOCK && idx < 0)) {
		mpssd_log(PERROR, "wrong index for requested type");
		return;
	}

	std::string path = "/dev/vop_virtio" + std::to_string(mic->id);

	do {
		fd = open(path.c_str(), O_RDWR);
		if (fd > 0)
			break;
		mpssd_log(PINFO, "waiting for %s ...", path.c_str());
		sleep(1);
	}
	while (timeout_count--);
	if (fd < 0) {
		mpssd_log(PERROR, "could not open %s: %s", path.c_str(), strerror(errno));
		return;
	} else {
		mpssd_log(PINFO, "%s opened (%d), timeout %d",
			  path.c_str(), fd, timeout_count);
	}

	err = ioctl(fd, MIC_VIRTIO_ADD_DEVICE, dd);
	if (err < 0) {
		mpssd_log(PERROR, "Could not add %d %s", dd->type, strerror(errno));
		close(fd);
		return;
	}

	switch (dd->type) {
	case VIRTIO_ID_NET:
		mpssdi->mic_net.virtio_net_fd = fd;
		mpssd_log(PINFO, "Added VIRTIO_ID_NET");
		break;
	case VIRTIO_ID_CONSOLE:
		mpssdi->mic_console.virtio_console_fd = fd;
		mpssd_log(PINFO, "Added VIRTIO_ID_CONSOLE");
		break;
	case VIRTIO_ID_BLOCK:
		mpssdi->mic_virtblk[idx].virtio_block_fd = fd;
		mpssd_log(PINFO, "Added VIRTIO_ID_BLOCK");
		break;
	}
}

/*
 * This initialization routine requires at least one
 * vring i.e. vr0. vr1 is optional.
 */
static void *
init_vr(struct mic_info *mic, int fd, int type,
	struct mic_vring *vr0, struct mic_vring *vr1, int num_vq, int idx = -1)
{
	int vr_size;
	char *va;

	vr_size = PAGE_ALIGN(_vring_size(MIC_VRING_ENTRIES,
		MIC_VIRTIO_RING_ALIGN) + sizeof(struct _mic_vring_info));
	size_t dp_size = MIC_DEVICE_PAGE_END + vr_size * num_vq;
	va = (char *) mmap(NULL, dp_size, PROT_READ, MAP_SHARED, fd, 0);
	if (MAP_FAILED == va) {
		mpssd_log(PERROR, "mmap failed errno %s", strerror(errno));
		goto done;
	}

	if ((type != VIRTIO_ID_BLOCK && idx != -1) ||
	    (type == VIRTIO_ID_BLOCK && idx < 0)) {
		mpssd_log(PERROR, "wrong index for requested type");
		return NULL;
	}

	set_dp(mic, type, va, dp_size, idx);

	vr0->va = (struct mic_vring *)&va[MIC_DEVICE_PAGE_END];
	vr0->info = (struct _mic_vring_info *) ((char *) vr0->va +
		_vring_size(MIC_VRING_ENTRIES, MIC_VIRTIO_RING_ALIGN));
	vring_init(&vr0->vr,
		   MIC_VRING_ENTRIES, vr0->va, MIC_VIRTIO_RING_ALIGN);
	mpssd_log(PINFO, "virtio_device: %s vr0 %p vr0->info %p vr_size 0x%x vring 0x%x",
		get_virtio_device_name(type), vr0->va, vr0->info, vr_size,
		_vring_size(MIC_VRING_ENTRIES, MIC_VIRTIO_RING_ALIGN));
	mpssd_log(PINFO, "magic 0x%x expected 0x%x",
		le32toh(vr0->info->magic), MIC_MAGIC + type);
	assert(le32toh(vr0->info->magic) == MIC_MAGIC + type);
	if (vr1) {
		vr1->va = (struct mic_vring *)
			&va[MIC_DEVICE_PAGE_END + vr_size];
		vr1->info = (struct _mic_vring_info *) ((char *) vr1->va + _vring_size(MIC_VRING_ENTRIES,
			MIC_VIRTIO_RING_ALIGN));
		vring_init(&vr1->vr,
			   MIC_VRING_ENTRIES, vr1->va, MIC_VIRTIO_RING_ALIGN);
		mpssd_log(PINFO, "virtio_device: %s vr1 %p vr1->info %p vr_size 0x%x vring 0x%x",
			get_virtio_device_name(type), vr1->va, vr1->info, vr_size,
			_vring_size(MIC_VRING_ENTRIES, MIC_VIRTIO_RING_ALIGN));
		mpssd_log(PINFO, "magic 0x%x expected 0x%x",
			le32toh(vr1->info->magic), MIC_MAGIC + type + 1);
		assert(le32toh(vr1->info->magic) == MIC_MAGIC + type + 1);
	}
done:
	return va;
}

static int
wait_for_card_driver(mic_device_context *mdc, struct mic_info *mic, int fd, int type)
{
	struct pollfd pollfd;
	int err;
	struct mic_device_desc *desc = get_device_desc(mic, type);
	__u8 prev_status;

	if (!desc)
		return ENODEV;
	prev_status = desc->status;
	pollfd.fd = fd;
	mpssd_log(PINFO, "Waiting for card driver: virtio_device %s status 0x%x",
		get_virtio_device_name(type), desc->status);
	while (1) {
		if (mdc->need_shutdown()) {
			break;
		}

		pollfd.events = POLLIN;
		pollfd.revents = 0;
		err = poll(&pollfd, 1, 1000);
		if (err < 0) {
			mpssd_log(PERROR, "poll failed %s", strerror(errno));
			continue;
		}

		if (pollfd.revents) {
			if (pollfd.revents & POLLERR) {
				mpssd_log(PINFO, "POLLERR: device not initialized");
				return ENODEV;
			}

			if (pollfd.revents & POLLHUP) {
				mpssd_log(PINFO, "POLLHUP: device not initialized");
				return ENODEV;
			}

			if (desc->status != prev_status) {
				mpssd_log(PINFO, "Waiting: virtio_deivce %s status 0x%x prev_status 0x%x",
					get_virtio_device_name(type), desc->status, prev_status);
				prev_status = desc->status;
			}
			if (desc->status & VIRTIO_CONFIG_S_DRIVER_OK) {
				mpssd_log(PINFO, "poll.revents %d", pollfd.revents);
				mpssd_log(PINFO, "desc-> type %d status 0x%x",
					type, desc->status);
				break;
			}
		}
	}
	return 0;
}

/* Spin till we have some descriptors */
static void
spin_for_descriptors(mic_device_context *mdc, struct mic_info *mic, struct mic_vring *vr)
{
	__u16 avail_idx = read_avail_idx(vr);

	while (avail_idx == le16toh(ACCESS_ONCE(vr->vr.avail->idx))) {
#ifdef DEBUG
		mpssd_log(PINFO, "waiting for desc avail %d info_avail %d",
			le16toh(vr->vr.avail->idx), vr->info->avail_idx);
#endif
		if (mdc->need_shutdown()) {
			break;
		}
		sched_yield();
	}
}

void
add_virtio_net_device(mic_device_context *mdc, struct mic_info *mic)
{
	if (!mic->config.net.mic_mac.address().empty()) {
		mpssd_log(PINFO, "Set mic MAC address to %s", mic->config.net.mic_mac.address().c_str());

		if (mic->config.net.mic_mac.to_mac_48(mdc->virtnet_dev_page.net_config.mac))
			mdc->virtnet_dev_page.host_features |= (1 << VIRTIO_NET_F_MAC);
		else
			mpssd_log(PWARN, "Could not parse mic MAC address");
	}

	add_virtio_device(mic, &mdc->virtnet_dev_page.dd);
}

void
virtio_net(mic_device_context *mdc, mic_info* mic)
{
	__u8 vnet_hdr[2][sizeof(struct virtio_net_hdr)];
	__u8 vnet_buf[2][MAX_NET_PKT_SIZE] __attribute__ ((aligned(64)));
	struct iovec vnet_iov[2][2] = {};
	struct iovec *iov0, *iov1;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	struct pollfd net_poll[MAX_NET_FD];
	struct mic_vring tx_vr, rx_vr;
	struct mic_copy_desc copy;
	struct mic_device_desc *desc;
	int err;

	virtio_log.virtio_device_type = VIRTIO_ID_NET;
	virtio_log.virtio_device_number = 1;
	set_thread_name(mpssdi->name().c_str(), "virtnet");

	add_virtio_net_device(mdc, mic);

	vnet_iov[0][0].iov_base = vnet_hdr[0];
	vnet_iov[0][0].iov_len = sizeof(vnet_hdr[0]);
	vnet_iov[0][1].iov_base = vnet_buf[0];
	vnet_iov[0][1].iov_len = sizeof(vnet_buf[0]);
	vnet_iov[1][0].iov_base = vnet_hdr[1];
	vnet_iov[1][0].iov_len = sizeof(vnet_hdr[1]);
	vnet_iov[1][1].iov_base = vnet_buf[1];
	vnet_iov[1][1].iov_len = sizeof(vnet_buf[1]);

	iov0 = vnet_iov[0];
	iov1 = vnet_iov[1];

	std::string if_name = "mic" + std::to_string(mic->id);
	mpssdi->mic_net.tap_fd = tun_alloc(mic, if_name);
	if (mpssdi->mic_net.tap_fd < 0)
		goto done;
	if (tap_configure(mic, if_name))
		goto done;
	mpssd_log(PINFO, "Start virtio net thread");

	net_poll[NET_FD_VIRTIO_NET].fd = mpssdi->mic_net.virtio_net_fd;
	net_poll[NET_FD_VIRTIO_NET].events = POLLIN;
	net_poll[NET_FD_TUN].fd = mpssdi->mic_net.tap_fd;
	net_poll[NET_FD_TUN].events = POLLIN;

	if (MAP_FAILED == init_vr(mic, mpssdi->mic_net.virtio_net_fd,
				  VIRTIO_ID_NET, &tx_vr, &rx_vr,
				  mdc->virtnet_dev_page.dd.num_vq)) {
		mpssd_log(PERROR, "init_vr failed %s", strerror(errno));
		goto done;
	}

	copy.iovcnt = 2;
	desc = get_device_desc(mic, VIRTIO_ID_NET);

	if (desc == NULL) {
		mpssd_log(PERROR, "no net device exist");
		goto done;
	}

	while (1) {
		ssize_t len;

		if (mdc->need_shutdown()) {
			break;
		}

		net_poll[NET_FD_VIRTIO_NET].revents = 0;
		net_poll[NET_FD_TUN].revents = 0;

		/* Start polling for data from tap and virtio net */
		err = poll(net_poll, 2, 1000);
		if (err == 0) {
			continue;
		}

		if (err < 0) {
			mpssd_log(PERROR, "poll failed %s", strerror(errno));
			continue;
		}

		if (net_poll[NET_FD_VIRTIO_NET].revents & POLLERR)
			mpssd_log(PINFO, "POLLERR occured on NET device for %s", mic->name.c_str());

		if (net_poll[NET_FD_VIRTIO_NET].revents & POLLHUP) {
			mpssd_log(PINFO, "POLLHUP occured on NET device for %s", mic->name.c_str());
			break;
		}

		if (!(desc->status & VIRTIO_CONFIG_S_DRIVER_OK)) {
			err = wait_for_card_driver(mdc, mic,
					mpssdi->mic_net.virtio_net_fd,
					VIRTIO_ID_NET);
			if (err) {
				mpssd_log(PINFO, "Exiting...");
					break;
			}
		}
		/*
		 * Check if there is data to be read from TUN and write to
		 * virtio net fd if there is.
		 */
		if (net_poll[NET_FD_TUN].revents & POLLIN) {
			copy.iov = iov0;
			len = readv(net_poll[NET_FD_TUN].fd,
				copy.iov, copy.iovcnt);
			if (len > 0) {
				struct virtio_net_hdr *hdr
					= (struct virtio_net_hdr *)vnet_hdr[0];

				/* Disable checksums on the card since we are on
				   a reliable PCIe link */
				hdr->flags |= VIRTIO_NET_HDR_F_DATA_VALID;
#ifdef DEBUG
				mpssd_log(PINFO, "hdr->flags 0x%x copy.out_len %d hdr->gso_type 0x%x",
					hdr->flags, copy.out_len, hdr->gso_type);

				disp_iovec(mic, &copy);
				mpssd_log(PINFO, "read from tap 0x%lx", len);
#endif
				spin_for_descriptors(mdc, mic, &tx_vr);
				txrx_prepare(VIRTIO_ID_NET, 1, &tx_vr, &copy,
					     len);

				err = mic_virtio_copy(mic,
					mpssdi->mic_net.virtio_net_fd, &tx_vr,
					&copy);
				if (err < 0) {
					mpssd_log(PINFO, "mic_virtio_copy %s", strerror(errno));
				}
				if (!err)
					verify_out_len(mic, &copy);
#ifdef DEBUG
				disp_iovec(mic, &copy);
				mpssd_log(PINFO, "wrote to net 0x%lx", sum_iovec_len(&copy));
#endif
				/* Reinitialize IOV for next run */
				iov0[1].iov_len = MAX_NET_PKT_SIZE;
			} else if (len < 0) {
				disp_iovec(mic, &copy);
				mpssd_log(PERROR, "read failed %s cnt %d sum %zd",
					strerror(errno), copy.iovcnt, sum_iovec_len(&copy));
			}
		}

		/*
		 * Check if there is data to be read from virtio net and
		 * write to TUN if there is.
		 */
		if (net_poll[NET_FD_VIRTIO_NET].revents & POLLIN) {
			while (rx_vr.info->avail_idx !=
				le16toh(rx_vr.vr.avail->idx)) {
				copy.iov = iov1;
				txrx_prepare(VIRTIO_ID_NET, 0, &rx_vr, &copy,
					     MAX_NET_PKT_SIZE
					+ sizeof(struct virtio_net_hdr));

				err = mic_virtio_copy(mic,
					mpssdi->mic_net.virtio_net_fd, &rx_vr,
					&copy);
				if (!err) {
#ifdef DEBUG
					struct virtio_net_hdr *hdr
						= (struct virtio_net_hdr *)
							vnet_hdr[1];

					mpssd_log(PINFO, "hdr->flags 0x%x, out_len %d gso_type 0x%x",
						hdr->flags, copy.out_len, hdr->gso_type);
#endif
					/* Set the correct output iov_len */
					iov1[1].iov_len = copy.out_len -
						sizeof(struct virtio_net_hdr);
					verify_out_len(mic, &copy);
#ifdef DEBUG
					disp_iovec(mic, &copy);
					mpssd_log(PINFO, "read from net 0x%lx", sum_iovec_len(&copy));
#endif
					len = writev(net_poll[NET_FD_TUN].fd,
						copy.iov, copy.iovcnt);
					if (len != sum_iovec_len(&copy)) {
						mpssd_log(PERROR, "Tun write failed %s len 0x%zx read_len 0x%zx",
							strerror(errno), len, sum_iovec_len(&copy));
					} else {
#ifdef DEBUG
						disp_iovec(mic, &copy);
						mpssd_log(PINFO, "wrote to tap 0x%lx", len);
#endif
					}
				} else {
					mpssd_log(PERROR, "mic_virtio_copy %s", strerror(errno));
					break;
				}
				if (mdc->need_shutdown()) {
					break;
				}
			}
		}
	}
done:
	munmap(mpssdi->mic_net.net_dp, mpssdi->mic_net.dp_size);
	close(mpssdi->mic_net.virtio_net_fd);
	close(mpssdi->mic_net.tap_fd);
}

/* virtio_console */
#define VIRTIO_CONSOLE_FD 0
#define MONITOR_FD (VIRTIO_CONSOLE_FD + 1)
#define MAX_CONSOLE_FD (MONITOR_FD + 1)  /* must be the last one + 1 */
#define MAX_BUFFER_SIZE PAGE_SIZE
#define MIC_TTY_PREFIX "/dev/ttyMIC"

static std::mutex pty_mutex;

int
create_monitor_console(int mic_id)
{
	int err;
	int pty_fd;
	char *pts_name;
	std::string console_fn;

	std::lock_guard<std::mutex> guard(pty_mutex);

	pty_fd = posix_openpt(O_RDWR);
	if (pty_fd < 0) {
		mpssd_log(PERROR, "can't open a pseudoterminal master device: %s",
			strerror(errno));
		goto _return;
	}

	pts_name = ptsname(pty_fd);
	if (pts_name == NULL) {
		mpssd_log(PERROR, "can't get pts name");
		goto _close_pty;
	}

	mpssd_log(PINFO, "console message goes to %s", pts_name);

	console_fn = MIC_TTY_PREFIX + std::to_string(mic_id);
	err = symlink(pts_name, console_fn.c_str());
	if (err < 0 && errno == EEXIST) {
		unlink(console_fn.c_str());
		mpssd_log(PINFO, "found old symlink to %s, removing...", console_fn.c_str());
		err = symlink(pts_name, console_fn.c_str());
	}
	if (err < 0) {
		mpssd_log(PERROR, "can't create symlink: %s", strerror(errno));
	}

	err = chmod(pts_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (err < 0) {
		mpssd_log(PERROR, "can't change mode of pseudoterminal: %s %s",
			pts_name, strerror(errno));
		goto _unlink_pty;
	}

	err = chown(pts_name, 0, 0);
	if (err < 0) {
		mpssd_log(PERROR, "can't change owner of pseudoterminal: %s %s",
			pts_name, strerror(errno));
		goto _unlink_pty;
	}

	err = unlockpt(pty_fd);
	if (err < 0) {
		mpssd_log(PERROR, "can't unlock a pseudoterminal: %s %s",
			pts_name, strerror(errno));
		goto _unlink_pty;
	}

	return pty_fd;

_unlink_pty:
	unlink(console_fn.c_str());
_close_pty:
	close(pty_fd);
_return:
	return -1;
}
int
create_virtio_console(mic_device_context *mdc, mic_info *mic,
		      mic_vring *txvr, mic_vring *rxvr)
{
	int virtcons_fd;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	struct mic_device_desc *desc;

	mpssdi->mic_console.virtio_console_fd = 0;
	add_virtio_device(mic, &mdc->virtcons_dev_page.dd);
	virtcons_fd = mpssdi->mic_console.virtio_console_fd;

	if (virtcons_fd <= 0)
		goto _return_err;

	if (MAP_FAILED == init_vr(mic, virtcons_fd, VIRTIO_ID_CONSOLE, txvr, rxvr,
				  mdc->virtcons_dev_page.dd.num_vq)) {
		mpssd_log(PERROR, "init_vr failed %s", strerror(errno));
		goto _close_pty_vr;
	}

	desc = get_device_desc(mic, VIRTIO_ID_CONSOLE);
	if (desc == NULL) {
		mpssd_log(PERROR, "no console device exist");
		goto _close_pty_vr;
	}

	return virtcons_fd;

_close_pty_vr:
	close(virtcons_fd);
_return_err:
	return -1;
}

void*
virtio_console(mic_device_context *mdc, mic_info* mic)
{
	__u8 vcons_buf[2][PAGE_SIZE];
	struct iovec vcons_iov[2] = {};
	struct iovec *iov0, *iov1;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	int err;
	struct pollfd console_poll[MAX_CONSOLE_FD];
	ssize_t len;
	struct mic_vring tx_vr, rx_vr;
	struct mic_copy_desc copy;
	struct mic_device_desc *desc;

	int monitor_fd;
	int virtcons_fd;
	std::string console_fn;

	// thread and logging settings
	virtio_log.virtio_device_type = VIRTIO_ID_CONSOLE;
	virtio_log.virtio_device_number = 1;
	set_thread_name(mpssdi->name().c_str(), "virtcon");

	// virtio fd setup
	vcons_iov[1].iov_base = vcons_buf[1];
	vcons_iov[1].iov_len = sizeof(vcons_buf[1]);
	iov1 = &vcons_iov[1];

	virtcons_fd = create_virtio_console(mdc, mic, &tx_vr, &rx_vr);
	if (virtcons_fd <= 0) {
		mpssd_log(PERROR, "cannot create CONSOLE virtio fd, aborting...");
		goto _close_pty_vr;
	}

	console_poll[VIRTIO_CONSOLE_FD].fd = virtcons_fd;
	console_poll[VIRTIO_CONSOLE_FD].events = POLLIN;

	// monitor fd setup
	vcons_iov[0].iov_base = vcons_buf[0];
	vcons_iov[0].iov_len = sizeof(vcons_buf[0]);
	iov0 = &vcons_iov[0];
	console_fn = MIC_TTY_PREFIX + std::to_string(mic->id);

	monitor_fd = create_monitor_console(mic->id);
	if (monitor_fd <= 0) {
		mpssd_log(PERROR, "cannot create CONSOLE monitor fd, aborting...");
		goto _close_pty;
	}

	console_poll[MONITOR_FD].fd = monitor_fd;
	console_poll[MONITOR_FD].events = POLLIN;

	// common setup
	copy.iovcnt = 1;
	desc = get_device_desc(mic, VIRTIO_ID_CONSOLE);

	for (;;) {
		if (mdc->need_shutdown()) {
			break;
		}

		console_poll[MONITOR_FD].revents = 0;
		console_poll[VIRTIO_CONSOLE_FD].revents = 0;
		err = poll(console_poll, MAX_CONSOLE_FD, 1000);

		if (err == 0) {
			continue;
		}

		if (err < 0) {
			mpssd_log(PERROR, "poll failed: %s", strerror(errno));
			continue;
		}

		if (console_poll[MONITOR_FD].revents & POLLHUP) {
			mpssd_log(PINFO, "POLLHUP occured on CONSOLE device, reinitializing...");
			close(monitor_fd);
			monitor_fd = create_monitor_console(mic->id);
			if (monitor_fd <= 0) {
				mpssd_log(PERROR, "CONSOLE reinitialization failed, aborting...");
				break;
			}

			console_poll[MONITOR_FD].fd = monitor_fd;
			continue;
		}

		if (console_poll[VIRTIO_CONSOLE_FD].revents & POLLERR)
			mpssd_log(PERROR, "POLLERR occured on CONSOLE device for %s", mic->name.c_str());

		if (console_poll[VIRTIO_CONSOLE_FD].revents & POLLHUP) {
			mpssd_log(PERROR, "POLLHUP occured on CONSOLE device for %s", mic->name.c_str());
			break;
		}

		if (!(desc->status & VIRTIO_CONFIG_S_DRIVER_OK)) {
			err = wait_for_card_driver(mdc, mic,
					mpssdi->mic_console.virtio_console_fd,
					VIRTIO_ID_CONSOLE);
			if (err) {
				mpssd_log(PINFO, "Exiting...");
					break;
			}
		}

		if (console_poll[MONITOR_FD].revents & POLLIN) {
			copy.iov = iov0;
			len = readv(monitor_fd, copy.iov, copy.iovcnt);
			if (len > 0) {
#ifdef DEBUG
				disp_iovec(mic, &copy);
				mpssd_log(PINFO, "read from tap 0x%lx", len);
#endif
				spin_for_descriptors(mdc, mic, &tx_vr);
				txrx_prepare(VIRTIO_ID_CONSOLE, 1, &tx_vr,
					     &copy, len);

				err = mic_virtio_copy(mic,
					mpssdi->mic_console.virtio_console_fd,
					&tx_vr, &copy);
				if (err < 0) {
					mpssd_log(PERROR, "mic_virtio_copy %s", strerror(errno));
				}
				if (!err)
					verify_out_len(mic, &copy);
#ifdef DEBUG
				disp_iovec(mic, &copy);
				mpssd_log(PINFO, "wrote to net 0x%lx", sum_iovec_len(&copy));
#endif
				/* Reinitialize IOV for next run */
				iov0->iov_len = PAGE_SIZE;
			} else if (len < 0) {
				disp_iovec(mic, &copy);
				mpssd_log(PERROR, "read failed %s cnt %d sum %zd",
					strerror(errno), copy.iovcnt, sum_iovec_len(&copy));
			}
		}

		if (console_poll[VIRTIO_CONSOLE_FD].revents & POLLIN) {
			while (rx_vr.info->avail_idx !=
				le16toh(rx_vr.vr.avail->idx)) {
				copy.iov = iov1;
				txrx_prepare(VIRTIO_ID_CONSOLE, 0, &rx_vr,
					     &copy, PAGE_SIZE);

				err = mic_virtio_copy(mic,
					mpssdi->mic_console.virtio_console_fd,
					&rx_vr, &copy);
				if (!err) {
					/* Set the correct output iov_len */
					iov1->iov_len = copy.out_len;
					verify_out_len(mic, &copy);
#ifdef DEBUG
					disp_iovec(mic, &copy);
					mpssd_log(PINFO, "read from net 0x%lx", sum_iovec_len(&copy));
#endif
					len = writev(monitor_fd,
						copy.iov, copy.iovcnt);
					if (len != sum_iovec_len(&copy)) {
						mpssd_log(PERROR, "Tun write failed %s len 0x%zx read_len 0x%zx",
							strerror(errno), len, sum_iovec_len(&copy));
					} else {
#ifdef DEBUG
						disp_iovec(mic, &copy);
						mpssd_log(PINFO, "wrote to tap 0x%lx", len);
#endif
					}
				} else {
					mpssd_log(PINFO, "mic_virtio_copy %s", strerror(errno));
					break;
				}
				if (mdc->need_shutdown()) {
					break;
				}
			}
		}
	}

_close_pty:
	unlink(console_fn.c_str());
	close(monitor_fd);

_close_pty_vr:
	munmap(mpssdi->mic_console.console_dp, mpssdi->mic_console.dp_size);
	close(virtcons_fd);

	return 0;
}

static bool
set_backend_file(mic_device_context *mdc, struct mic_info *mic, int idx)
{
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	std::string target = mic->config.blockdevs[idx].source;
	mpssdi->mic_virtblk[idx].backend_file = target;
	return true;
}

#define SECTOR_SIZE 512
static bool
set_backend_size(mic_device_context *mdc, struct mic_info *mic, int idx)
{
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;

	mpssdi->mic_virtblk[idx].backend_size = lseek(mpssdi->mic_virtblk[idx].backend, 0,
		SEEK_END);
	if (mpssdi->mic_virtblk[idx].backend_size < 0) {
		mpssd_log(PERROR, "can't seek: %s",
			mpssdi->mic_virtblk[idx].backend_file.c_str());
		return false;
	}
	mdc->virtblk_dev_page[idx].blk_config.capacity =
		mpssdi->mic_virtblk[idx].backend_size / SECTOR_SIZE;
	if ((mpssdi->mic_virtblk[idx].backend_size % SECTOR_SIZE) != 0)
		mdc->virtblk_dev_page[idx].blk_config.capacity++;

	mdc->virtblk_dev_page[idx].blk_config.capacity =
		htole64(mdc->virtblk_dev_page[idx].blk_config.capacity);

	return true;
}

static bool
open_backend(mic_device_context *mdc, struct mic_info *mic, int idx)
{
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;

	if (!set_backend_file(mdc, mic, idx))
		goto _error_exit;
	mpssdi->mic_virtblk[idx].backend = open(mpssdi->mic_virtblk[idx].backend_file.c_str(), O_RDWR);
	if (mpssdi->mic_virtblk[idx].backend < 0) {
		mpssd_log(PERROR, "can't open: %s", mpssdi->mic_virtblk[idx].backend_file.c_str());
	}
	if (!set_backend_size(mdc, mic, idx))
		goto _error_close;
	mpssdi->mic_virtblk[idx].backend_addr = mmap(NULL,
		mpssdi->mic_virtblk[idx].backend_size,
		PROT_READ|PROT_WRITE, MAP_SHARED,
		mpssdi->mic_virtblk[idx].backend, 0L);
	if (mpssdi->mic_virtblk[idx].backend_addr == MAP_FAILED) {
		mpssd_log(PERROR, "can't map: %s %s", mpssdi->mic_virtblk[idx].backend_file.c_str(), strerror(errno));
		goto _error_close;
	}
	return true;

 _error_close:
	close(mpssdi->mic_virtblk[idx].backend);
 _error_exit:
	return false;
}

static void
close_backend(struct mic_info *mic, int idx)
{
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;

	munmap(mpssdi->mic_virtblk[idx].backend_addr, mpssdi->mic_virtblk[idx].backend_size);
	close(mpssdi->mic_virtblk[idx].backend);
}

static bool
add_virtblk_device(mic_device_context *mdc, struct mic_info *mic, int idx)
{
	if (((unsigned long)&mdc->virtblk_dev_page[idx].blk_config % 8) != 0) {
		mpssd_log(PERROR, "blk_config is not 8 byte aligned");
		return false;
	}

	add_virtio_device(mic, &mdc->virtblk_dev_page[idx].dd, idx);
	return true;
}

static void
remove_virtblk_device(mic_device_context *mdc, struct mic_info *mic, int idx)
{
	int vr_size, ret;
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;

	vr_size = PAGE_ALIGN(_vring_size(MIC_VRING_ENTRIES,
		MIC_VIRTIO_RING_ALIGN) + sizeof(struct _mic_vring_info));
	ret = munmap(mpssdi->mic_virtblk[idx].block_dp,
		MIC_DEVICE_PAGE_END + vr_size * mdc->virtblk_dev_page[idx].dd.num_vq);
	if (ret < 0)
		mpssd_log(PERROR, "munmap errno %d", errno);
	close(mpssdi->mic_virtblk[idx].virtio_block_fd);
}

static __u8
header_error_check(struct vring_desc *desc)
{
	if (le32toh(desc->len) != sizeof(struct virtio_blk_outhdr)) {
		mpssd_log(PERROR, "length is not sizeof(virtio_blk_outhd)");
		return -EIO;
	}
	if (!(le16toh(desc->flags) & VRING_DESC_F_NEXT)) {
		mpssd_log(PERROR, "alone");
		return -EIO;
	}
	if (le16toh(desc->flags) & VRING_DESC_F_WRITE) {
		mpssd_log(PERROR, "not read");
		return -EIO;
	}
	return 0;
}

static int
read_header(int fd, struct virtio_blk_outhdr *hdr, __u32 desc_idx)
{
	struct iovec iovec;
	struct mic_copy_desc copy;

	iovec.iov_len = sizeof(*hdr);
	iovec.iov_base = hdr;
	copy.iov = &iovec;
	copy.iovcnt = 1;
	copy.vr_idx = 0;  /* only one vring on virtio_block */
	copy.update_used = false;  /* do not update used index */
	return ioctl(fd, MIC_VIRTIO_COPY_DESC, &copy);
}

static int
transfer_blocks(int fd, struct iovec *iovec, __u32 iovcnt)
{
	struct mic_copy_desc copy;

	copy.iov = iovec;
	copy.iovcnt = iovcnt;
	copy.vr_idx = 0;  /* only one vring on virtio_block */
	copy.update_used = false;  /* do not update used index */
	return ioctl(fd, MIC_VIRTIO_COPY_DESC, &copy);
}

static __u8
status_error_check(struct vring_desc *desc)
{
	if (le32toh(desc->len) != sizeof(__u8)) {
		mpssd_log(PERROR, "length is not sizeof(status)");
		return -EIO;
	}
	return 0;
}

static int
write_status(int fd, __u8 *status)
{
	struct iovec iovec;
	struct mic_copy_desc copy;

	iovec.iov_base = status;
	iovec.iov_len = sizeof(*status);
	copy.iov = &iovec;
	copy.iovcnt = 1;
	copy.vr_idx = 0;  /* only one vring on virtio_block */
	copy.update_used = true; /* Update used index */
	return ioctl(fd, MIC_VIRTIO_COPY_DESC, &copy);
}

#ifndef VIRTIO_BLK_T_GET_ID
#define VIRTIO_BLK_T_GET_ID    8
#endif

void
virtio_block(mic_device_context *mdc, mic_info *mic, int idx)
{
	struct mpssd_info *mpssdi = (struct mpssd_info *)mic->data;
	int ret;
	struct pollfd block_poll;
	struct mic_vring vring;
	__u16 avail_idx;
	__u32 desc_idx;
	struct vring_desc *desc;
	struct iovec *iovec, *piov;
	__u8 status;
	__u32 buffer_desc_idx;
	struct virtio_blk_outhdr hdr;
	void *fos;

	virtio_log.virtio_device_type = VIRTIO_ID_BLOCK;
	virtio_log.virtio_device_number = idx + 1;
	set_thread_name(mpssdi->name().c_str(), "virtblk");

	if (MAP_FAILED == init_vr(mic, mpssdi->mic_virtblk[idx].virtio_block_fd,
				  VIRTIO_ID_BLOCK, &vring, NULL,
				  mdc->virtblk_dev_page[idx].dd.num_vq, idx)) {
		mpssd_log(PERROR, "init_vr failed %s", strerror(errno));
		goto remove_virtblk;
	}

	for (;;) {  /* forever */
		if (mdc->need_shutdown()) {
			break;
		}

		iovec = (struct iovec *) malloc(sizeof(*iovec) *
						le32toh(mdc->virtblk_dev_page[idx].blk_config.seg_max));
		if (!iovec) {
			mpssd_log(PERROR, "can't alloc iovec: %s", strerror(ENOMEM));
			goto remove_virtblk;
		}

		block_poll.fd = mpssdi->mic_virtblk[idx].virtio_block_fd;
		block_poll.events = POLLIN;
		for (mpssdi->mic_virtblk[idx].signaled = 0;
		     !mpssdi->mic_virtblk[idx].signaled;) {

			if (mdc->need_shutdown()) {
				goto free_iovec;
			}
			block_poll.revents = 0;
			/* timeout in 1 sec to see signaled */
			ret = poll(&block_poll, 1, 1000);

			if (ret == 0) {
				continue;
			}
			if (ret < 0) {
				mpssd_log(PERROR, "poll failed: %s", strerror(errno));
				continue;
			}
			if (block_poll.revents & POLLHUP) {
				mpssd_log(PINFO, "POLLHUP closing BLOCK device");
				/* device not initialized already, so exiting thread
				 * to avoid vop_poll flood */
				free(iovec);
				goto remove_virtblk;

			}
			if (block_poll.revents & POLLERR) {
				mpssd_log(PERROR, "POLLERR on BLOCK device %d", idx);
				continue;
			}

			/* POLLIN */
			while (vring.info->avail_idx !=
				le16toh(vring.vr.avail->idx)) {
				/* read header element */
				avail_idx =
					vring.info->avail_idx &
					(vring.vr.num - 1);
				desc_idx = le16toh(
					vring.vr.avail->ring[avail_idx]);
				desc = &vring.vr.desc[desc_idx];
#ifdef DEBUG
				mpssd_log(PINFO, "avail_idx=%d vring.vr.num=%d desc=%p",
					vring.info->avail_idx, vring.vr.num, desc);
#endif
				status = header_error_check(desc);
				if (status) {
					mpssd_log(PERROR, "header_error_check status=%d %s",
						  status, strerror(status));
					free(iovec);
					goto remove_virtblk;
				}

				ret = read_header(
					mpssdi->mic_virtblk[idx].virtio_block_fd,
					&hdr, desc_idx);
				if (ret < 0) {
					mpssd_log(PERROR, "ret=%d %s", ret, strerror(errno));
					break;
				}
				/* buffer element */
				piov = iovec;
				status = 0;
				fos = (void *) ((char *) mpssdi->mic_virtblk[idx].backend_addr +
					(hdr.sector * SECTOR_SIZE));
				buffer_desc_idx = next_desc(desc);
				for (desc = &vring.vr.desc[buffer_desc_idx];
				     desc->flags & VRING_DESC_F_NEXT;
				     desc_idx = next_desc(desc),
					     desc = &vring.vr.desc[desc_idx]) {
					piov->iov_len = desc->len;
					piov->iov_base = fos;
					piov++;
					fos = (void *) ((char *) fos + desc->len);
				}
				/* Returning NULLs for VIRTIO_BLK_T_GET_ID. */
				if (hdr.type & ~(VIRTIO_BLK_T_OUT |
					VIRTIO_BLK_T_GET_ID)) {
					/*
					  VIRTIO_BLK_T_IN - does not do
					  anything. Probably for documenting.
					  VIRTIO_BLK_T_SCSI_CMD - for
					  virtio_scsi.
					  VIRTIO_BLK_T_FLUSH - turned off in
					  config space.
					  VIRTIO_BLK_T_BARRIER - defined but not
					  used in anywhere.
					*/
					mpssd_log(PERROR, "type %x is not supported", hdr.type);
					status = -ENOTSUP;

				} else {
					ret = transfer_blocks(
					mpssdi->mic_virtblk[idx].virtio_block_fd,
						iovec,
						piov - iovec);
					if (ret < 0 &&
					    status != 0)
						status = ret;
				}
				/* write status and update used pointer */
				if (status != 0)
					status = status_error_check(desc);
				ret = write_status(
					mpssdi->mic_virtblk[idx].virtio_block_fd,
					&status);
				if (ret < 0)
					mpssd_log(PERROR, "write status failed: %d", ret);
#ifdef DEBUG
				mpssd_log(PINFO, "write status=%d on desc=%p", status, desc);
#endif
				if (mdc->need_shutdown()) {
					break;
				}
			}
		}
free_iovec:
		free(iovec);
	}  /* forever */
remove_virtblk:
	remove_virtblk_device(mdc, mic, idx);
	mpssd_log(PINFO, "virtblk stopped for block device %d", idx);

	close_backend(mic, idx);
	mpssd_log(PINFO, "backend file closed for block device %d", idx);
	mpssd_log(PINFO, "exiting thread for block device %d", idx);
}

bool
setup_virtio_block(struct mic_info* mic, mic_device_context* mdc, int idx)
{
	bool read_only = (mic->config.blockdevs[idx].mode
			  == mpss_block_device_mode::READ_ONLY);
	mdc->virtblk_dev_page[idx] = virtblk_dev_page_init(read_only);

	mpssd_log(PINFO, "block device %d configured as %s", idx,
		  (read_only) ? READ_ONLY_MODE : READ_WRITE_MODE);

	mpssd_log(PINFO, "opening backend file for block device %d", idx);
	if (!open_backend(mdc, mic, idx)) {
		mpssd_log(PERROR, "can't open backend file");
		return false;
	}

	mpssd_log(PINFO, "adding block device %d", idx);
	if (!add_virtblk_device(mdc, mic, idx)) {
		mpssd_log(PERROR, "can't add virblk device");
		close_backend(mic, idx);
		return false;
	}

	return true;
}
