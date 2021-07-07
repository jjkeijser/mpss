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
#include <mpssconfig.h>

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

static char *
onoff(int on_or_off)
{
	if (on_or_off)
		return "on";
	else
		return "off";
}

int
mpssconfig_read_pm(char *name, int *cpufreq, int *corec6, int *pc3, int *pc6)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	struct mpss_env menv;
	struct mbridge *brlist = NULL;
	struct mpss_elist perrs;
	char *s1;
	char *s2;
	int err = 0;
	int cnt;

	if (mpssenv_init(&menv))
		return CONFIG_ERROR_ENV;

	if ((miclist = mpss_get_miclist(&menv, &cnt)) == NULL)
		return CONFIG_ERROR_EXIST;

	if ((mic = mpss_find_micname_inlist(miclist, name)) == NULL) {
		err = CONFIG_ERROR_EXIST;
		goto free_and_ret;
	}

	if (mpss_parse_config(&menv, mic, &brlist, &perrs)) {
		err = CONFIG_ERROR_PARSE;
		goto free_and_ret;
	}

	s1 = mic->config.boot.pm;
	if ((s2 = strchr(s1, ';')) == NULL) {
		err = CONFIG_ERROR_MALFORMED;
		goto free_and_ret;
	}
	*s2 = '\0';

	if (!strcmp(s1, "cpufreq_on")) {
		*cpufreq = 1;
	} else if (!strcmp(s1, "cpufreq_off")) {
		*cpufreq = 0;
	} else {
		err = CONFIG_ERROR_MALFORMED;
		goto free_and_ret;
	}

	s1 = s2 + 1;
	if ((s2 = strchr(s1, ';')) == NULL) {
		err = CONFIG_ERROR_MALFORMED;
		goto free_and_ret;
	}
	*s2 = '\0';

	if (!strcmp(s1, "corec6_on")) {
		*corec6 = 1;
	} else if (!strcmp(s1, "corec6_off")) {
		*corec6 = 0;
	} else {
		err = CONFIG_ERROR_MALFORMED;
		goto free_and_ret;
	}

	s1 = s2 + 1;
	if ((s2 = strchr(s1, ';')) == NULL) {
		err = CONFIG_ERROR_MALFORMED;
		goto free_and_ret;
	}
	*s2 = '\0';

	if (!strcmp(s1, "pc3_on")) {
		*pc3 = 1;
	} else if (!strcmp(s1, "pc3_off")) {
		*pc3 = 0;
	} else {
		err = CONFIG_ERROR_MALFORMED;
		goto free_and_ret;
	}

	s1 = s2 + 1;
	if (!strcmp(s1, "pc6_on")) {
		*pc6 = 1;
	} else if (!strcmp(s1, "pc6_off")) {
		*pc6 = 0;
	} else {
		err = CONFIG_ERROR_MALFORMED;
		goto free_and_ret;
	}

free_and_ret:
	mpss_free_miclist(miclist);
	return err;
}

int
mpssconfig_update_pm(char *name, int cpufreq, int corec6, int pc3, int pc6)
{
	struct mic_info *miclist;
	struct mic_info *mic;
	struct mpss_env menv;
	int err = 0;

	if (mpssenv_init(&menv))
		return CONFIG_ERROR_ENV;


	if ((miclist = mpss_get_miclist(&menv, NULL)) == NULL)
		return CONFIG_ERROR_EXIST;

	if ((mic = mpss_find_micname_inlist(miclist, name)) == NULL) {
		err = CONFIG_ERROR_EXIST;
		goto free_and_ret;
	}

	if (mpss_update_config(&menv, mic->config.name, "PowerManagement", NULL,
			   "PowerManagement \"cpufreq_%s;corec6_%s;pc3_%s;pc6_%s\"\n",
			   onoff(cpufreq), onoff(corec6), onoff(pc3), onoff(pc6)))
		err = CONFIG_ERROR_WRITE;

free_and_ret:
	mpss_free_miclist(miclist);
	return err;
}


