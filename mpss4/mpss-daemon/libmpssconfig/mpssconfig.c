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

#include "mpssconfig.h"
#include "libmpsscommon.h"

#include <algorithm>
#include <arpa/inet.h>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <net/if.h>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#define MIC_MODULES_LOAD "modprobe -a mic_x200_dma scif_bus vop_bus cosm_bus scif vop mic_cosm mic_x200"
#define MIC_MODULES_UNLOAD "modprobe -r mic_cosm scif vop mic_x200 mic_x200_dma vop_bus scif_bus cosm_bus"

const char* const MODULESYSFSDIR = "/sys/module";

const std::string NET_TYPE_INET = "inet";
const std::string NET_TYPE_INET6 = "inet6";
const std::string NET_TYPE_STATIC = "static";
const std::string NET_TYPE_DHCP = "dhcp";
const std::string NET_TYPE_NONE = "none";
const std::string NET_TYPE_BRIDGE = "bridge";

struct tokens {
	const char	*name;
	unsigned int	minargs;
	unsigned int	maxargs;
	int		(*func)(const std::vector<std::string>&, struct
			mic_info *mic, mpss_elist& perr, const char *cfile, int lineno);
	bool		duplication_allowed;
	bool		verify_path;
};

void
mpss_mac::reload_serial()
{
	if (m_type != SERIAL_HOST && m_type != SERIAL_MIC)
		return;

	if (!m_address.empty())
		return;

	if (m_device.empty())
		return;

	std::string serial = mpss_readsysfs(m_device.c_str(), "info/system_serial_number");
	if (serial.length() != 12 || serial[2] != 'K' || serial[3] != 'L')
		return;

	// serial number
	int sn = strtoul(serial.substr(7).c_str(), NULL, 10);

	// last digit of Year (2015 - 2024)
	int year = (serial[4] >= '5') ? (serial[4] - '5') : (serial[4] - '0' + 5);

	// work week
	int ww = ((serial[5] - '0') * 10) + (serial[6] - '0');

	// generate the last 3 bytes of MAC
	// please note, the MAC address is not unique
	// it is possible that two different serial numbers will generate the same MAC
	// especially if sn >= 8192, however (since sn resets to 00001 at the beginning
	// of every week and 8192 units is more than doubles planned capacity per week)
	// it is very unlikely it will happen in the real
	int last_bytes = ((year * 54 + ww) << 14) + (sn << 1);

	// least bit indicates host MAC
	if (m_type == SERIAL_HOST)
		last_bytes += 1;

	std::ostringstream output_stream;
	output_stream << std::hex << std::setfill('0');
	output_stream << "e4:fa:fd:";
	output_stream << std::setw(2) << ((last_bytes >> 16) & 0xff);
	output_stream << ':';
	output_stream << std::setw(2) << ((last_bytes >> 8) & 0xff);
	output_stream << ':';
	output_stream << std::setw(2) << (last_bytes & 0xff);

	m_address = output_stream.str();
}

bool
mpss_mac::set_serial(const std::string& mic_name, bool host_side)
{
	m_device = mic_name;

	m_type = (host_side) ? SERIAL_HOST : SERIAL_MIC;
	m_address.clear();
	reload_serial();

	return true;
}

bool
mpss_mac::set_random()
{
	m_type = RANDOM;
	m_address.clear();

	return true;
}

bool
mpss_mac::set_fixed(const std::string& address)
{
	m_type = FIXED;
	m_address = address;

	return true;
}

std::string
mpss_mac::address()
{
	reload_serial();

	return m_address;
}

bool
mpss_mac::to_mac_48(uint8_t mac[6])
{
	if (address().empty())
		return false;

	int mac_tmp[6];

	std::istringstream input_stream(address());

	input_stream >> std::hex;
	input_stream >> mac_tmp[0];
	input_stream.ignore();
	input_stream >> mac_tmp[1];
	input_stream.ignore();
	input_stream >> mac_tmp[2];
	input_stream.ignore();
	input_stream >> mac_tmp[3];
	input_stream.ignore();
	input_stream >> mac_tmp[4];
	input_stream.ignore();
	input_stream >> mac_tmp[5];

	if (input_stream.fail())
		return false;

	for (int i = 0; i < 6; ++i) {
		if (mac_tmp[i] < 0 || mac_tmp[i] > 255)
			return false;

		mac[i] = mac_tmp[i];
	}

	return true;
}

std::string
mpss_mac::type_str() const
{
	if (m_type == SERIAL_HOST || m_type == SERIAL_MIC)
		return "Serial";

	if (m_type == RANDOM)
		return "Random";

	if (m_type == FIXED)
		return "Fixed";

	return "Unknown";
}

std::string
mpss_mac::str()
{
	if (!address().empty()) {
		return address() + " [" + type_str() + "]";
	}

	return type_str();
}

int parse_config_file(struct mic_info *mic,
		const std::string& filename, mpss_elist& perrs, std::vector<std::string>& parsed_files);

int
mpss_parse_config(struct mic_info *mic, mpss_elist& perrs)
{
	perrs.clear();
	mpss_clear_config(&mic->config);

	std::vector<std::string> parsed_files;
	parsed_files.push_back(mic->config.name);

	int parse_ret_code = parse_config_file(mic, mic->config.name, perrs, parsed_files);

	// if MacAddrs is not in the config, then host_mac and mic_mac are not set
	// we don't have any mechanism to init default values, thus we do it here
	if (mic->config.net.host_mac.type() == mpss_mac::UNKNOWN)
		mic->config.net.host_mac.set_serial(mic->name, true);
	if (mic->config.net.mic_mac.type() == mpss_mac::UNKNOWN)
		mic->config.net.mic_mac.set_serial(mic->name, false);

	return parse_ret_code;
}

int
mc_kernelimage(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	mic->config.boot.osimage = args[0];
	return 0;
}

int
mc_kernelsymbols(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	mic->config.boot.systemmap = args[0];
	return 0;
}

int
mc_efiimage(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	mic->config.boot.efiimage = args[0];
	return 0;
}

int
mc_doinclude(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno,
		std::vector<std::string>& parsed_files)
{
	std::string file = args[0];
	std::string filename;

	if (file.at(0) == '/') {
		filename = file;
	} else {
		filename = DEFAULT_CONFDIR "/" + file;
	}
	if (std::find(parsed_files.begin(), parsed_files.end(), filename) != parsed_files.end()) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: File %s already parsed", 
			mic->name.c_str(), cfile, lineno, filename.c_str());
		return EEXIST;
	} else {
		parsed_files.push_back(filename);
	}


	return parse_config_file(mic, filename, perrs, parsed_files);
}

std::map<std::string, std::string>
parse_network_parameters(const std::vector<std::string>& args, const std::string& mic_name, mpss_elist& perrs)
{
	std::map<std::string, std::string> keyvalues;

	for (auto& arg : args) {
		std::string param = arg;
		std::size_t pos;

		if ((pos = param.find("=")) == std::string::npos) {
			perrs.add(PWARN, "%s: [Parse] Network invalid subparameter %s %d", mic_name.c_str(), arg.c_str(), (int)pos);
			continue;
		}

		std::string key =  param.substr(0, pos);
		std::string value = param.substr(pos + 1);

		keyvalues[key] = value;
	}
	return std::move(keyvalues);
}

bool is_ip_address_valid(int type, const std::string& ip_address)
{
	if (type == AF_INET) {
		struct sockaddr_in ip4_addr = {0};
		ip4_addr.sin_family = AF_INET;
		return inet_pton(AF_INET, ip_address.c_str(), &ip4_addr.sin_addr);
	} else if (type == AF_INET6) {
		struct in6_ifreq ifr_inet6 = {0};
		return inet_pton(AF_INET6, ip_address.c_str(), &ifr_inet6.ifr6_addr);
	}
	return false;
}

int verify_network_parameters(const std::string& param_name, const std::string& ip, const std::string& netmask, const std::string& gateway, 
		const std::string& mic_name, const char *cfile, int lineno, mpss_elist& perrs, int type)
{
	if (ip.empty() || netmask.empty() || (param_name == CONFIG_CARD_NET && gateway.empty())) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: %s does not exist:", mic_name.c_str(), cfile, lineno, param_name.c_str());
		std::string msg;
		if (ip.empty()) {
			msg += "IP address";
		}
		if (netmask.empty()) {
			if (!msg.empty()) {
				msg += ", ";
			}
			msg += "Netmask";
		}
		if (param_name == CONFIG_CARD_NET && gateway.empty()) {
			if (!msg.empty()) {
				msg += ", ";
			}
			msg += "Gateway";
		}
		perrs.add(PERROR, "%s: Empty parameters: %s", param_name.c_str(), msg.c_str());
		return EINVAL;
	}

	bool ip_address_failed = false;
	if (!is_ip_address_valid(type, ip)) {
		perrs.add(PERROR, "%s: IP address not valid", param_name.c_str());
		ip_address_failed = true;
	}
	if (!is_ip_address_valid(type, netmask)) {
		perrs.add(PERROR, "%s: Netmask not valid", param_name.c_str());
		ip_address_failed = true;
	}
	if (!gateway.empty() && !is_ip_address_valid(type, gateway)) {
		perrs.add(PERROR, "%s: Gateway not valid", param_name.c_str());
		ip_address_failed = true;
	}

	if (ip_address_failed) {
		return EINVAL;
	}
	return 0;
}

int validate_number_of_parameters(const std::string& param_name, const std::vector<std::string>& args, mpss_elist& perrs, 
		const char *cfile, int lineno, std::string& mic_name)
{
	bool result = false;
	std::string type = args[0];

	if (type == NET_TYPE_INET || type == NET_TYPE_INET6) {
		if (args.size() < 2) {
			perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: %s does not exist",
				mic_name.c_str(), cfile, lineno, param_name.c_str());
			return EINVAL;
		}
		if (param_name == CONFIG_CARD_NET) {
			std::string sec_type = args[1];
			if ((sec_type == NET_TYPE_STATIC && args.size() == 5) ||
				(sec_type == NET_TYPE_DHCP && (args.size() == 2 || args.size() == 3))) {
				result = true;
			}
		} else if (param_name == CONFIG_HOST_NET && args.size() == 4) {
			result = true;
		}
	} else if (type == NET_TYPE_BRIDGE && args.size() == 2) {
		result = true;
	} else if (type == NET_TYPE_NONE && args.size() == 1) {
		result = true;
	}

	if (!result) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: %s invalid number of parameters",
			mic_name.c_str(), cfile, lineno, param_name.c_str());
		return EINVAL;
	}
	return 0;
}

int
mc_host_ip_config(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	if (validate_number_of_parameters(CONFIG_HOST_NET, args, perrs, cfile, lineno, mic->name)) {
		return EINVAL;
	}

	std::string type = args[0];
	if (type != "bridge" && mic->config.net.host_type == mpss_host_network_type::BRIDGE) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: only one HostNetworkConfig parameter is allowed when bridge is specified",
			mic->name.c_str(), cfile, lineno);
		return EINVAL;
	}

	if (type == NET_TYPE_INET && args[1] == NET_TYPE_STATIC) {
		mic->config.net.host_type = mpss_host_network_type::STATIC;
		auto first = args.cbegin() + 2;
		std::vector<std::string> new_args(first, args.end());

		auto params = parse_network_parameters(new_args, mic->name, perrs);
		mic->config.net.host_setting.ip = params["address"];
		mic->config.net.host_setting.netmask = params["netmask"];

		return verify_network_parameters(CONFIG_HOST_NET, mic->config.net.host_setting.ip,
			mic->config.net.host_setting.netmask, "", mic->name, cfile, lineno, perrs, AF_INET);
	} else if (type == NET_TYPE_INET6 && args[1] == NET_TYPE_STATIC) {
		mic->config.net.host_type_inet6 = mpss_host_network_type::STATIC;
		auto first = args.cbegin() + 2;
		std::vector<std::string> new_args(first, args.end());

		auto params = parse_network_parameters(new_args, mic->name, perrs);
		mic->config.net.host_setting_inet6.ip = params["address"];
		mic->config.net.host_setting_inet6.netmask = params["netmask"];
		return verify_network_parameters(CONFIG_HOST_NET, mic->config.net.host_setting_inet6.ip,
			mic->config.net.host_setting_inet6.netmask, "", mic->name, cfile, lineno, perrs, AF_INET6);
	} else if (type == NET_TYPE_BRIDGE) {
		mic->config.net.host_type = mpss_host_network_type::BRIDGE;
		mic->config.net.bridge_name = args[1];
		if (!if_nametoindex(args[1].c_str())) {
			perrs.add(PERROR, "%s: Bridge interface does not exist", mic->name.c_str());
			return EINVAL;
		}
	} else if (type == NET_TYPE_NONE) {
		mic->config.net.host_type = mpss_host_network_type::NONE;
		mic->config.net.host_type_inet6 = mpss_host_network_type::NONE;
	} else {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: HostNetworkConfig unknown type %s", 
			mic->name.c_str(), cfile, lineno, type.c_str());
		mic->config.net.host_type = mpss_host_network_type::UNKNOWN;
		return EINVAL;
	}
	return 0;
}

int
mc_card_ip_config(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	if (validate_number_of_parameters(CONFIG_CARD_NET, args, perrs, cfile, lineno, mic->name)) {
		return EINVAL;
	}

	std::string type = args[0];
	if (type == NET_TYPE_INET) {
		std::string sec_type = args[1];
		if (sec_type == NET_TYPE_STATIC) {
			mic->config.net.card_type = mpss_card_network_type::STATIC;

			auto first = args.cbegin() + 2;
			std::vector<std::string> new_args(first, args.end());
			auto params = parse_network_parameters(new_args, mic->name, perrs);
			mic->config.net.card_setting.ip = params["address"];
			mic->config.net.card_setting.netmask = params["netmask"];
			mic->config.net.card_setting.gateway = params["gateway"];
			return verify_network_parameters(CONFIG_CARD_NET, mic->config.net.card_setting.ip,
				mic->config.net.card_setting.netmask, mic->config.net.card_setting.gateway,
				mic->name, cfile, lineno, perrs, AF_INET);
		} else if (sec_type == NET_TYPE_DHCP) {
			auto first = args.cbegin() + 2;
			mic->config.net.card_type = mpss_card_network_type::DHCP;
			std::vector<std::string> new_args(first, args.end());
			auto params = parse_network_parameters(new_args, mic->name, perrs);
			mic->config.net.card_setting.dhcp_hostname = params["hostname"];
		} else if (sec_type == NET_TYPE_NONE) {
			mic->config.net.card_type = mpss_card_network_type::NONE;
		} else {
			mic->config.net.card_type = mpss_card_network_type::UNKNOWN;
		}
	} else if (type == NET_TYPE_INET6) {
		std::string sec_type = args[1];
		if (sec_type == NET_TYPE_STATIC) {
			mic->config.net.card_type_inet6 = mpss_card_network_type::STATIC;

			auto first = args.cbegin() + 2;
			std::vector<std::string> new_args(first, args.end());
			auto params = parse_network_parameters(new_args, mic->name, perrs);
			mic->config.net.card_setting_inet6.ip = params["address"];
			mic->config.net.card_setting_inet6.netmask = params["netmask"];
			mic->config.net.card_setting_inet6.gateway = params["gateway"];
			return verify_network_parameters(CONFIG_CARD_NET, mic->config.net.card_setting_inet6.ip,
				mic->config.net.card_setting_inet6.netmask, mic->config.net.card_setting_inet6.gateway,
				mic->name, cfile, lineno, perrs, AF_INET6);
		} else if (sec_type == NET_TYPE_DHCP) {
			mic->config.net.card_type_inet6 = mpss_card_network_type::DHCP;
		} else if (sec_type == NET_TYPE_NONE) {
			mic->config.net.card_type_inet6 = mpss_card_network_type::NONE;
		} else {
			mic->config.net.card_type_inet6 = mpss_card_network_type::UNKNOWN;
		}
	} else if (type == NET_TYPE_NONE) {
		mic->config.net.card_type = mpss_card_network_type::NONE;
		mic->config.net.card_type_inet6 = mpss_card_network_type::NONE;
	} else {
		mic->config.net.card_type = mpss_card_network_type::UNKNOWN;
		mic->config.net.card_type_inet6 = mpss_card_network_type::UNKNOWN;
	}

	if (mic->config.net.card_type == mpss_card_network_type::UNKNOWN ||
			mic->config.net.card_type_inet6 == mpss_card_network_type::UNKNOWN) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: CardNetworkConfig unknown type %s",
			mic->name.c_str(), cfile, lineno, type.c_str());
		return EINVAL;
	}
	return 0;
}

int is_mac_address_valid(const std::string& mac_address)
{
	boost::match_results<std::string::const_iterator> results;
	boost::regex expression("^([[:xdigit:]]{2}):([[:xdigit:]]{2}):([[:xdigit:]]{2}):([[:xdigit:]]{2}):([[:xdigit:]]{2}):([[:xdigit:]]){2}$");
	if (boost::regex_match(mac_address, results, expression)) {
		return true;
	}
	return false;
}

int
mc_hostmac(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	if (args[0] == "Serial") {
		mic->config.net.host_mac.set_serial(mic->name, true);
	} else if (args[0] == "Random") {
		mic->config.net.host_mac.set_random();
	} else {
		mic->config.net.host_mac.set_fixed(args[0]);
		if (!is_mac_address_valid(args[0])) {
			perrs.add(PERROR, "HostMacAddress not valid");
			return EINVAL;
		}
	}
	return 0;
}

int
mc_cardmac(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	if (args[0] == "Serial") {
		mic->config.net.mic_mac.set_serial(mic->name, false);
	} else if (args[0] == "Random") {
		mic->config.net.mic_mac.set_random();
	} else {
		mic->config.net.mic_mac.set_fixed(args[0]);
		if (!is_mac_address_valid(args[0])) {
			perrs.add(PERROR, "CardMacAddress not valid");
			return EINVAL;
		}
	}
	return 0;
}

int
mc_initramfs(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	mic->config.filesrc.base.image = args[0];
	return 0;
}

int
mc_setblock(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	std::string source_path;
	std::string name;
	mpss_block_device_mode mode = mpss_block_device_mode::READ_ONLY;

	int idx = mic->config.blocknum;

	if (idx >= BLOCK_MAX_COUNT) {
		perrs.add(PWARN, "%s: [Parse] %s line %d: too much block devices defined "
				"(max. %d block devices available);"
				" ignoring BlockDevice \"%s\" entry",
				mic->name.c_str(), cfile, lineno, BLOCK_MAX_COUNT, name.c_str());
		return 0;
	}

	std::vector<std::string>::const_iterator iter = args.begin();

	for (auto& arg : args) {
		std::string::size_type eq_pos = arg.find("=");

		if (eq_pos == std::string::npos) {
			perrs.add(PWARN,
					"%s: [Parse] %s line %d: BlockDevice: argument %s is not key-value, \"=\" splitted type",
					name.c_str(), cfile, lineno, arg.c_str());
			iter++;
			continue;
		}

		std::string key   = arg.substr(0, eq_pos);
		std::string value = arg.substr(eq_pos + 1);

		if (key == "name") {
			name = value;
		} else if (key == "path") {
			source_path = value;
		} else if (key == "mode") {
			if (value == "rw") {
				mode = mpss_block_device_mode::READ_WRITE;
			} else if (value == "ro") {
				mode = mpss_block_device_mode::READ_ONLY;
			} else {
				perrs.add(PERROR, "%s [Parse] %s line %d: BlockDevice: mode %s is not valid argument",
					mic->name.c_str(), cfile, lineno, value.c_str());
				return EINVAL;
			}
		} else {
			perrs.add(PWARN,
					"%s [Parse] %s line %d: BlockDevice: key %s is not valid argument for BlockDevice",
					mic->name.c_str(), cfile, lineno, key.c_str());
		}

	}

	if (name == BLOCK_NAME_ROOT || name == BLOCK_NAME_REPO) {
		perrs.add(PERROR,
				"%s: [Parse FATAL] %s line %d: BlockDevice: name %s is reserved for internal daemon mechanisms",
				mic->name.c_str(), cfile, lineno, name.c_str());
		return EINVAL;
	}

	if (source_path.empty() || name.empty() || !path_exists(source_path)) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: BlockDevice: source or block name not defined",
			mic->name.c_str(), cfile, lineno);
		return EINVAL;
	}

	mic->config.blockdevs[idx].source = source_path;
	mic->config.blockdevs[idx].dest = name;
	mic->config.blockdevs[idx].mode = mode;

	for (int i = 0; i < idx; ++i)
		if (mic->config.blockdevs[i].source == mic->config.blockdevs[idx].source ||
				mic->config.blockdevs[i].dest == mic->config.blockdevs[idx].dest) {
			perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: BlockDevice: source file or destination name repeated\n"
					"\t" "previous: %s -> %s\n" "\t" "    next: %s -> %s",
					mic->name.c_str(), cfile, lineno, mic->config.blockdevs[i].source.c_str(), mic->config.blockdevs[i].dest.c_str(),
					mic->config.blockdevs[idx].source.c_str(), mic->config.blockdevs[idx].dest.c_str());
			return EINVAL;
		}

	mic->config.blocknum++;

	return 0;
}

int
mc_setrootimage(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	std::string source_path = args[0];
	std::string name = BLOCK_NAME_ROOT;

	int idx = mic->config.blocknum;

	if (idx >= BLOCK_MAX_COUNT) {
		perrs.add(PWARN, "%s: [Parse] %s line %d: too much block devices defined "
				"(max. %d block devices available);"
				" ignoring RepoFsImage \"%s\" entry",
				mic->name.c_str(), cfile, lineno, BLOCK_MAX_COUNT, name.c_str());
		return 0;
	}

	if (source_path.empty() || !path_exists(args[0])) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: RootFsImage path does not exist",
			mic->name.c_str(), cfile, lineno);
		return EINVAL;
	}

	mic->config.blockdevs[idx].source = source_path;
	mic->config.blockdevs[idx].dest = name;
	mic->config.blockdevs[idx].mode = mpss_block_device_mode::READ_WRITE;

	for (int i = 0; i < idx; ++i)
		if (mic->config.blockdevs[i].source == mic->config.blockdevs[idx].source ||
				mic->config.blockdevs[i].dest == mic->config.blockdevs[idx].dest) {
			perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: source file or destination name repeated\n"
					"\t" "previous: %s -> %s\n" "\t" "    next: %s -> %s",
					mic->name.c_str(), cfile, lineno, mic->config.blockdevs[i].source.c_str(), mic->config.blockdevs[i].dest.c_str(),
					mic->config.blockdevs[idx].source.c_str(), mic->config.blockdevs[idx].dest.c_str());
			return EINVAL;
		}

	mic->config.blockdevs[idx].options |= BLOCK_OPTION_ROOT;
	mic->config.rootdev.type = ROOT_TYPE_PFS;
	mic->config.rootdev.target = std::string(source_path);
	mic->config.blocknum++;

	return 0;
}

int
mc_setrepoimage(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	std::string source_path = args[0];
	std::string name = BLOCK_NAME_REPO;

	int idx = mic->config.blocknum;

	if (idx >= BLOCK_MAX_COUNT) {
		perrs.add(PWARN, "%s: [Parse] %s line %d: too much block devices defined "
				"(max. %d block devices available);"
				" ignoring RepoFsImage \"%s\" entry",
				mic->name.c_str(), cfile, lineno, BLOCK_MAX_COUNT, name.c_str());
		return 0;
	}

	if (source_path.empty()) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: source path not defined",
			mic->name.c_str(), cfile, lineno);
		return EINVAL;
	}

	mic->config.blockdevs[idx].source = source_path;
	mic->config.blockdevs[idx].dest = name;
	mic->config.blockdevs[idx].mode = mpss_block_device_mode::READ_ONLY;

	for (int i = 0; i < idx; ++i)
		if (mic->config.blockdevs[i].source == mic->config.blockdevs[idx].source ||
				mic->config.blockdevs[i].dest == mic->config.blockdevs[idx].dest) {
			perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: source file or destination name repeated\n"
					"\t" "previous: %s -> %s\n" "\t" "    next: %s -> %s",
					mic->name.c_str(), cfile, lineno, mic->config.blockdevs[i].source.c_str(), mic->config.blockdevs[i].dest.c_str(),
					mic->config.blockdevs[idx].source.c_str(), mic->config.blockdevs[idx].dest.c_str());
			return EINVAL;
		}

	mic->config.blockdevs[idx].options |= BLOCK_OPTION_REPO;
	mic->config.blocknum++;

	return 0;
}

int
mc_autoboot(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	if (args[0] == MIC_DISABLED) {
		mic->config.boot.onstart = FALSE;
	} else if (args[0] ==  MIC_ENABLED) {
		mic->config.boot.onstart = TRUE;
	} else {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: AutoBoot unknown value %s",
			mic->name.c_str(), cfile, lineno, args[0].c_str());
		mic->config.boot.onstart = NOTSET;
		return EINVAL;
	}
	return 0;
}

int
mc_extra(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	mic->config.boot.extraCmdline += args[0];
	mic->config.boot.extraCmdline += " ";
	return 0;
}

int validate_and_parse_timeout(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	char *end;

	long value = strtol(args[0].c_str(), &end, 0);
	if (*end != '\0') {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: Invalid timeout value %s",
			mic->name.c_str(), cfile, lineno, args[0].c_str());
		return -1;
	}
	value = std::min<long>(value, INT_MAX);
	value = std::max<long>(value, INT_MIN);
	if (value < 0) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: Timeout value < 0",
			mic->name.c_str(), cfile, lineno);
		return -1;;
	}

	return (int) value;
}

int
mc_shutdown_to(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	int value = validate_and_parse_timeout(args, mic, perrs, cfile, lineno);
	if (value < 0) {
		mic->config.misc.shutdowntimeout = SHUTDOWN_TIMEOUT_DEFAULT;
		return EINVAL;
	}

	mic->config.misc.shutdowntimeout = value;
	return 0;
}

int
mc_boot_to(const std::vector<std::string>& args, struct mic_info *mic,
		mpss_elist& perrs, const char *cfile, int lineno)
{
	int value = validate_and_parse_timeout(args, mic, perrs, cfile, lineno);
	if (value < 0) {
		mic->config.misc.boottimeout = BOOT_TIMEOUT_DEFAULT;
		return EINVAL;
	}

	mic->config.misc.boottimeout = value;
	return 0;
}

void
mpss_clear_config(struct mconfig *config)
{
	config->boot.onstart = NOTSET;
	config->rootdev.type = NOTSET;

	config->boot.osimage = "";
	config->boot.efiimage = "";

	config->rootdev.target.clear();

	for (int i = 0; i < BLOCK_MAX_COUNT; ++i) {
		config->blockdevs[i].source.clear();
		config->blockdevs[i].dest.clear();
		config->blockdevs[i].options = 0;
	}
	config->blocknum = 0;
	config->misc.shutdowntimeout = SHUTDOWN_TIMEOUT_DEFAULT;
	config->misc.boottimeout = BOOT_TIMEOUT_DEFAULT;

	config->net.clear();

	config->boot.extraCmdline.clear();
	config->filesrc.base.image.clear();
}

struct tokens tokens[] = {
	{CONFIG_KERNEL_IMAGE, 1, 1, mc_kernelimage, false, true},
	{CONFIG_KERNEL_SYMBOLS, 1, 1, mc_kernelsymbols, false, true},
	{CONFIG_EFI_IMAGE, 1, 1, mc_efiimage, false, true},
	{CONFIG_AUTO_BOOT, 1, 1, mc_autoboot, false, false},
	{CONFIG_HOST_NET, 1, 4, mc_host_ip_config, true, false},
	{CONFIG_CARD_NET, 1, 5, mc_card_ip_config, true, false},
	{CONFIG_HOST_MAC, 1, 1, mc_hostmac, false, false},
	{CONFIG_CARD_MAC, 1, 1, mc_cardmac, false, false},
	{CONFIG_KERNEL_CMDLINE, 1, 1, mc_extra, true, false},
	{CONFIG_INIT_RAMFS, 1, 1, mc_initramfs, false, true},
	{CONFIG_BLOCK_DEVICE, 2, 3, mc_setblock, true, false},
	{CONFIG_ROOTFS_IMAGE, 1, 1, mc_setrootimage, false, false},
	{CONFIG_REPOFS_IMAGE, 1, 1, mc_setrepoimage, false, false},
	{CONFIG_SHUTDOWN_TIMEOUT, 1, 1, mc_shutdown_to, false, false},
	{CONFIG_BOOT_TIMEOUT, 1, 1, mc_boot_to, false, false},
	{NULL, 0, 0, NULL, NULL}
};

#define MAX_ARGS 10

int
parse_config_file(struct mic_info *mic,
		const std::string& filename, mpss_elist& perrs, std::vector<std::string>& parsed_files)
{
	std::ifstream conffile(filename);
	if (!conffile.is_open()) {
		perrs.add(PERROR, "%s: [Parse FATAL] %s open fail", mic->name.c_str(), filename.c_str());
		return EBADF;
	}

	std::string line;
	int lineno = 1;
	int ret = 0;
	struct tokens *tok;
	std::map<std::string, int> params_read;
	while (std::getline(conffile, line)) {
		boost::trim(line);
		if (line.length() == 0 || line.at(0) == '#') {
			lineno++;
			continue;
		}
		std::vector<std::string> results;

		std::size_t first_index = line.find("\"");
		if (first_index != std::string::npos) {
			std::size_t second_index = line.find("\"", first_index + 1);
			if (second_index == std::string::npos) {
				perrs.add(PERROR, "%s [Parse FATAL] %s line %d: "
					"Quoted argument missing ending \"", mic->name.c_str(), filename.c_str(), lineno);
				break;
			}

			std::string quoted_str = line.substr(first_index + 1, second_index - first_index - 1);
			line = line.substr(0, first_index);
			boost::trim(line);
			results.push_back(line);
			results.push_back(quoted_str);
		} else {
			boost::split(results, line, boost::is_any_of("\t "));
		}

		if (results[0] == "Include") {
			results.erase(results.begin());
			int err;
			if ((err = mc_doinclude(results, mic, perrs, filename.c_str(), lineno, parsed_files))) {
				ret = err;
			}
			lineno++;
			continue;
		}

		tok = &tokens[0];
		while (tok->name) {
			if ((results[0] == tok->name)) {

				//if parameter is more than one in config file -> display warning
				//this is not applicable to KernelExtraCommandLine parameters, which
				//is concatenated when exists more than one
				params_read[tok->name]++;
				if (params_read[tok->name] > 1 && !tok->duplication_allowed) {
					perrs.add(PWARN, "%s: [Parse] %s line %d: %s parameter duplication",
						mic->name.c_str(), filename.c_str(), lineno, tok->name);
				}
				//remove the first position on the list - it's name of the parameter, not a value;
				results.erase(results.begin());
				if ((results.size() < tok->minargs) || (results.size() > tok->maxargs)) {
					perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: parameter %s: invalid argument "
						"count", mic->name.c_str(), filename.c_str(), lineno, results[0].c_str());
					ret = EINVAL;
					break;
				}
				if (tok->verify_path && !path_exists(results[0])) {
					perrs.add(PERROR, "%s: [Parse FATAL] %s line %d: %s path does not exist",
						mic->name.c_str(), filename.c_str(), lineno, tok->name);
					ret = EINVAL;
					break;
				}

				int err;
				if ((err = tok->func(results, mic, perrs, filename.c_str(), lineno))) {
					ret = err;
				}
				break;
			}
			tok++;
		}

		lineno++;
	}
	return ret;
}

static struct mic_info *
_get_miclist_present(int *cnt)
{
	struct mic_info miclist;
	struct mic_info *mic = &miclist;
	struct dirent *file;
	DIR *dp;

	*cnt = 0;

	if ((dp = opendir(MICSYSFSDIR)) == NULL) {
		return NULL;
	}

	while ((file = readdir(dp)) != NULL) {
		if (strncmp(file->d_name, "mic", 3))
			continue;

		mic->next = new mic_info;

		mic = mic->next;
		mic->id = atoi(&file->d_name[3]);
		mic->present = TRUE;
		mic->name = file->d_name;
		mic->config.name = mpssut_configname(mic->name);
		mpss_clear_config(&mic->config);
		(*cnt)++;
	}

	closedir(dp);
	return miclist.next;
}

static struct mic_info *
_add_miclist_not_present(struct mic_info *miclist, int *cnt)
{
	struct mic_info *mic;
	struct mic_info *tmp;
	char confdir[PATH_MAX];
	int id;
	struct dirent *file;
	DIR *dp;

	mpssut_filename(confdir, PATH_MAX, "%s", DEFAULT_CONFDIR);
	if ((dp = opendir(confdir)) == NULL)
		return miclist->next;

	while ((file = readdir(dp)) != NULL) {
		if (sscanf(file->d_name, "mic%d.conf", &id) <= 0)
			continue;

		std::string name = "mic" + std::to_string(id);

		mic = miclist;
		while (mic->next) {
			if (mic->next->id > id) {
				tmp = new mic_info;
				tmp->next = mic->next;
				mic->next = tmp;
				mic = tmp;
				mic->id = id;
				mic->present = FALSE;
				mic->name = name;
				mpss_clear_config(&mic->config);
				mic->config.name = mpssut_configname(mic->name);
				(*cnt)++;
				break;
			} else if (mic->next->id == id) {
				break;
			}

			mic = mic->next;
		}

		if (mic->next == NULL) {
			tmp = new mic_info;
			mic->next = tmp;
			mic = tmp;
			mic->next = NULL;
			mic->id = id;
			mic->present = FALSE;
			mic->name = name;
			mpss_clear_config(&mic->config);
			mic->config.name = mpssut_configname(mic->name);
			(*cnt)++;
		}
	}
	closedir(dp);

	return miclist->next;
}

void
mpss_free_miclist(struct mic_info *miclist)
{
	struct mic_info *mic = miclist;
	struct mic_info *oldmic;

	while (mic) {
		mpss_clear_config(&mic->config);
		oldmic = mic;
		mic = mic->next;
		delete oldmic;
	}
}

static std::shared_ptr<mic_info>
_get_mic_list(int *miccnt, bool use_config_files)
{
	struct mic_info miclist;
	int cnt = -1;

	miclist.next = _get_miclist_present(&cnt);

	//whether we should also search for the configuration files
	//and merge the MIC card entries
	if (use_config_files) {
		miclist.next = _add_miclist_not_present(&miclist, &cnt);
	}

	if (miccnt != NULL)
		*miccnt = cnt;

	return std::shared_ptr<mic_info>(miclist.next, mpss_free_miclist);
}

std::shared_ptr<mic_info>
mpss_get_miclist(int *miccnt, bool use_config_files)
{
	return _get_mic_list(miccnt, use_config_files);
}

struct mic_info *
mpss_find_micid_inlist(struct mic_info *miclist, int micid)
{
	for (auto& mic: miclist) {
		if (mic.id == micid)
			return &mic;
	}
	return NULL;
}

int
exec_command(const std::string& command)
{
	int ret = system(command.c_str());

	if (ret == -1 || (WIFSIGNALED(ret) &&
				(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))) {
		return -1;
	}

	return WEXITSTATUS(ret);
}

int
exec_command(const std::string& command, mpss_elist& output)
{
	FILE* pipe = popen(command.c_str(), "r");
	if (!pipe) {
		return -1;
	}

	char* line = NULL;
	size_t lenght = 0;
	ssize_t read;

	while ((read = getline(&line, &lenght, pipe)) != -1) {
		if (read > 0) {
			if (line[read - 1] == '\n')
				line[read - 1] = '\0';
			output.add(PINFO, "%s", line);
		}
	}
	free(line);

	int ret = pclose(pipe);

	if (ret == -1 || (WIFSIGNALED(ret) &&
				(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))) {
		return -1;
	}

	return WEXITSTATUS(ret);
}

int load_mic_modules()
{
	return exec_command(MIC_MODULES_LOAD);
}

int unload_mic_modules()
{
	return exec_command(MIC_MODULES_UNLOAD);
}

mpss_elist::mpss_elist()
{
	clear();
}

void mpss_elist::print(int level, void (*func)(int level, const char* fmt, ...))
{
	for (auto& err: this->list) {
		if (level >= err.level)
			func(err.level, "%s\n", err.message.c_str());
	}
}

void mpss_elist::clear()
{
	this->list.clear();
	for (auto& elem: this->ecount)
		elem = 0;
}

void mpss_elist::add(int level, const char* format, ...)
{
	level = std::max(level, PERROR);
	level = std::min(level, PINFO);

	this->ecount[level]++;

	char buffer[4096];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	mpss_error err{level, buffer};
	this->list.push_back(err);
}

std::string get_module_version(const std::string& mod_name)
{
	return mpss_readsysfs(mod_name, "version", MODULESYSFSDIR);
}
