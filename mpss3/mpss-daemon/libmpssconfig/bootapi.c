/*
 * Copyright 2010-2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * Disclaimer: The codes contained in these modules may be specific to
 * the Intel Software Development Platform codenamed Knights Ferry,
 * and the Intel product codenamed Knights Corner, and are not backward
 * compatible with other Intel products. Additionally, Intel will NOT
 * support the codes or instruction set in future products.
 *
 * Intel offers no warranty of any kind regarding the code. This code is
 * licensed on an "AS IS" basis and Intel is not obligated to provide
 * any support, assistance, installation, training, or other services
 * of any kind. Intel is also not obligated to provide any updates,
 * enhancements or extensions. Intel specifically disclaims any warranty
 * of merchantability, non-infringement, fitness for any particular
 * purpose, and any other warranty.
 *
 * Further, Intel disclaims all liability of any kind, including but
 * not limited to liability for infringement of any proprietary rights,
 * relating to the use of the code, even if Intel is notified of the
 * possibility of such liability. Except as expressly stated in an Intel
 * license agreement provided with this code and agreed upon with Intel,
 * no license, express or implied, by estoppel or otherwise, to any
 * intellectual property rights is granted herein.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>

#define __symver_tag MPSSCONFIG

#define __symver(S, at, M, N) \
        __asm__ (".symver " __str(V(S, M, N)) "," __V(S, at, M, N));
#define __V(symbol, at, major, minor) \
        __str(symbol) at __str(__symver_tag) "_" __str(major) "." __str(minor)
#define __str(s) ___str(s)
#define ___str(s) #s

#ifndef GENMAP_PARSING_PASS
#define V(symbol, major, minor) \
        __ ## symbol ## _ ## major ## _ ## minor
#define compatible_version(symbol, major, minor) \
        __symver(symbol, "@", major, minor)
#define default_version(symbol, major, minor) \
        __symver(symbol, "@@", major, minor)
#define only_version(symbol, major, minor)
#endif

void
boot_linux(int node)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	struct mpss_env mpssenv;
	struct mbridge *brlist = NULL;
	struct mpss_elist perrs;
	struct stat sbuf;
	char boot_string[PATH_MAX];
	char name[8];
	char os_image_path[PATH_MAX];
	char kernel[PATH_MAX];
	char ramdisk[PATH_MAX];
	char efiimage[PATH_MAX];
	char cmdline[2048];
	char *initrd = NULL;
	char *errmsg = NULL;
	int cnt;

	if (mpssenv_init(&mpssenv))
		return;

	if ((miclist = mpss_get_miclist(&mpssenv, &cnt)) == NULL)
		return;

	memset(boot_string, 0, PATH_MAX);
	memset(os_image_path, 0, PATH_MAX);
	memset(kernel, 0, PATH_MAX);
	memset(ramdisk, 0, PATH_MAX);
	memset(efiimage, 0, PATH_MAX);
	memset(name, 0, 8);

	snprintf(name, 8, "mic%d", node);

	if ((mic = mpss_find_micname_inlist(miclist, name)) == NULL) {
		goto linux_ret;
	}

	if (mpss_parse_config(&mpssenv, mic, &brlist, &perrs)) {
		goto linux_ret;
	}

	if (mic->config.boot.osimage == NULL) {
		goto linux_ret;
	}

	if (stat(mic->config.boot.osimage, &sbuf) != 0) {
		goto linux_ret;
	}

	if (verify_bzImage(&mpssenv, mic->config.boot.osimage, mic->name)) {
		goto linux_ret;
	}

	if ((errmsg = mpss_set_cmdline(mic, brlist, cmdline, NULL)) != NULL) {
		goto linux_ret;

	}

	if (stat(mic->config.rootdev.target, &sbuf) == 0)
		unlink(mic->config.rootdev.target);

	if (mpssfs_gen_initrd(&mpssenv, mic, &perrs)) {
		goto linux_ret;
	}

	mpss_clear_elist(&perrs);
	initrd = mic->config.rootdev.target;

	snprintf(boot_string, PATH_MAX, "boot:linux:%s:%s", mic->config.boot.osimage, initrd);

	mpss_setsysfs(mic->name, "state", boot_string);

linux_ret:
	mpss_free_miclist(miclist);
	mpssenv_clean(&mpssenv);
	return;
}

void
boot_media(int node, const char *media_path)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	struct mpss_env mpssenv;
	char boot_string[PATH_MAX];
	char os_image_path[PATH_MAX];
	char name[8];
	int cnt;

	if (mpssenv_init(&mpssenv))
		return;

	if ((miclist = mpss_get_miclist(&mpssenv, &cnt)) == NULL)
		return;

	memset(boot_string, 0, PATH_MAX);
	memset(os_image_path, 0, PATH_MAX);
	memset(name, 0, 8);

	snprintf(name, 8, "mic%d", node);
	snprintf(os_image_path, PATH_MAX, "/lib/firmware/%s", media_path);
	snprintf(boot_string, PATH_MAX, "boot");

	if ((mic = mpss_find_micname_inlist(miclist, name)) == NULL) {
		goto media_ret;
	}

	return;

media_ret:
	mpss_free_miclist(miclist);
	mpssenv_clean(&mpssenv);
	return;
}

char *
get_state(int node)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	struct mpss_env mpssenv;
	char name[8];
	char *state;
	char *micstates[4] = { "ready", "booting", "online", "unknown" };
	int cnt;
	int index = 3;

	if (mpssenv_init(&mpssenv))
		goto state_ret;

	if ((miclist = mpss_get_miclist(&mpssenv, &cnt)) == NULL)
		goto state_ret;

	memset(name, 0, 8);
	snprintf(name, 8, "mic%d", node);

	if ((mic = mpss_find_micname_inlist(miclist, name)) == NULL) {
		goto state_free;
	}

	if ((state = mpss_readsysfs(mic->name, "state")) == NULL) {
		goto state_free;
	}

	if (!strcmp(state, "ready"))
		index = 0;
	else if (!strcmp(state, "booting"))
		index = 1;
	else if (!strcmp(state, "online"))
		index = 2;
	else
		index = 3;

	free(state);

state_free:
	mpss_free_miclist(miclist);
	mpssenv_clean(&mpssenv);
state_ret:
	return micstates[index];
}

void
reset_node(int node)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	struct mpss_env mpssenv;
	char name[8];
	int cnt;

	if (mpssenv_init(&mpssenv))
		return;

	if ((miclist = mpss_get_miclist(&mpssenv, &cnt)) == NULL)
		return;

	memset(name, 0, 8);
	snprintf(name, 8, "mic%d", node);

	if ((mic = mpss_find_micname_inlist(miclist, name)) == NULL) {
		goto reset_free;
	}

	mpss_setsysfs(mic->name, "state", "reset");

reset_free:
	mpss_free_miclist(miclist);
	mpssenv_clean(&mpssenv);
}

