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
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/dir.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include <syslog.h>
#include <getopt.h>
#include <mpssconfig.h>
#include <libmpsscommon.h>

void
mpssnet_genmask(char *prefix, char* netMask)
{
	int netmask = 0xffffff00, nm[4];

	if (prefix)
		netmask = ~0x0 << (32 - atoi(prefix));

	nm[0] = (netmask >> 24) & 0xff;
	nm[1] = (netmask >> 16) & 0xff;
	nm[2] = (netmask >> 8)  & 0xff;
	nm[3] = (netmask >> 0)  & 0xff;

	snprintf(netMask, 32, "%d.%d.%d.%d", nm[0], nm[1], nm[2], nm[3]);
}


int
mpssnet_decode_ipaddr(char *caddr, int *ipaddr, int num)
{
	char *next = caddr;
	int elem;

	for (elem = 0; elem < num; elem++) {
		*ipaddr = 0xffff;
		if (strlen(next) == 0)
			return 1;

		while (*next && isdigit(*next))
			next++;

		if ((*next == '\0') && (elem < (num - 1)))
			return 1;

		*ipaddr = atoi(caddr);

		if (*ipaddr == 0xffff)
			return 1;

		ipaddr++;
		caddr = ++next;
	}

	return 0;
}

int
mpssnet_set_ipaddrs(struct mic_info *mic, char *micip0, char *hostip0)
{
	int micip1[4];
	int hostip1[4];

	if ((micip0 == NULL) || (hostip0 == NULL)) {
		mic->config.net.type = -1;
		return 1;
	}

	if (mpssnet_decode_ipaddr(micip0, micip1, 4)) {
		mic->config.net.type = -1;
		return 1;
	}

	if (mpssnet_decode_ipaddr(hostip0, hostip1, 4)) {
		mic->config.net.type = -1;
		return 1;
	}

	if ((mic->config.net.micIP = (char *) malloc(strlen(micip0) + 1)) != NULL)
		strcpy(mic->config.net.micIP, micip0);
	if ((mic->config.net.hostIP = (char *) malloc(strlen(hostip0) + 1)) != NULL)
		strcpy(mic->config.net.hostIP, hostip0);
	return 0;
}

char *br_strings[] = {"Broken", "Internal", "External", "Static External"};

char *
mpssnet_brtypes(unsigned int type)
{
	if (type > BRIDGE_TYPE_STATICEXT)
		return "Unknown";

	return (br_strings[type]);
}

struct mbridge *
mpss_insert_bridge(char *name, unsigned int type, char *ip, char *netbits, char *mtu, struct mbridge **brlist,
		   struct mpss_elist *perrs)
{
	struct mbridge top;
	struct mbridge *br = &top;

	top.next = *brlist;

	while (br->next != NULL) {
		if (!strcmp(br->next->name, name)) {
			if (!strcmp(br->next->ip, ip) && (br->next->type == type))
				return br->next;

			add_perr(perrs, PERROR, "[Parse] Bridge %s cannot change previous bridge "
				 "config %s %s to %s %s",
				 name, mpssnet_brtypes(br->next->type), br->next->ip, mpssnet_brtypes(type), ip);
			return NULL;
		}

		br = br->next;
	}

	if ((br->next = (struct mbridge *) malloc(sizeof(struct mbridge))) == NULL) {
		add_perr(perrs, PERROR, "[Parse] Insertbridge mem alloc error %d", errno);
		return NULL;
	}

	if (*brlist == NULL)
		*brlist = br->next;

	br->next->type = type;
	br->next->next = NULL;

	if ((br->next->name = (char *) malloc(strlen(name) + 1)) == NULL) {
		add_perr(perrs, PERROR, "[Parse] Insertbridge mem alloc error %d", errno);
		free(br->next);
		br->next = NULL;
		return NULL;
	}
	strcpy(br->next->name, name);

	if ((br->next->ip = (char *) malloc(strlen(ip) + 1)) == NULL) {
		goto ipmallocfail;
	}
	strcpy(br->next->ip, ip);

	if ((br->next->prefix = (char *) malloc(strlen(netbits) + 1)) == NULL) {
		goto netbitsmallocfail;
	}
	strcpy(br->next->prefix, netbits);

	if (mtu) {
		if ((br->next->mtu = (char *) malloc(strlen(mtu) + 1)) == NULL) {
			goto mtumallocfail;
		}
		strcpy(br->next->mtu, mtu);
	} else {
		br->next->mtu = NULL;
	}
	return br->next;

mtumallocfail:
	free(br->next->prefix);
netbitsmallocfail:
	free(br->next->ip);
ipmallocfail:
	add_perr(perrs, PERROR, "[Parse] Insertbridge mem alloc error %d", errno);
	free(br->next->name);
	free(br->next);
	br->next = NULL;
	return NULL;
}

void
mpss_free_bridgelist(struct mbridge **brlist)
{
	struct mbridge *br1 = *brlist;
	struct mbridge *br2;

	while (br1) {
		br2 = br1->next;
		free(br1);
		br1 = br2;
	}

	*brlist = NULL;
}

struct mbridge *
mpss_bridge_byname(char *name, struct mbridge *brlist)
{
	struct mbridge *br = brlist;

	if (name == NULL)
		return NULL;

	while (br != NULL) {
		if (!strcmp(br->name, name)) {
			return br;
		}

		br = br->next;
	}

	return NULL;
}

struct mbridge *
mpss_bridge_byip(char *ip, struct mbridge *brlist)
{
	struct mbridge *br = brlist;

	if (ip == NULL)
		return NULL;

	while (br != NULL) {
		if (!strcmp(br->ip, ip)) {
			return br;
		}

		br = br->next;
	}

	return NULL;
}
