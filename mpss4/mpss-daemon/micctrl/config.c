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

/*
 * micctrl --config
 *
 * Display the current configuration information for the MIC card list in a
 * human readable format.
 */
const char *showconfig_help =
"\
micctrl --config [list of coprocessors]\n\
\n\
The --config command displays current configuration of coprocessors in a human\n\
readable format.\n\
\n"
COMMON_HELP;

void display_config(struct mic_info *miclist);

void run_command_config(po::variables_map &vm, const std::vector<std::string> &options)
{
	if (vm.count("help")) {
		micctrl_help(showconfig_help);
		exit(0);
	}

	po::options_description config_options;
	po::store(po::command_line_parser(options).options(config_options).run(), vm);

	display_config(create_miclist(vm, true).get());
}

void display_card_network_config(enum mpss_card_network_type network_type, struct mnetwork_opt& network_opt)
{
	switch (network_type) {
	case mpss_card_network_type::STATIC:
		printf("Static\n");
		println_indent(SECOND_LEVEL_INDENT, "Address:", network_opt.ip);
		println_indent(SECOND_LEVEL_INDENT, "Netmask:", network_opt.netmask);
		println_indent(SECOND_LEVEL_INDENT, "Gateway:", network_opt.gateway);
		break;
	case mpss_card_network_type::DHCP:
		printf("DHCP\n");
		break;
	case mpss_card_network_type::NONE:
		printf("None\n");
		break;
	default:
		throw std::runtime_error("Unknown card network configuration type");
	}
}

void display_host_network_config(enum mpss_host_network_type network_type, struct mnetwork_opt& network_opt)
{
	switch (network_type) {
	case mpss_host_network_type::STATIC:
		printf("Static\n");
		println_indent(SECOND_LEVEL_INDENT, "Address:", network_opt.ip);
		println_indent(SECOND_LEVEL_INDENT, "Netmask:", network_opt.netmask);
		break;
	case mpss_host_network_type::NONE:
		printf("None\n");
		break;
	default:
		throw std::runtime_error("Unknown host network configuration type");
	}
}

/*
 * Display current configuration parameters in a human readable format.
 */
void
display_config(struct mic_info *miclist)
{
	int err = 0;

	putchar('\n');
	for (auto& mic: miclist) {
		printf("%s:\n", mic.name.c_str());
		printf("=============================================================\n");

		if (micctrl_parse_config(&mic, mpssperr, PINFO) == EBADF) {
			err++;
			display(PERROR, "%s: Not configured\n", mic.name.c_str());
			continue;
		}

		println_indent(FIRST_LEVEL_INDENT, "EFI Image:", mic.config.boot.efiimage);
		println_indent(FIRST_LEVEL_INDENT, "Kernel Image:", mic.config.boot.osimage);
		println_indent(FIRST_LEVEL_INDENT, "Kernel Symbols:", mic.config.boot.systemmap);
		printf("\n");

		report_rootdev(&mic);
		printf("\n");
		println_indent(FIRST_LEVEL_INDENT, "Kernel Extra Command Line:", mic.config.boot.extraCmdline);
		printf("\n");

		println_indent(FIRST_LEVEL_INDENT, "Auto Boot:", (mic.config.boot.onstart == TRUE) ? MIC_ENABLED : MIC_DISABLED);
		println_indent(FIRST_LEVEL_INDENT, "Shutdown Timeout:", std::to_string(mic.config.misc.shutdowntimeout));
		println_indent(FIRST_LEVEL_INDENT, "Boot Timeout:", std::to_string(mic.config.misc.boottimeout));
		printf("\n");

		if (mic.config.net.host_type == mpss_host_network_type::BRIDGE ||
			mic.config.net.host_type_inet6 == mpss_host_network_type::BRIDGE) {
			println_indent(FIRST_LEVEL_INDENT, "Host Network:", "Bridge");
			println_indent(SECOND_LEVEL_INDENT, "Bridge Name:", mic.config.net.bridge_name);
		} else {
			print_indent(FIRST_LEVEL_INDENT, "Host Network IPv4:", "");
			display_host_network_config(mic.config.net.host_type, mic.config.net.host_setting);

			print_indent(FIRST_LEVEL_INDENT, "Host Network IPv6:", "");
			display_host_network_config(mic.config.net.host_type_inet6, mic.config.net.host_setting_inet6);
		}

		printf("\n");

		print_indent(FIRST_LEVEL_INDENT, "Card Network IPv4:", "");
		display_card_network_config(mic.config.net.card_type, mic.config.net.card_setting);

		print_indent(FIRST_LEVEL_INDENT, "Card Network IPv6:", "");
		display_card_network_config(mic.config.net.card_type_inet6, mic.config.net.card_setting_inet6);

		printf("\n");
		println_indent(FIRST_LEVEL_INDENT, "Host MAC:", mic.config.net.host_mac.str());
		println_indent(FIRST_LEVEL_INDENT, "Card MAC:", mic.config.net.mic_mac.str());
		printf("\n");

		putchar('\n');
	}

	exit(err);
}

