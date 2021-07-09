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

const char *cardlog_help =
"\
micctrl -l [sub options] [list of coprocessors]\n\
micctrl --log [sub options] [list of coprocessors]\n\
\n\
The --log option prints kernel log of all or specified coprocessors.\n\
\n"
COMMON_HELP
;

const unsigned char CARD_IP_LAST_BYTE = 1;
const unsigned char HOST_IP_LAST_BYTE = 254;

void
print_card_log(struct mic_info *miclist)
{
	bump_display_level(); // required to have INFO messages printed without increasing verbosity
	for (auto& mic: miclist) {
		display(PNORM, "%s:\n", mic.name.c_str());
		display(PNORM, "=============================================================\n");

		mpss_elist mpssperr;
		if (micctrl_parse_config(&mic, mpssperr, PINFO)) {
			display(PWARN, "%s: Not configured - skipping\n", mic.name.c_str());
			continue;
		}

		int err;
		if ((err = mpssut_print_card_log(&mic, mpssperr)))
			display(PERROR, "%s: cannot print MIC kernel log:%s (%d)\n", mic.name.c_str(), strerror(err), err);

		mpssperr.print(PINFO, &display_raw);
	}
}

void run_command_log(po::variables_map &vm, const std::vector<std::string> &options)
{
	if (vm.count("help")) {
		micctrl_help(cardlog_help);
		exit(0);
	}

	po::options_description log_options;
	po::store(po::command_line_parser(options).options(log_options).run(), vm);

	print_card_log(create_miclist(vm).get());
	exit(0);
}

void println_indent(int level, const std::string& key, const std::string& value)
{
	printf("%*s%-*s%s\n", level, " ", CONFIG_DISPLAY_LENGTH - level, key.c_str(), value.c_str());
}

void print_indent(int level, const std::string& key, const std::string& value)
{
	printf("%*s%-*s%s", level, " ", CONFIG_DISPLAY_LENGTH - level, key.c_str(), value.c_str());
}

std::string get_netmask()
{
	return "255.255.255.0";
}
std::string get_default_ip_address(int mic_id, unsigned char last)
{
	int id = mic_id + 1;
	return "172.31." + std::to_string(id) + "." + std::to_string(last);
}
std::string get_default_card_ip_address(int mic_id)
{
	return get_default_ip_address(mic_id, CARD_IP_LAST_BYTE);
}
std::string get_default_host_ip_address(int mic_id)
{
	return get_default_ip_address(mic_id, HOST_IP_LAST_BYTE);
}
