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

#pragma once

#include "libmpsscommon.h"

#include <limits.h>
#include <memory>
#include <signal.h>
#include <string>
#include <vector>

/*
 * Section 2: APIs for controlling the enviroment mpss configuration is used in.
 */
#ifndef MPSS_CONFIG_FILE
#define MPSS_CONFIG_FILE "/etc/sysconfig/mpss.conf"
#endif

#ifndef MICSYSFSDIR
#define MICSYSFSDIR "/sys/class/mic"
#endif

#ifndef DEFAULT_CONFDIR
#define DEFAULT_CONFDIR "/etc/mpss"
#endif

#ifndef DEFAULT_VARDIR
#define DEFAULT_VARDIR "/var/mpss"
#endif

#ifndef DEFAULT_SRCDIR
#define DEFAULT_SRCDIR "/usr/share/mpss/boot"
#endif

#ifndef DEFAULT_REPODIR
#define DEFAULT_REPODIR "/usr/share/mpss/boot"
#endif

/*
 * Knightslanding
 */
#ifndef DEFAULT_BZIMAGE_KNL
#define DEFAULT_BZIMAGE_KNL "bzImage-knl-lb"
#endif

#ifndef DEFAULT_SYSMAP_KNL
#define DEFAULT_SYSMAP_KNL "System.map-knl-lb"
#endif

#ifndef DEFAULT_INITRD_KNL
#define DEFAULT_INITRD_KNL "initramfs-knl-lb.cpio.gz"
#endif

#ifndef DEFAULT_EFIIMAGE_KNL
#define DEFAULT_EFIIMAGE_KNL "efiImage-knl-lb.hddimg"
#endif

#ifndef DEFAULT_PFS_KNL
#define DEFAULT_PFS_KNL	"persistent-FS-knl-lb"
#endif

#ifndef DEFAULT_PFSREPO_KNL
#define DEFAULT_PFSREPO_KNL "repo-card.squashfs"
#endif

/**
 * Mic family
*/
#define MIC_FAMILY_KNL		"x200"
#define MIC_FAMILY_UNKNOWN	"unknown"

/*
 * MPSS Version
 */
#define MPSS_VERSION_3		"3.x"
#define MPSS_VERSION_4		"4.x"
#define MPSS_VERSION_UNKNOWN	"unknown"

#define LSB_LOCK_FILENAME "/var/lock/mpss"

/*
 * Section 3: APIs for finding MIC cards.
 */
/**
 * mpss_get_miclist - retrieve a linked list of known MIC instances
 *     \param mic_cnt	Returns the number of MIC configurations found
 *     \param use_config_files Whether we should also search in the configuration files.
 *
 * mpss_get_miclist() searches /sys/class/mic directory (and if use_config_files is set to true
 * also the configuration files) for instance of the MIC card. If found in both locations
 * it merges the MIC card entries and returns a linked list of mic_info structure.
 * Each structure will have an entry for a MIC card with its name
 * and ID Filled out and config structure initialized.
 *
 *\return
 * Upon successful completion mpss_get_miclist returns a pointer to the
 * first element in the chain.  If no MIC cards are found it returns a NULL
 * pointer.
**/
MPSS_EXPORT std::shared_ptr<struct mic_info> mpss_get_miclist(int *mic_cnt, bool use_config_files = false);

MPSS_EXPORT void
mpss_free_miclist(struct mic_info *miclist);

/**
 * mpss_find_micid_inlist - Find the correct MIC entry if the ID.s
 *     \param miclist	List to MIC structures to search in
 *     \param micid	ID of MIC card to find
 *
 * mpss_findmic_inlist() searches the linked list of MIC cards for the
 * specified ID.
**/
MPSS_EXPORT struct mic_info *mpss_find_micid_inlist(struct mic_info *miclist, int micid);

#define PERROR	0
#define PWARN	1
#define PNORM	2
#define PINFO	3
#define PALL	10

struct mpss_error {
	int 		level;
	std::string 	message;
};

class MPSS_EXPORT mpss_elist {
	std::vector<mpss_error>	list;
	int			ecount[4];

	mpss_elist(const mpss_elist&);
	void operator=(const mpss_elist&);
public:
	mpss_elist();
	void print(int level, void (*func)(int level, const char* fmt, ...));
	void clear();
	void add(int level, const char* format, ...) __attribute__((format(printf,3,4)));
	int error_count() const { return this->ecount[PERROR]; }
};

/*
 * Data structure containing parsed configuration data
 */

struct mic_console_info {
	pthread_t       console_thread;
	int             virtio_console_fd;
	void            *console_dp;
	size_t          dp_size;
};

struct mic_net_info {
	pthread_t       net_thread;
	int             virtio_net_fd;
	int             tap_fd;
	void            *net_dp;
	size_t          dp_size;
};

struct mic_virtblk_info {
	pthread_t       block_thread;
	int             virtio_block_fd;
	void            *block_dp;
	volatile sig_atomic_t   signaled;
	std::string     backend_file;
	int             backend;
	void            *backend_addr;
	long            backend_size;
	size_t          dp_size;
};

struct mroot {		// RootDev parameter
#define ROOT_TYPE_PFS		0
	int		type;
	std::string 	target;
};

const int BLOCK_MAX_COUNT = 8;

const int BLOCK_OPTION_ROOT = (1 << 0);
const int BLOCK_OPTION_REPO = (1 << 1);

const char* const BLOCK_NAME_ROOT = "root";
const char* const BLOCK_NAME_REPO = "repo";

const char* const MIC_DISABLED = "Disabled";
const char* const MIC_ENABLED = "Enabled";

const char* const READ_ONLY_MODE = "read-only";
const char* const READ_WRITE_MODE = "read-write";

const char* const CONFIG_KERNEL_IMAGE = "KernelImage";
const char* const CONFIG_KERNEL_SYMBOLS = "KernelSymbols";
const char* const CONFIG_EFI_IMAGE = "EfiImage";
const char* const CONFIG_AUTO_BOOT = "AutoBoot";
const char* const CONFIG_HOST_NET = "HostNetworkConfig";
const char* const CONFIG_CARD_NET = "CardNetworkConfig";
const char* const CONFIG_HOST_MAC = "HostMacAddress";
const char* const CONFIG_CARD_MAC = "CardMacAddress";
const char* const CONFIG_KERNEL_CMDLINE = "KernelExtraCommandLine";
const char* const CONFIG_INIT_RAMFS = "InitRamFsImage";
const char* const CONFIG_BLOCK_DEVICE = "BlockDevice";
const char* const CONFIG_ROOTFS_IMAGE = "RootFsImage";
const char* const CONFIG_REPOFS_IMAGE = "RepoFsImage";
const char* const CONFIG_SHUTDOWN_TIMEOUT = "ShutdownTimeout";
const char* const CONFIG_BOOT_TIMEOUT = "BootTimeout";

const int SHUTDOWN_TIMEOUT_DEFAULT = 60;
const int BOOT_TIMEOUT_DEFAULT = 90;

enum class mpss_block_device_mode {
	READ_ONLY,
	READ_WRITE
};

struct mblock {	// BlockDevice
	uint64_t options;
	std::string source;
	std::string dest;
	mpss_block_device_mode mode;
};

struct MPSS_EXPORT mpss_mac {
	enum type {
		UNKNOWN,
		SERIAL_HOST,
		SERIAL_MIC,
		RANDOM,
		FIXED
	};

private:
	type         m_type;
	std::string  m_address;
	std::string  m_device;

	void reload_serial();

public:
	mpss_mac() : m_type(UNKNOWN) { }

	bool set_serial(const std::string& mic_name, bool host_side);
	bool set_random();
	bool set_fixed(const std::string& address);

	type type() const {
		return m_type;
	}

	std::string address();

	bool to_mac_48(uint8_t mac[6]);

	std::string type_str() const;
	std::string str();

	void clear() {
		m_address.clear();
		m_device.clear();
		m_type = UNKNOWN;
	}
};


enum class mpss_host_network_type {
	NONE,
	UNKNOWN,
	STATIC,
	BRIDGE
};

enum class mpss_card_network_type {
	NONE,
	UNKNOWN,
	STATIC,
	DHCP
};

struct mnetwork_opt {
	std::string ip;
	std::string netmask;
	std::string gateway;
	std::string dhcp_hostname;

	void clear() {
		ip.clear();
		netmask.clear();
		gateway.clear();
		dhcp_hostname.clear();
	}
};

struct mnet {		// Bridge, NetWork, MacAddr
	mpss_host_network_type	host_type;
	mpss_host_network_type	host_type_inet6;
	std::string	bridge_name;
	mnetwork_opt host_setting;
	mnetwork_opt host_setting_inet6;
	mpss_card_network_type card_type;
	mpss_card_network_type card_type_inet6;
	mnetwork_opt card_setting;
	mnetwork_opt card_setting_inet6;
	mpss_mac host_mac;
	mpss_mac mic_mac;

	void clear() {
		host_type = mpss_host_network_type::NONE;
		host_type_inet6 = mpss_host_network_type::NONE;
		bridge_name.clear();
		host_setting.clear();
		host_setting_inet6.clear();
		card_type = mpss_card_network_type::NONE;
		card_type_inet6 = mpss_card_network_type::NONE;
		card_setting.clear();
		card_setting_inet6.clear();
		host_mac.clear();
		mic_mac.clear();
	}
};

struct msource {	// Base, CommonDir, MicDir
	std::string	image;
};

struct mfiles {		// Congregate two above
	struct msource	base;
};

struct mboot {		// BootOnStart, OsImage, misc. etc.
	int		onstart;
	std::string	osimage;
	std::string	efiimage;
	std::string	systemmap;
	std::string	extraCmdline;
};

struct mmisc {
	mmisc() :
		shutdowntimeout(SHUTDOWN_TIMEOUT_DEFAULT),
		boottimeout(BOOT_TIMEOUT_DEFAULT) { }

	int	shutdowntimeout;
	int	boottimeout;
};

struct mconfig {
	std::string		name;
	struct mmisc		misc;
	struct mboot		boot;
	struct mroot		rootdev;
	struct mblock		blockdevs[BLOCK_MAX_COUNT];
	int			blocknum;
	struct mfiles		filesrc;
	struct mnet		net;
};

/*
 * Top level data structure for configuration data.
 */
struct mic_info {
mic_info() :
	id(NOTSET),
	present(NOTSET),
	config(),
	data(nullptr),
	next(nullptr) {
	}

	int		id;		// First element - do not move
	std::string	name;
	int		present;
	struct mconfig	config;
	void		*data;		// Void pointer for apps attaching data
	struct mic_info *next;
};

class mic_info_iterator {
	mic_info* m_current;
public:
	mic_info_iterator(mic_info* ptr): m_current(ptr) {}
	void operator++() { m_current = m_current->next; }
	mic_info& operator*() { return *m_current; }
	bool operator!=(mic_info_iterator rhs) const { return m_current != rhs.m_current; }
};

inline mic_info_iterator begin(mic_info* miclist) { return mic_info_iterator(miclist); }
inline mic_info_iterator end(mic_info* miclist) { return mic_info_iterator(0); }

inline mic_info_iterator begin(std::shared_ptr<mic_info> miclist) { return begin(miclist.get()); }
inline mic_info_iterator end(std::shared_ptr<mic_info> miclist) { return end(miclist.get()); }

MPSS_EXPORT std::string get_module_version(const std::string& mod_name);

/**
 * mpss_parse_config - parse the MPSS config files and fill in config struct
 *     \param mic	Pointer to MIC card info
 *     \param perrs	List to return error infomation
 *
 * mpss_parse_config() parses the MPSS configuration files for the specified
 * MIC card passed in and fills in it configuration data.
 *
 *\return
 * Upon successful completion mpss_parse_config returns 0.
 *\par Errors:
 *- ENOENT
 * -If the configuration file for the specified mic card could not be found
**/
MPSS_EXPORT int mpss_parse_config(struct mic_info *mic, mpss_elist& perrs);

/**
 * mpss_clear_config - clear the MPSS config data structure
 *     \param config		Data to be cleated
 *
 * mpss_clear_config() frees any pointers to data in the structure
 * and sets them to NULL.  Any non pointer data is set to a default value.
**/
MPSS_EXPORT void mpss_clear_config(struct mconfig *config);

/**
 * mpss_opensysfs - open the sysfs 'entry' under mic card 'name'
 *     \param name	Name of the mic card (i.e. mic0, mic1, ...)
 *     \param entry	Particular sysfs entry under 'name' to open
 *
 * mpss_opensysfs() opens the sysfs entry under /sys/class/mic/<name>/<entry>
 * and returns the file discriptor.
 *
 *\return
 * Upon successful completion an open file descriptor is returned.  If a
 * failure occurs it will return a -1 and errno values will correspond to
 * documentation for open(2).
*/
MPSS_EXPORT int mpss_opensysfs(const std::string& name, const char *entry);

/**
 * mpss_readsysfs - read the sysfs 'entry' under mic card 'name'
 *     \param name	Name of the mic card (i.e. mic0, mic1, ...)
 *     \param entry	Particular sysfs entry under 'name' to open
 *
 * mpss_readsysfs() reads the output from /sys/class/mic/<name>/<entry>
 * and returns a malloced string including it.  After use is finished the
 * caller should free() this address.
 *
 *\return
 * Upon successful completion an address containg the string of information.
 * If the data could not be read then NULL will be returned.
*/
MPSS_EXPORT std::string mpss_readsysfs(const std::string& name, const char *entry, const char *directory = MICSYSFSDIR);

/**
 * mpss_setsysfs - open the sysfs 'entry' under mic card 'name'
 *     \param name	Name of the mic card (i.e. mic0, mic1, ...)
 *     \param entry	Particular sysfs entry under 'name' to open
 *     \param value	String to write
 *
 * mpss_setsysfs() opens the sysfs entry under /sys/class/mic/<name>/<entry>
 * and attempts to write the string in 'value' to it.
 *
 *\return
 * Upon successful completion an open file descriptor is returned.  If a
 * failure occurs it will return zero.  If a negative value is returned then
 * the sysfs entry failed to open.  A positive return has the value of errno
 * returned from the write to the entry.
*/
MPSS_EXPORT int mpss_setsysfs(const std::string& name, const char *entry, const char *value);
inline int mpss_setsysfs(const std::string& name, const char *entry, const std::string& value)
{
	return mpss_setsysfs(name, entry, value.c_str());
}

/**
 * mpss_set_cmdline - create and write the kernel command line
 *     \param mic	mic_info data structre with valid config data
 *     \param cmdline	Space to return command line being set
 *
 * mpss_set_cmdline() uses the parsed configuartion data in the mic_info
 * structure passed in to create the kernel command line for the MIC
 * card.  It then writes it to the cards "cmdline" sysfs entry.
 *
 * If the cmdline argument is not NULL, the created cmdline will be copied
 * to this location.
 *
 *\return
 * Upon successful completion a zero value is returned.  Non zero value
 * indicates failure.
*/
MPSS_EXPORT int mpss_set_cmdline(struct mic_info *mic, std::string *cmdline);

