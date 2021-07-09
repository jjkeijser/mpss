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

#include <mpssconfig.h>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <fstream>
#include <pwd.h>
#include <string>
#include <sys/stat.h>

void
report_rootdev(struct mic_info *mic)
{
	switch(mic->config.rootdev.type) {

	case ROOT_TYPE_PFS:
		println_indent(FIRST_LEVEL_INDENT, "Initial RAM Disk Image:", mic->config.filesrc.base.image);

		bool root_in_ram = mic->config.boot.extraCmdline.find("mic_root_in_ram=1") != std::string::npos;
		for (int i = 0; i < mic->config.blocknum; i++) {
			const mblock& blk_device = mic->config.blockdevs[i];
			if (blk_device.dest == BLOCK_NAME_ROOT) {
				println_indent(FIRST_LEVEL_INDENT, "Root File System Image:", blk_device.source + " [" +
					(root_in_ram ? READ_ONLY_MODE : READ_WRITE_MODE) + "]");
			} else if (blk_device.dest == BLOCK_NAME_REPO) {
				println_indent(FIRST_LEVEL_INDENT, "Package Repository:", blk_device.source + " [" +
					READ_ONLY_MODE + "]");
			} else {
				println_indent(FIRST_LEVEL_INDENT, "Block Device:", blk_device.dest + " -> " + blk_device.source +
					" [" + (blk_device.mode == mpss_block_device_mode::READ_WRITE ? READ_WRITE_MODE : READ_ONLY_MODE) + "]");
			}
		}
		break;
	}
}

void unzip_pfs_image(const std::string& micname, const std::string& mictarget, const std::string& micsource)
{
	try {
		std::ifstream ifs (micsource.c_str(), std::ios_base::in | std::ios_base::binary);

		boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
		in.push(boost::iostreams::gzip_decompressor());
		in.push(ifs);

		std::ofstream ofs(mictarget.c_str(), std::ios_base::out |
						 std::ios_base::binary |
						 std::ios_base::trunc);

		boost::iostreams::copy(in, ofs);
	} catch (std::ios_base::failure& e) {
		display(PERROR, "%s: Failed to extract PFS image: %s\n", micname.c_str(), e.what());
		exit(EXIT_FAILURE);
	}
}

void
prepare_pfs_root_device(struct mic_info *mic, int remove_mode)
{
	display(PINFO, "%s: Default BlockDevice - Persistent FS\n", mic->name.c_str());

	if (remove_mode & REMOVE_IMAGE) {
		std::string targetname = DEFAULT_SRCDIR "/" DEFAULT_PFS_KNL ".ext4.gz";
		std::string mictarget;

		if (mic->config.rootdev.target.empty()) {
			mictarget = DEFAULT_VARDIR "/" DEFAULT_PFS_KNL "-" + mic->name + ".ext4";
		} else {
			mictarget = mic->config.rootdev.target;
		}

		unzip_pfs_image(mic->name, mictarget, targetname);

		//set proper access rights
		chmod(mictarget.c_str(), 0600);

		struct passwd *hpw = getpwuid(0);
		std::string keydir = hpw->pw_dir + std::string("/.ssh/known_hosts");
		if (path_exists(keydir)) {
			std::string ip;
			if (mic->config.net.card_setting.ip.empty()) {
				ip = get_default_card_ip_address(mic->id);
			} else {
				ip = mic->config.net.card_setting.ip;
			}
			exec_command("ssh-keygen -R " + ip + " >& /dev/null");
		}
	}
}

