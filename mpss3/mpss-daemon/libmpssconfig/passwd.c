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

int
parse_pwfile(char *user, char **pw, uid_t *uid, gid_t *gid, char **name, char **home, char **app)
{
	char *tmp1;
	char *tmp2;

	if ((*pw = strchr(user, ':')) == NULL)
		goto parse_pw_error;
	**pw = '\0';
	(*pw)++;

	if ((tmp1 = strchr(*pw, ':')) == NULL)
		goto parse_pw_error;
	*tmp1 = '\0';
	tmp1++;

	if ((tmp2 = strchr(tmp1, ':')) == NULL)
		goto parse_pw_error;
	*tmp2= '\0';
	tmp2++;

	*uid = atoi(tmp1);
	*gid = atoi(tmp2);

	if ((*name = strchr(tmp2, ':')) == NULL)
		goto parse_pw_error;
	**name = '\0';
	(*name)++;

	if ((*home = strchr(*name, ':')) == NULL)
		goto parse_pw_error;
	**home = '\0';
	(*home)++;

	if ((*app = strchr(*home, ':')) == NULL)
		goto parse_pw_error;
	**app = '\0';
	(*app)++;

	if ((tmp1 = strchr(*app, '\n')) == NULL)
		goto parse_pw_error;
	*tmp1 = '\0';
	return 0;

parse_pw_error:
	*uid = *gid = -1;
	*pw = *name = *home = *app = NULL;
	return 1;
}

int
parse_shadow(char *user, char **pw, char **lastd, char **minage, char **maxage, char **passwarn,
	     char **inactive, char **expire)
{
	if ((*pw = strchr(user, ':')) == NULL)
		goto parse_shadow_error;
	**pw = '\0';
	(*pw)++;

	if ((*lastd = strchr(*pw, ':')) == NULL)
		goto parse_shadow_error;
	**lastd = '\0';
	if (*lastd[1] != ':')
		(*lastd)++;

	if ((*minage = strchr(*lastd, ':')) == NULL)
		goto parse_shadow_error;
	**minage = '\0';
	if (*minage[1] != ':')
		(*minage)++;

	if ((*maxage = strchr(*minage, ':')) == NULL)
		goto parse_shadow_error;
	**maxage = '\0';
	if (*maxage[1] != ':')
		(*maxage)++;

	if ((*passwarn = strchr(*maxage, ':')) == NULL)
		goto parse_shadow_error;
	**passwarn = '\0';
	if (*passwarn[1] != ':')
		(*passwarn)++;

	if ((*inactive = strchr(*passwarn, ':')) == NULL)
		goto parse_shadow_error;
	**inactive = '\0';
	if (*inactive[1] != ':')
		(*inactive)++;

	if ((*expire = strchr(*inactive, ':')) == NULL)
		goto parse_shadow_error;
	**expire = '\0';
	return 0;

parse_shadow_error:
	pw = lastd = minage = maxage = passwarn = inactive = expire = NULL;
	return 1;
}

int
parse_group(char *group, char **pw, gid_t *gid, char **users)
{
	char *tmp;

	if ((*pw = strchr(group, ':')) == NULL)
		goto parse_gr_error;
	**pw = '\0';
	(*pw)++;

	if ((tmp = strchr(*pw, ':')) == NULL)
		goto parse_gr_error;
	*tmp = '\0';
	tmp++;

	*gid = atoi(tmp);

	if ((*users = strchr(tmp, ':')) == NULL)
		goto parse_gr_error;
	**users = '\0';
	(*users)++;

	if ((tmp = strchr(*users, '\n')) != NULL)
		*tmp = '\0';
	return 0;

parse_gr_error:
	*gid = -1;
	*pw = *users = NULL;
	return 1;
}

