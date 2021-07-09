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

#include "micctrl.h"
#include "help.h"

#include <mpssconfig.h>
#include <libmpsscommon.h>

#include <fstream>
#include <sys/types.h>
#include <dirent.h>

void initdefaults(struct mic_info *miclist, bool force, int remove_mode);
void remove_mic_configuration(struct mic_info *miclist, bool force, int remove_mode);

const std::string mpss_limits_file = "/etc/security/limits.d/99-mpss.conf";

/*
 * micctrl --initdefaults [MIC list]
 *
 * Initializes default configuration. If configuration already exists, removes old
 * configuration files and creates new ones. Creates the default.conf file shared by
 * all coprocessors and one micN.conf file for each coprocessor in the system.
 *
 */

#define INIT_COMMONSUBS \
"\
    --keep-config\n\
        Keep configuration files. File system image files are replaced.\n\
\n\
    --keep-image\n\
        Keep file system image files. Configuration files are replaced.\n\
\n\
    -f\n\
    --force\n\
        Do not prompt user about removing configuration and/or image files.\n\
\n"

const char *config_help =
/* Initdefaults Help */
"\
micctrl --initdefaults [sub options] [list of coprocessors]\n\
\n\
The 'micctrl --initdefaults' command has two purposes. If no configuration\n\
currently exists, it creates configuration files (with the default values) and\n\
file system image files for each coprocessor. If those files already exist, it\n\
removes them and replaces them with new ones. New files will contain default\n\
configuration.\n\
\n"
COMMON_HELP
INIT_COMMONSUBS;

void run_command_initdefaults(po::variables_map &vm, const std::vector<std::string> &options)
{
	int remove_mode = REMOVE_ALL;
	bool force = false;

	if (vm.count("help")) {
		micctrl_help(config_help);
		exit(0);
	}

	po::options_description init_options;
	init_options.add_options()
			("keep-config", "")
			("keep-image", "")
			("force,f", "");

	po::store(po::command_line_parser(options).options(init_options).run(), vm);

	if (vm.count("keep-image"))
		remove_mode = REMOVE_CONFIG;
	if (vm.count("keep-config"))
		remove_mode = REMOVE_IMAGE;
	if (vm.count("force"))
		force = true;

	check_rootid();

	if (load_mic_modules()) {
		display(PERROR, "cannot load mic modules\n");
		exit(EXIT_FAILURE);
	}

	std::shared_ptr<mic_info> miclist = create_miclist(vm);

	if (is_mic_images_being_used(miclist.get())) {
		display(PERROR, "cannot use selected option while the coprocessor images are being used\n");
		exit(EXIT_FAILURE);
	}

	remove_mic_configuration(miclist.get(), force, remove_mode);
	initdefaults(miclist.get(), force, remove_mode);
	exit(0);
}

const char *knlextracmd =
	"\"loglevel=8\"\n";

const char *micconfhead =
	"# Include configuration shared by all coprocessors\n"
	"Include default.conf\n";

const char *config_file_path_pattern = "%s/%s.conf";
const char *config_orig_file_path_pattern = "%s/%s.conf.orig";

/*
 * Create the initial defaults for all coprocessors in the system.
 */
void
initdefaults(struct mic_info *miclist, bool force, int remove_mode)
{
	if (!mpssut_mksubtree(DEFAULT_CONFDIR, 0, 0, 0755))
		display(PFS, "Created directory %s\n", DEFAULT_CONFDIR);

	if (!mpssut_mksubtree(DEFAULT_VARDIR, 0, 0, 0755))
		display(PFS, "Created directory %s\n", DEFAULT_VARDIR);

	if (remove_mode & REMOVE_CONFIG) {
		std::ofstream default_stream(DEFAULT_CONFDIR "/default.conf");
		if (default_stream.is_open()) {
			output_mpss_default_config(default_stream);
			display(PFS, "Created default.conf\n");
		} else {
			display(PFS, "default.conf file could not be created\n");
		}
	}

	if (remove_mode & REMOVE_IMAGE) {
		display(PNORM, "Creating default file system for coprocessors.\n"
			"This operation may take up to several minutes, depending\n"
			"on the number of coprocessors in the host system.\n");
	}

	for (auto& mic: miclist) {
		if (remove_mode & REMOVE_CONFIG) {
			std::ofstream stream(DEFAULT_CONFDIR "/" + mic.name + ".conf");
			if (stream.is_open()) {
				output_mpss_config(&mic, stream);
				display(PFS, "%s: Created mic%d.conf\n", mic.name.c_str(), mic.id);
			} else {
				display(PFS, "%s: mic%d.conf file could not be created\n", mic.name.c_str(), mic.id);
			}
			micctrl_parse_config(&mic, mpssperr, PERROR);
		}
		prepare_pfs_root_device(&mic, remove_mode);
	}

	if (!force) {
		display(PNORM, "Offloading applications may require increasing the memory lock limit.\n"
			"For more details please refer to Section 7.4.3 of the MPSS User's Guide.\n"
			"Do you want to change it to 'unlimited' for all users? [y/N]? ");

		int user_response = prompt_user();
		if (user_response) {
			//create file /etc/security/limits.d/99-mpss.conf
			std::ofstream limits_stream(mpss_limits_file);
			if (limits_stream.is_open()) {
				limits_stream << "*\t" << "soft\t" << "memlock\t" << "unlimited\n";
				limits_stream << "*\t" << "hard\t" << "memlock\t" << "unlimited\n";
				display(PFS, ("Created " + mpss_limits_file).c_str());
			} else {
				display(PERROR, (mpss_limits_file + " file could not be created)\n").c_str());
			}
		}
	}
}

/*
 * Set all configuration information to parameters in the configuration files.  Should
 * be run after a MPSS stack update to ensure deprecated parameters get changed into
 * current ones.
 */
void delete_conffile(const std::string& name, int backup);
int check_no_configs();

/*
 * Completely cleanup the MIC configuration.  Also removes all files in MicDir.
 */
void
remove_mic_configuration(struct mic_info *miclist, bool force, int remove_mode)
{
	char filename[PATH_MAX];

	bool config_exist = false;
	for (auto& mic: miclist) {
		if (mpssut_filename(filename, PATH_MAX, config_file_path_pattern, DEFAULT_CONFDIR, mic.name.c_str())) {
			config_exist = true;
			break;
		}
	}

	//if force flag is not set and any of the micN.conf files exist, then we have to ask user
	if (config_exist && !force) {
		display(PNORM, "Do you want to overwrite image or configuration files [y/N]? ");

		int user_response = prompt_user();
		if (!user_response) {
			exit(EXIT_FAILURE);
		}
	}

	/*
	 * Reading configuration file is being done in other loop then
	 * removing it (and removing PFS image) to avoid displaying error
	 * message about non existent files (which could come from config file parser).
	 *
	 * PFS image can be shared between multiple cards, so each card can have path to
	 * the same PFS image.
	 * In the first loop we just read the config files, while in the second one,
	 * we remove them (if the same PFS image is used in multiple cards, it will be
	 * remove only once).
	 *
	 * Reading configuration file only if this file exists
	 */
	for (auto& mic: miclist) {
		if (mpssut_filename(filename, PATH_MAX, config_file_path_pattern, DEFAULT_CONFDIR, mic.name.c_str())) {
			micctrl_parse_config(&mic, mpssperr, PERROR);
		}
	}

	for (auto& mic: miclist) {
		if (remove_mode & REMOVE_CONFIG) {
			delete_conffile(mic.name, FALSE);
		}

		if (remove_mode & REMOVE_IMAGE) {

			//if rootdev.target exists, this means that the config file was read
			//so we remove file from the location in config file
			std::string path;
			if (!mic.config.rootdev.target.empty()) {
				std::size_t pos;
				if ((pos = mic.config.rootdev.target.find(":")) == std::string::npos)
					path = mic.config.rootdev.target;
				else
					path = mic.config.rootdev.target.substr(pos + 1);
			} else {
				//create default location for PFS image
				path = DEFAULT_VARDIR "/" DEFAULT_PFS_KNL "-" +
						mic.name + std::string(".ext4");
			}

			if (mpssut_filename(filename, PATH_MAX, "%s", path.c_str())) {
				display(PFS, "%s: Remove file %s\n", mic.name.c_str(), filename);
				mpssut_deltree(filename);
			}
		}
	}

	if (check_no_configs() && (remove_mode & REMOVE_CONFIG)) {
		delete_conffile("default", FALSE);
	}
}

/*
 * Returns true if no config are valid config file names of the pattern mic<id>.conf where
 * id is all digits.
 */
int
check_no_configs(void)
{
	char confdir[PATH_MAX];
	char *id;
	char *ext;
	struct dirent *file;
	DIR *dp;
	int ret = TRUE;
	int nondigit = FALSE;

	mpssut_filename(confdir, PATH_MAX, "%s", DEFAULT_CONFDIR);

	if ((dp = opendir(confdir))) {
		while ((file = readdir(dp)) != NULL) {
			ext = &file->d_name[strlen(file->d_name) - 5];

			if (ext < file->d_name )
				continue;

			if (strncmp(file->d_name, "mic", 3))
				continue;

			if (strncmp(ext, ".conf", 5))
				continue;

			for (id = file->d_name + 3; id < ext; id++)
				if (!isdigit(*id)) nondigit = TRUE;

			if (nondigit == FALSE)
				ret = FALSE;
		}

		closedir(dp);
	}

	return ret;
}

void
delete_conffile(const std::string& name, int backup)
{
	char filename[PATH_MAX];
	char origname[PATH_MAX];

	if (!mpssut_filename(filename, PATH_MAX, config_file_path_pattern, DEFAULT_CONFDIR, name.c_str()))
		return;

	if (mpssut_filename(origname, PATH_MAX, config_orig_file_path_pattern, DEFAULT_CONFDIR, name.c_str()))
		unlink(origname);

	if (backup) {
		if (mpssut_rename(filename, origname)) {
				display(PERROR, "Failed to rename temporary file %s\n", origname);
				return;
		}
		display(PFS, "%s: Rename %s to %s\n", name.c_str(), filename, origname);
	} else {
		unlink(filename);
		display(PFS, "%s: Remove %s\n", name.c_str(), filename);
	}
}

const char *host_net_desc =
	"# Network configuration of the micN interface on the host side\n"
	"# Available values:\n"
	"# HostNetworkConfig inet static address=<IPv4_address> netmask=<IPv4_netmask>\n"
	"# HostNetworkConfig inet6 static address=<IPv6_address> netmask=<IPv6_netmask>\n"
	"# HostNetworkConfig bridge <bridge_name>\n"
	"# HostNetworkConfig none\n"
	"# \n"
	"# Default value is \"HostNetworkConfig inet static\"\n"
	"# Setting value to \"HostNetworkConfig bridge <bridge_name>\" will connect\n"
	"#     the micN interface to the bridge <bridge_name>.\n"
	"# Setting value to \"HostNetworkConfig none\" will prevent any operations.\n"
	"#     The micN interface will not have an IP address assigned by the MPSS.\n"
	"#     It allows user to configure micN interface manually.\n";

const char *card_net_desc =
	"# Network configuration of the eth0 interface on the coprocessor side\n"
	"# Available value:\n"
	"# CardNetworkConfig inet static address=<IPv4_address> netmask=<IPv4_netmask> gateway=<IPv4_gateway>\n"
	"# CardNetworkConfig inet6 static address=<IPv6_address> netmask=<IPv6_netmask> gateway=<IPv6_gateway>\n"
	"# CardNetworkConfig inet dhcp hostname=<hostname>\n"
	"# CardNetworkConfig inet6 dhcp\n"
	"# CardNetworkConfig none\n"
	"# \n"
	"# Default values are \"CardNetworkConfig inet static\" and \"CardNetworkConfig inet6 static.\"\n"
	"# Setting value to \"CardNetworkConfig inet dhcp\" will assign an IP address from a DHCP server.\n"
	"# Setting value to \"CardNetworkConfig none\" will prevent modification\n"
	"#     of the /etc/network/interfaces file on the coprocessor.\n"
	"#     It allows user to configure coprocessor network (the eth0 interface) manually.\n";

void
set_net_config(struct mic_info *mic, std::ostream &stream)
{
	stream << host_net_desc;
	stream << CONFIG_HOST_NET << " inet static address=" <<
		get_default_host_ip_address(mic->id) <<
		" netmask=" << get_netmask() << "\n\n";
	stream << card_net_desc;
	stream << CONFIG_CARD_NET << " inet static address=" <<
		get_default_card_ip_address(mic->id) <<
		" netmask=" << get_netmask() << " gateway=" <<
		get_default_host_ip_address(mic->id) << "\n\n";
}

const char *block_device_desc =
	"# Optional block device images for the coprocessor OS\n"
	"#\n"
	"# Parameters:\n"
	"#     <name>: name of the block device visible on the coprocessor OS\n"
	"#             in the /dev/mapper directory\n"
	"#     <mode>: access mode - ro (read-only) / rw (read-write)\n"
	"#     <path>: path to the block device image file on the host\n"
	"#     The <mode> parameter is optional; if it does not exist, ro (read-only) is used\n"
	"#\n"
	"# Note: Using the same block device image in read-write mode for more\n"
	"#       than one block device (including block devices of other coprocessors) is\n"
	"#       not supported and may lead to unexpected errors.\n"
	"# BlockDevice name=blk mode=rw path=/../../\n\n";

const char *conf_file_info =
	"# This is the configuration file for the MPSS stack.\n"
	"# For more information about options, see the MPSS User's Guide.\n\n";

void
output_mpss_config(struct mic_info *mic, std::ostream &stream)
{
	stream << conf_file_info;

	stream <<  micconfhead << "\n";

	stream << "# Path to file containing the EFI image\n";
	stream << CONFIG_EFI_IMAGE << " " << DEFAULT_SRCDIR << "/" << DEFAULT_EFIIMAGE_KNL << "\n\n";

	stream << "# Path to file containing the coprocessor OS kernel image\n";
	stream << CONFIG_KERNEL_IMAGE << " " << DEFAULT_SRCDIR << "/" << DEFAULT_BZIMAGE_KNL << "\n\n";

	stream << "# Path to file containing the coprocessor OS kernel symbols\n";
	stream << CONFIG_KERNEL_SYMBOLS << " " << DEFAULT_SRCDIR << "/" << DEFAULT_SYSMAP_KNL << "\n\n";

	stream << "# Base filesystem for the embedded Linux file system\n";
	stream << CONFIG_INIT_RAMFS << " " << DEFAULT_SRCDIR << "/" << DEFAULT_INITRD_KNL << "\n\n";

	stream << "# Root filesystem for the coprocessor OS\n";
	stream << CONFIG_ROOTFS_IMAGE << " " << DEFAULT_VARDIR << "/" << DEFAULT_PFS_KNL << "-" << mic->name << ".ext4" << "\n\n";

	stream << "# Smart repository with RPM packages for the coprocessor\n";
	stream << CONFIG_REPOFS_IMAGE << " " << DEFAULT_REPODIR << "/" << DEFAULT_PFSREPO_KNL << "\n\n";

	stream << block_device_desc;

	set_net_config(mic, stream);
	stream << "# Boot the coprocessor when the mpss service is started\n";
	stream << CONFIG_AUTO_BOOT << " Enabled\n\n";
}

void
output_mpss_default_config(std::ostream &stream)
{
	stream << CONFIG_SHUTDOWN_TIMEOUT << " " << SHUTDOWN_TIMEOUT_DEFAULT << "\n\n";
	stream << CONFIG_BOOT_TIMEOUT << " " << BOOT_TIMEOUT_DEFAULT << "\n\n";
	stream << "# Extra command line passed to the coprocessor kernel\n";
	stream << CONFIG_KERNEL_CMDLINE << " " << knlextracmd << "\n";
}
