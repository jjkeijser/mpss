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

#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * Create the list of mic cards in the system.  If no cards are given on the command line
 * it calls the from_probe() function to get the list.  Otherwise it builds a list of
 * the cards from the command line list.
 */
std::shared_ptr<mic_info>
create_miclist(const po::variables_map &vm, bool use_config_files)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	int cnt = 0;
	std::vector<std::string> card_vec;
	std::shared_ptr<mic_info> miclistsp;

	// If no card list is specified then use all the cards.  This requires the mic.ko
	// driver to be loaded.
	if (!vm.count("card-list")) {
		miclistsp = mpss_get_miclist(&cnt, use_config_files);
		goto listdone;
	}

	miclist = new mic_info;
	mic = miclist;

	card_vec = vm["card-list"].as< std::vector<std::string> >();
	for (auto it = card_vec.begin(); it != card_vec.end(); ++it) {
		sscanf(it->c_str(), "mic%d", &mic->id);
		std::string tempname = "mic" + std::to_string(mic->id);

		if ((mic->id < 0) || (mic->id > 255) || (tempname != *it)) {
			printf("\n\tError: Invalid device name \"%s\"\n", it->c_str());
			printf("\tInfo:  Valid device name is composed from two parts: \"mic\" and number from 0 - 255\n");
			exit(EXIT_FAILURE);
		}

		mic->present = FALSE;
		mic->name = tempname;
		mpss_clear_config(&mic->config);
		mic->config.name = mpssut_configname(mic->name);

		std::string sysfsdir = MICSYSFSDIR "/" + mic->name;
		if (path_exists(sysfsdir))
			mic->present = TRUE;

		cnt++;

		if (cnt == (int)card_vec.size()) {
			mic->next = NULL;
		} else {
			mic->next = new mic_info;
			mic = mic->next;
		}
	}
	miclistsp = std::shared_ptr<mic_info>(miclist, mpss_free_miclist);

listdone:
	if (cnt == 0) {
		display(PWARN, "No Mic cards found or specified on command line\n");
		exit(0);
	}

	return miclistsp;
}

int
micctrl_parse_config(struct mic_info *mic, mpss_elist& perrs, int errlev)
{
	int err = mpss_parse_config(mic, perrs);

	if ((errlev < PERROR) || (errlev > PINFO))
		errlev = PINFO;

	perrs.print(errlev, &display);
	perrs.clear();

	return err;
}

const char *clev_color[] = {
	"  \033[31m[Error]\033[0m ",
	"\033[33m[Warning]\033[0m ",
	"",
	"   \033[32m[Info]\033[0m ",
	"\033[34m[Filesys]\033[0m ",
	"\033[35m[Network]\033[0m " };

const char *clev_plain[] = {
	"  [Error] ",
	"[Warning] ",
	"",
	"   [Info] ",
	"[Filesys] ",
	"[Network] " };

unsigned char display_level = PNORM;

void
display(int  level, const char *format, ...)
{
	va_list args;
	char buffer[4096];
	FILE *stream;

	if ((level > display_level) || (level > PNET))
		return;

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	if (level == PWARN || level == PERROR) {
		stream = stderr;
	} else {
		stream = stdout;
	}

	if (isatty(1))
		fprintf(stream, "%s", clev_color[level]);
	else
		fprintf(stream, "%s", clev_plain[level]);

	fprintf(stream, "%s", buffer);

	fflush(stream);
}

void
display_raw(int  level, const char *format, ...)
{
	va_list args;

	if ((level > display_level) || (level > PNET))
		return;

	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	fflush(stdout);
}

const char *dis_indent = "          ";
const char *
display_indent(void)
{
	return dis_indent;
}

void
bump_display_level(void)
{
	display_level++;
}

void
set_display_level(unsigned char level)
{
	display_level = level;
}

/*
 * Used by routines that must run under root ID.
 */
void
check_rootid(void)
{
	uid_t euid = geteuid();

	if (euid != 0) {
		display(PERROR, "Only root is allowed to use this option\n");
		exit(EXIT_FAILURE);
	}
}

bool
is_mic_images_being_used(struct mic_info *mics)
{
	if (!mpssenv_aquire_lockfile()) {
		return false;
	}

	for (auto& mic: mics) {
		int state = mic_get_state_id(&mic, NULL);

		switch (state) {
		case MIC_STATE_BOOTING:
		case MIC_STATE_ONLINE:
		case MIC_STATE_RESETTING:
		case MIC_STATE_SHUTTING_DOWN:
			return true;
		}
	}

	return false;
}

bool prompt_user()
{
	bool yes;
	int c = getchar();

	yes = (c == 'y' || c == 'Y');
	while (c != '\n' && c != EOF)
		c = getchar();
	return yes;
}

