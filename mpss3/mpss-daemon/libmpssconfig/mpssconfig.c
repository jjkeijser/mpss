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
#include <ctype.h>
#include "mpssconfig.h"
#include "libmpsscommon.h"

struct tokens {
	char		*name;
	int		namelen;
	int		minargs;
	int		maxargs;
	int 		(*func)(char **args, struct mpss_env *menv, struct
				mic_info *mic, struct mbridge **brlist,
				struct mpss_elist *perr, int level, char *cfile, int lineno);
};

int parse_config_file(struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
		      char *filename, struct mpss_elist *perrs, int level);
void init_perror(struct mpss_elist *perrs);

int
mpss_parse_config(struct mpss_env *menv, struct mic_info *mic,
		  struct mbridge **brlist, struct mpss_elist *perrs)
{
	init_perror(perrs);
	mpss_clear_config(&mic->config);
	return parse_config_file(menv, mic, brlist, mic->config.name, perrs, 0);
}

int
mc_version(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	   struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *major = args[0];
	char *minor = args[1];

	mic->config.version = (atoi(major) << 16) + atoi(minor);

	if (mic->config.version < MPSS_CONFIG_VER(1, 0)) {
		add_perr(perrs, PERROR, "[Parse FATAL] %s line %d", cfile, lineno);
		add_perr(perrs, PERADD, "     Outdated configuration version %s.%s", major, minor);
		add_perr(perrs, PERADD, "     Cannot parse versions older than 1.0");
		mic->config.valid = FALSE;
		return EINVAL;
	}

	if (mic->config.version > MPSS_CURRENT_CONFIG_VER) {
		add_perr(perrs, PERROR, "[Parse FATAL] %s line %d", cfile, lineno);
		add_perr(perrs, PERADD, "     Configuration version %d.%d newer than utility version %d.%d",
				(mic->config.version >> 16) & 0xff, mic->config.version & 0xff,
				CURRENT_CONFIG_MAJOR, CURRENT_CONFIG_MINOR);
		add_perr(perrs, PERADD, "     Cannot parse configurations newer than parser version -"
					" check your installation");
		mic->config.valid = FALSE;
		return EIO;
	}

	if (mic->config.version != MPSS_CURRENT_CONFIG_VER) {
		add_perr(perrs, PWARN, "[Parse] %s line %d", cfile, lineno);
		add_perr(perrs, PWARN, "     Configuration %d.%d version older than parser version %d.%d",
			      (mic->config.version >> 16) & 0xff, mic->config.version & 0xff,
			      CURRENT_CONFIG_MAJOR, CURRENT_CONFIG_MINOR);
		return 0;
	}

	add_perr(perrs, PINFO, "%s: [Parse] Configuration version %s.%s", mic->name, major, minor);
	return 0;
}

int
doconfdir(struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	  struct mpss_elist *perrs, int level)
{
	char subconfdir[PATH_MAX];
	char filename[PATH_MAX];
	struct dirent *file;
	DIR *dp;

	mpssut_filename(menv, NULL, subconfdir, sizeof(subconfdir), "%s/%s",  menv->confdir, "conf.d");

	if ((dp = opendir(subconfdir)) == NULL)
		return 0;

	while ((file = readdir(dp)) != NULL) {
		if (strlen(file->d_name) < strlen(".conf") + 1)
			continue;

		if (strncmp(&file->d_name[strlen(file->d_name) - 5], ".conf", 5))
			continue;

		snprintf(filename, PATH_MAX, "%s/%s", subconfdir, file->d_name);
		// TODO Handle error returns from parse_config_file
		parse_config_file(menv, mic, brlist, filename, perrs, level + 1);
	}

	closedir(dp);
	return 0;
}

int
mc_osimage(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	   struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *image = args[0];
	char *map = args[1];

	if (image == NULL) {
		add_perr(perrs, PERROR, "[Parse ERROR] %s line %d", cfile, lineno);
		add_perr(perrs, PERADD, "    OSImage parameter requires 2 arguments and has none\n");
		mic->config.boot.osimage = NULL;
		mic->config.boot.systemmap = NULL;
		return 0;
	}

	if ((mic->config.boot.osimage = (char *) malloc(strlen(image) + 1)) != NULL) {
		strcpy(mic->config.boot.osimage, image);
	}

	if (map == NULL) {
		add_perr(perrs, PERROR, "[Parse ERROR] %s line %d", cfile, lineno);
		add_perr(perrs, PERADD, "    OSImage parameter requires 2 arguments and has only 1\n");
		mic->config.boot.systemmap = NULL;
		return 0;
	}

	if ((mic->config.boot.systemmap = (char *) malloc(strlen(map) + 1)) != NULL) {
		strcpy(mic->config.boot.systemmap, map);
	}

	return 0;
}

int
mc_efiimage(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	    struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *image = args[0];

	if (image == NULL) {
		add_perr(perrs, PERROR, "[Parse ERROR] %s line %d", cfile, lineno);
		add_perr(perrs, PERADD, "    EFImage parameter requires 1 argument and has none\n");
		mic->config.boot.efiimage = NULL;
		return 0;
	}

	if ((mic->config.boot.efiimage = (char *) malloc(strlen(image) + 1)) != NULL) {
		strcpy(mic->config.boot.efiimage, image);
	}

	return 0;
}

int
mc_family(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
		struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *family = args[0];

	if (family == NULL) {
		add_perr(perrs, PERROR, "[Parse ERROR] %s line %d", cfile, lineno);
		add_perr(perrs, PERADD, "    Family parameter requires 1 argument and has none\n");
		mic->config.family = MIC_FAMILY_UNKNOWN_VALUE;
		return 0;
	}

	if (!strcmp(family, MIC_FAMILY_KNC))
		mic->config.family = MIC_FAMILY_KNC_VALUE;
	else if (!strcmp(family, MIC_FAMILY_KNL))
		mic->config.family = MIC_FAMILY_KNL_VALUE;
	else {
		add_perr(perrs, PERROR, "[Parse ERROR] Family value %s not allowed. Possible values: %s, %s", family, MIC_FAMILY_KNC, MIC_FAMILY_KNL);
	}

	return 0;
}

int
mc_mpss_version(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
		struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *mpss_version = args[0];

	if (mpss_version == NULL) {
		add_perr(perrs, PERROR, "[Parse ERROR] %s line %d", cfile, lineno);
		add_perr(perrs, PERADD, "    MPSSVersion parameter requires 1 argument and has none\n");
		mic->config.mpss_version = MPSS_VERSION_UNKNOWN_VALUE;
		return 0;
	}

	if (!strcmp(mpss_version, MPSS_VERSION_3))
		mic->config.mpss_version = MPSS_VERSION_3_VALUE;
	else if (!strcmp(mpss_version, MPSS_VERSION_4))
		mic->config.mpss_version = MPSS_VERSION_4_VALUE;
	else {
		add_perr(perrs, PERROR, "[Parse ERROR] MPSSVersion value %s not allowed. Possible values: %s, %s", mpss_version, MPSS_VERSION_3, MPSS_VERSION_4);
	}

	return 0;
}

int
mc_doinclude(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	     struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *name = args[0];
	char filename[PATH_MAX];

	if (!strcmp(name, "conf.d/*.conf")) {
		doconfdir(menv, mic, brlist, perrs, level + 1);
		return 0;
	}

	if (name[0] == '/') {
		strncpy(filename, name, PATH_MAX - 1);
		filename[PATH_MAX - 1] = '\0';
	} else {
		mpssut_filename(menv, NULL, filename, sizeof(filename), "%s/%s",  menv->confdir, name);
	}

	// TODO Handle error returns from parse_config_file
	parse_config_file(menv, mic, brlist, filename, perrs, level + 1);
	return 0;
}

int
mc_bridge(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	  struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *name = args[0];
	char *type = args[1];
	char *ip = args[2];
	char *netbits = args[3];
	char *mtu = args[4];
	int br_type;

	if (!strcasecmp(type, "Internal")) {
		br_type = BRIDGE_TYPE_INT;
	} else if (!strcasecmp(type, "External")) {
		if (!strcmp(ip, "dhcp")) {
			br_type = BRIDGE_TYPE_EXT;
		} else {
			br_type = BRIDGE_TYPE_STATICEXT;
		}
	} else {
		add_perr(perrs, PERROR, "%s: [Parse] Bridge '%s' type '%s' not allowed", mic->name, name, type);
		return 0;
	}

	if (mpss_insert_bridge(name, br_type, ip, netbits, mtu, brlist, perrs) == NULL) {
		add_perr(perrs, PERROR, "[Parse] Error inserting new bridge");
	}
	return 0;
}

int
mc_network(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	   struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char **arg = args;
	char *value;
	struct mbridge *br;
	int netclass = NOTSET;
	char *bridge = NULL;
	char *micip0 = NULL;
	char *hostip0 = NULL;
	char *modhost = NULL;
	char *modcard = NULL;
	char *netbits = NULL;
	char *mtu = NULL;
	int bits = 0;
	int micip1[4];

	while (*arg != NULL) {
		if ((value = strchr(*arg, '=') + 1) == NULL) {
			add_perr(perrs, PWARN, "%s: [Parse] Network invalid subparameter %s", mic->name, *arg);
			continue;
		}

		if (!strncasecmp(*arg, "class=", strlen("class="))) {
			if (!strcasecmp(value, "StaticPair")) {
				netclass = NETWORK_TYPE_STATPAIR;
			} else if (!strcasecmp(value, "StaticBridge")) {
				netclass = NETWORK_TYPE_STATBRIDGE;
			} else if (!strcasecmp(value, "Bridge")) {
				netclass = NETWORK_TYPE_BRIDGE;
			} else {
			}
		} else if (!strncasecmp(*arg, "bridge=", strlen("bridge="))) {
			bridge = value;
		} else if (!strncasecmp(*arg, "micip=", strlen("micip="))) {
			micip0 = value;
		} else if (!strncasecmp(*arg, "hostip=", strlen("hostip="))) {
			hostip0 = value;
		} else if (!strncasecmp(*arg, "gw=", strlen("gw="))) {
		} else if (!strncasecmp(*arg, "modhost=", strlen("modhost="))) {
			modhost = value;
		} else if (!strncasecmp(*arg, "modcard=", strlen("modcard="))) {
			modcard = value;
		} else if (!strncasecmp(*arg, "netbits=", strlen("netbits="))) {
			netbits = value;
		} else if (!strncasecmp(*arg, "mtu=", strlen("mtu="))) {
			mtu = value;
		} else {
			add_perr(perrs, PWARN, "%s: [Parse] Network invalid subparameter '%s'", mic->name, *arg);
		}

		arg++;
	}

	switch (netclass) {
	case NETWORK_TYPE_STATPAIR:
		if (mpssnet_set_ipaddrs(mic, micip0, hostip0))
			return 0;

		mic->config.net.type = NETWORK_TYPE_STATPAIR;

		if (netbits == NULL)
			netbits = "24";

		bits = atoi(netbits);

		if ((bits > 24) || (bits < 9)) {
			add_perr(perrs, PWARN, "[Parse] Network bits value %d invalid - using defaut value 24", bits);
			if ((mic->config.net.prefix = (char *) malloc(3)) != NULL)
				strcpy(mic->config.net.prefix, "24");
		} else {
			if ((mic->config.net.prefix = (char *) malloc(strlen(netbits) + 1)) != NULL)
				strcpy(mic->config.net.prefix, netbits);
		}

		if (mtu != NULL) {
			if ((mic->config.net.mtu = (char *) malloc(strlen(mtu) + 1)) != NULL)
				strcpy(mic->config.net.mtu, mtu);
		}

		if (modhost) {
			if (!strcasecmp(modhost, "yes")) {
				mic->config.net.modhost = TRUE;
			} else if (!strcasecmp(modhost, "no")) {
				mic->config.net.modhost = FALSE;
			} else {
				add_perr(perrs, PWARN, "%s: [Parse] Network option '%s' incorrrect - "
					      "defaulting to 'yes'", mic->name, modhost);
			}
		}

		if (modcard) {
			if ((mic->config.net.modcard = (char *) malloc(strlen(modcard)+1)) != NULL)
					strcpy(mic->config.net.modcard, modcard);
		}

		break;

	case NETWORK_TYPE_STATBRIDGE:
		if (bridge == NULL) {
			mic->config.net.type = NOTSET;
			return 0;
		}

		if (mpssnet_decode_ipaddr(micip0, micip1, 4)) {
			add_perr(perrs, PERROR, "%s: [Parse] IP %s not valid", mic->name, micip0);
			mic->config.net.type = NOTSET;
			return 0;
		}

		if ((mic->config.net.micIP = (char *) malloc(strlen(micip0) + 1)) != NULL)
			strcpy(mic->config.net.micIP, micip0);

		if ((br = mpss_bridge_byname(bridge, *brlist)) == NULL) {
			add_perr(perrs, PERROR, "%s: [Parse] Bridge %s not yet configured", mic->name, bridge);
			mic->config.net.type = NOTSET;
			return 0;
		}

		mic->config.net.type = NETWORK_TYPE_STATBRIDGE;
		if ((mic->config.net.bridge = (char *) malloc(strlen(bridge) + 1)) != NULL)
			strcpy(mic->config.net.bridge, bridge);

		if (modhost) {
			if (!strcasecmp(modhost, "yes")) {
				mic->config.net.modhost = TRUE;
			} else if (!strcasecmp(modhost, "no")) {
				mic->config.net.modhost = FALSE;
			} else {
				add_perr(perrs, PWARN, "%s: [Parse] Network option '%s' incorrrect - "
					      "defaulting to 'yes'", mic->name, modhost);
			}
		}

		if (modcard) {
			if ((mic->config.net.modcard = (char *) malloc(strlen(modcard)+1)) != NULL)
					strcpy(mic->config.net.modcard, modcard);
		}

		break;

	case NETWORK_TYPE_BRIDGE:
		if (bridge == NULL) {
			mic->config.net.type = NOTSET;
			return 0;
		}

		if ((br = mpss_bridge_byname(bridge, *brlist)) == NULL) {
			add_perr(perrs, PERROR, "%s: [Parse] Bridge %s not yet configured", mic->name, bridge);
			mic->config.net.type = NOTSET;
			return 0;
		}

		mic->config.net.type = NETWORK_TYPE_BRIDGE;
		if ((mic->config.net.micIP = (char *) malloc(strlen("dhcp") + 1)) != NULL)
			strcpy(mic->config.net.micIP, "dhcp");
		if ((mic->config.net.bridge = (char *) malloc(strlen(bridge) + 1)) != NULL)
			strcpy(mic->config.net.bridge, bridge);
	}
	return 0;
}

int
mc_mac(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
       struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *type = args[0];
	char *hostmac;
	char *micmac;

	if (!strcasecmp(type, "Serial")) {
		mic->config.net.hostMac = MIC_MAC_SERIAL;
	} else if (!strcasecmp(type, "Random")) {
		mic->config.net.hostMac = MIC_MAC_RANDOM;
	} else {
		hostmac = args[0];
		micmac = args[1];

		if ((hostmac == NULL) || (micmac == NULL)) {
			add_perr(perrs, PERROR, "%s: [Parse] Check MacAddrs syntax", mic->name);
			return 0;
		}

		if ((mic->config.net.micMac = (char *) malloc(strlen(micmac) + 1)) != NULL) {
			strcpy(mic->config.net.micMac, micmac);
		}

		if ((mic->config.net.hostMac = (char *) malloc(strlen(hostmac) + 1)) != NULL) {
			strcpy(mic->config.net.hostMac, hostmac);
		}
	}
	return 0;
}

int
mc_base(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *type = args[0];
	char *imagename = args[1];

	if (imagename == NULL) {
		add_perr(perrs, PERROR, "%s: [Parse] Malformed Base paramater lacks an image name", mic->name);
		mic->config.filesrc.base.type = SRCTYPE_UNKNOWN;
		mic->config.filesrc.base.image = NULL;
		return 0;
	}

	if (!strcasecmp(type, "CPIO")) {
		mic->config.filesrc.base.type = SRCTYPE_CPIO;
		mpssut_realloc_fill(&mic->config.filesrc.base.image, imagename);
	} else if (!strcasecmp(type, "DIR")) {
		mic->config.filesrc.base.type = SRCTYPE_DIR;
		mpssut_realloc_fill(&mic->config.filesrc.base.dir, imagename);
	} else {
		add_perr(perrs, PERROR, "%s: [Parse] Unkown Base parameter type '%s'", mic->name, type);
		mic->config.filesrc.base.type = SRCTYPE_UNKNOWN;
		mic->config.filesrc.base.image = NULL;
	}
	return 0;
}

int
mc_commondir(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	     struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *dirname = args[0];
	char *listname = args[1];

	mpssut_realloc_fill(&mic->config.filesrc.common.dir, dirname);

	if (listname != NULL) {
		add_perr(perrs, PWARN, "[Parse] CommonDir: %s line %d", cfile, lineno);
		add_perr(perrs, PWRADD, "     The %s filelist argument deprected.  If using micctrl", listname);
		add_perr(perrs, PWRADD, "     consider a cleanconfig - initdefaults cycle to recreate");
		mic->config.filesrc.common.type = SRCTYPE_FILELIST;
		mpssut_realloc_fill(&mic->config.filesrc.common.list, listname);
	} else {
		mic->config.filesrc.common.type = SRCTYPE_DIR;
	}
	return 0;
}

int
mc_micdir(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	  struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *dirname = args[0];
	char *listname = args[1];

	mpssut_realloc_fill(&mic->config.filesrc.mic.dir, dirname);

	if (listname != NULL) {
		add_perr(perrs, PWARN, "[Parse] MicDir: %s line %d", cfile, lineno);
		add_perr(perrs, PWRADD, "     The %s filelist argument deprected.  If using micctrl", listname);
		add_perr(perrs, PWRADD, "     consider a cleanconfig - initdefaults cycle to recreate");
		mic->config.filesrc.mic.type = SRCTYPE_FILELIST;
		mpssut_realloc_fill(&mic->config.filesrc.mic.list, listname);
	} else {
		mic->config.filesrc.mic.type = SRCTYPE_DIR;
	}
	return 0;
}

int
mc_overlay(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	   struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *type = args[0];
	char *source = args[1];
	char *target = args[2];
	char *state = args[3];
	struct moverdir *dir = &mic->config.filesrc.overlays;
	int otype;
	int ostate;

	if (!strcasecmp(type, "Simple")) {
		otype = OVER_TYPE_SIMPLE;
	} else if (!strcasecmp(type, "Filelist")) {
		otype = OVER_TYPE_FILELIST;
	} else if (!strcasecmp(type, "File")) {
		otype = OVER_TYPE_FILE;
	} else if (!strcasecmp(type, "RPM")) {
		otype = OVER_TYPE_RPM;
		state = args[2];
	} else {
		add_perr(perrs, PERROR, "%s: [Parse] Overlay invalid type parameter %s", mic->name, type);
		return 0;
	}

	if (state == NULL) {
		add_perr(perrs, PERROR, "[Parse] Overlay parameter %s malformed", source);
		return 0;
	} else if (!strcasecmp(state, "on")) {
		ostate = TRUE;
	} else if (!strcasecmp(state, "off")) {
		ostate = FALSE;
	} else {
		add_perr(perrs, PERROR, "[Parse] Overlay state %s invalid", state);
		return 0;
	}

	while (dir->next != NULL) {
		dir = dir->next;
		if ((otype == dir->type) &&
		    !strcmp(source, dir->dir) &&
		    ((otype == OVER_TYPE_RPM) || !strcmp(target, dir->target))) {
			dir->state = ostate;
			dir->level = level;
			return 0;
		}
	}

	if ((dir->next = (struct moverdir *) malloc(sizeof(struct moverdir))) == NULL) {
		add_perr(perrs, PERROR, "[Parse] Failed to alloc memory for overlay");
		return 0;
	}

	dir = dir->next;
	dir->type = otype;
	dir->state = ostate;
	dir->level = level;

	if ((dir->dir = (char *) malloc(strlen(source) + 1)) == NULL) {
		add_perr(perrs, PERROR, "[Parse] Failed to alloc memory for overlay");
		return 0;
	}
	strcpy(dir->dir, source);

	if (otype != OVER_TYPE_RPM) {
		if ((dir->target = (char *) malloc(strlen(target) + 1)) == NULL) {
			free(dir->dir);
			return 0;
		}
		strcpy(dir->target, target);
	} else {
		dir->target = NULL;
	}

	dir->next = NULL;
	return 0;
}

int
mc_k1omrpms(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	    struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *dirname = args[0];

	mpssut_realloc_fill(&mic->config.filesrc.k1om_rpms, dirname);
	return 0;
}

int
mc_userauth(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	   struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	add_perr(perrs, PWARN, "[Parse] UserAuthentication: %s line %d", cfile, lineno);
	add_perr(perrs, PWRADD, "     UserAuthentication: has been deprected - ignoring");
	mic->config.user.method = (char *)NOTSET;
	return 0;
}

int
mc_setroot(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	   struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *roottype = args[0];
	char *rootpath = args[1];
	char *usrpath = args[2];

	if (!strcasecmp(roottype, "ramfs")) {
		mic->config.rootdev.type = ROOT_TYPE_RAMFS;
		if (rootpath == NULL) {
			add_perr(perrs, PERROR, "%s: [Parse] RootDevice specifies ramfs but no ramfs image", mic->name);
		} else {
			if ((mic->config.rootdev.target = (char *) malloc(strlen(rootpath) + 1)) == NULL)
				return 0;
			strcpy(mic->config.rootdev.target, rootpath);
		}
	} else if (!strcasecmp(roottype, "staticramfs")) {
		mic->config.rootdev.type = ROOT_TYPE_STATICRAMFS;
		if (rootpath == NULL) {
			add_perr(perrs, PERROR, "%s: [Parse] RootDevice specifies ramfs but no ramfs image", mic->name);
		} else {
			if ((mic->config.rootdev.target = (char *) malloc(strlen(rootpath) + 1)) == NULL)
				return 0;
			strcpy(mic->config.rootdev.target, rootpath);
		}
	} else if (!strcasecmp(roottype, "nfs")) {
		mic->config.rootdev.type = ROOT_TYPE_NFS;
		if (rootpath == NULL) {
			add_perr(perrs, PERROR, "%s: [Parse] RootDevice specifies NFS but no root export", mic->name);
		} else {
			if ((mic->config.rootdev.target = (char *) malloc(strlen(rootpath) + 1)) == NULL)
				return 0;
			strcpy(mic->config.rootdev.target, rootpath);
		}
	} else if (!strcasecmp(roottype, "splitnfs")) {
		mic->config.rootdev.type = ROOT_TYPE_SPLITNFS;
		if (rootpath == NULL) {
			add_perr(perrs, PERROR, "%s: [Parse] RootDevice specifies NFS but no NFS export", mic->name);
		} else if (usrpath == NULL) {
			add_perr(perrs, PERROR, "%s: [Parse] RootDevice specifies NFS with shared /usr %s but no /usr export",
				mic->name, usrpath);
		} else {
			if ((mic->config.rootdev.target = (char *) malloc(strlen(rootpath) + 1)) == NULL)
				return 0;
			strcpy(mic->config.rootdev.target, rootpath);

			if ((mic->config.rootdev.nfsusr = (char *) malloc(strlen(usrpath) + 1)) == NULL)
				return 0;
			strcpy(mic->config.rootdev.nfsusr, usrpath);
		}
	} else {
		add_perr(perrs, PERROR, "%s: [Parse] RootDevice unknown type '%s'", mic->name, roottype);
		mic->config.rootdev.type = NOTSET;
	}
	return 0;
}

int
mc_bootonstart(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	       struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *enable = args[0];

	if (!strcasecmp(enable, "Disabled")) {
		mic->config.boot.onstart = FALSE;
	} else if (!strcasecmp(enable, "Enabled")) {
		mic->config.boot.onstart = TRUE;
	} else {
		add_perr(perrs, PERROR, "%s: [Parse] BootOnStart unknown value '%s'", mic->name, enable);
		mic->config.boot.onstart = NOTSET;
	}
	return 0;
}

int
mc_verboselog(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	      struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *verbose = args[0];

	if (!strcasecmp(verbose, "Disabled")) {
		mic->config.boot.verbose = FALSE;
	} else if (!strcasecmp(verbose, "Enabled")) {
		mic->config.boot.verbose = TRUE;
	} else {
		add_perr(perrs, PERROR, "%s: [Parse] VerboseLogging unknown value '%s'", mic->name, verbose);
		mic->config.boot.verbose = NOTSET;
	}
	return 0;
}

int
mc_hostname(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	    struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *name = args[0];

	if ((mic->config.net.hostname = (char *) malloc(strlen(name) + 1)) == NULL) {
		return 0;
	}
	strcpy(mic->config.net.hostname, name);
	return 0;
}

int
mc_extra(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	 struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *cmdline = args[0];

	if ((mic->config.boot.extraCmdline = (char *) malloc(strlen(cmdline) + 1)) == NULL) {
		return 0;
	}
	strcpy(mic->config.boot.extraCmdline, cmdline);
	return 0;
}

int
mc_console(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	   struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *console = args[0];

	if ((mic->config.boot.console = (char *) malloc(strlen(console) + 1)) == NULL) {
		return 0;
	}
	strcpy(mic->config.boot.console, console);
	return 0;
}

int
mc_pm(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
      struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *pm = args[0];

	if ((mic->config.boot.pm = (char *) malloc(strlen(pm) + 1)) == NULL) {
		return 0;
	}
	strcpy(mic->config.boot.pm, pm);
	return 0;
}

int
mc_shutdown_to(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	       struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *timeout = args[0];

	if ((mic->config.misc.shutdowntimeout = (char *) malloc(strlen(timeout) + 1)) == NULL) {
		return 0;
	}
	strcpy(mic->config.misc.shutdowntimeout, timeout);
	return 0;
}

int
mc_crashdump(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	     struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char *dirname = args[0];
	char *limit = args[1];

	if ((dirname == NULL) || (limit == NULL))
		return 0;

	if ((mic->config.misc.crashdumpDir = (char *) malloc(strlen(dirname) + 1)) == NULL)
		return 0;
	strcpy(mic->config.misc.crashdumpDir, dirname);

	if ((mic->config.misc.crashdumplimitgb = (char *) malloc(strlen(limit) + 1)) == NULL)
		return 0;
	strcpy(mic->config.misc.crashdumplimitgb, limit);
	return 0;
}

int
mc_service(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	   struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	struct mservice *serv = &mic->config.services;
	char *name = args[0];
	char *start = args[1];
	char *stop = args[2];
	char *state = args[3];
	unsigned int up;
	unsigned int down;
	int on;

	up = atoi(start);
	if (up > 100) {
		add_perr(perrs, PERROR, "[Parse] Invalid service start %d for '%s'", up, name);
		return 0;
	}

	down = atoi(stop);
	if (down > 100) {
		add_perr(perrs, PERROR, "[Parse] Invalid service stop %d for '%s'", down, name);
		return 0;
	}

	if (!strcmp(state, "on")) {
		on = TRUE;
	} else if (!strcmp(state, "off")) {
		on = FALSE;
	} else {
		add_perr(perrs, PERROR, "[Parse] Invalid state '%s' for '%s; - must be on or off", state, name);
		return 0;
	}

	while (serv->next != NULL) {
		serv = serv->next;
		if (!strcmp(serv->name, name)) {
			serv->start = up;
			serv->stop = down;
			serv->on = on;
			return 0;
		}
	}

	if ((serv->next = (struct mservice *) malloc(sizeof(struct mservice))) == NULL)
		return 0;
	serv = serv->next;

	if ((serv->name = (char *) malloc(strlen(name) + 1)) == NULL)
		return 0;
	strcpy(serv->name, name);

	serv->start = up;
	serv->stop = down;
	serv->on = on;
	serv->next = NULL;
	return 0;
}

int
mc_cgroup(char *args[], struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
	  struct mpss_elist *perrs, int level, char *cfile, int lineno)
{
	char **arg = args;
	char *value;

	while (*arg != NULL) {
		if (!strncasecmp(*arg, "memory=", strlen("memory="))) {
			value = strchr(*arg, '=') + 1;

			if (!strcasecmp(value, "enabled")) {
				mic->config.cgroup.memory = TRUE;
			} else if (!strcasecmp(value, "disabled")) {
				mic->config.cgroup.memory = FALSE;
			} else {
				add_perr(perrs, PERROR, "%s: [Parse] cgroups memory setting '%s' invalid",
					mic->name, value);
				return 0;
			}
		} else {
			add_perr(perrs, PERROR, "%s: [Parse] Unknown cgroup setting '%s' invalid",
					mic->name, *arg);
		}

		arg++;
	}
	return 0;
}

void
mpss_clear_config(struct mconfig *config)
{
	struct moverdir *dir = config->filesrc.overlays.next;
	struct moverdir *freedir = config->filesrc.overlays.next;

	config->boot.verbose = NOTSET;
	config->boot.onstart = NOTSET;
	config->rootdev.type = NOTSET;
	config->net.type = NOTSET;
	config->net.modhost = TRUE;
	config->cgroup.memory = NOTSET;
	config->valid = NOTSET;
	config->user.method = NULL;	// Deprecated.
	config->family = MIC_FAMILY_UNKNOWN_VALUE;
	config->mpss_version = MPSS_VERSION_UNKNOWN_VALUE;

	if (config->boot.osimage) {
		free(config->boot.osimage);
		config->boot.osimage = NULL;
	}
	if (config->boot.efiimage) {
		free(config->boot.efiimage);
		config->boot.efiimage = NULL;
	}
	if (config->rootdev.target) {
		free(config->rootdev.target);
		config->rootdev.target = NULL;
	}
	if (config->rootdev.nfsusr) {
		free(config->rootdev.nfsusr);
		config->rootdev.nfsusr = NULL;
	}
	if (config->misc.shutdowntimeout) {
		free(config->misc.shutdowntimeout);
		config->misc.shutdowntimeout = NULL;
	}
	if (config->net.hostname) {
		free(config->net.hostname);
		config->net.hostname = NULL;
	}
	if (config->net.bridge) {
		free(config->net.bridge);
		config->net.bridge = NULL;
	}
	if (config->net.micIP) {
		free(config->net.micIP);
		config->net.micIP = NULL;
	}
	if (config->net.hostIP) {
		free(config->net.hostIP);
		config->net.hostIP = NULL;
	}
	if (config->net.gateway) {
		free(config->net.gateway);
		config->net.gateway = NULL;
	}
	if (config->net.micMac) {
		free(config->net.micMac);
		config->net.micMac = NULL;
	}
	if (config->net.hostMac > MIC_MAC_RANDOM) {
		free(config->net.hostMac);
	}
	config->net.hostMac = NULL;
	if (config->net.prefix) {
		free(config->net.prefix);
		config->net.prefix = NULL;
	}
	if (config->net.mtu) {
		free(config->net.mtu);
		config->net.mtu = NULL;
	}
	if (config->net.modcard) {
		free(config->net.modcard);
		config->net.modcard = NULL;
	}
	if (config->boot.extraCmdline) {
		free(config->boot.extraCmdline);
		config->boot.extraCmdline = NULL;
	}
	if (config->boot.console) {
		free(config->boot.console);
		config->boot.console = NULL;
	}
	if (config->boot.pm) {
		free(config->boot.pm);
		config->boot.pm = NULL;
	}
	if (config->filesrc.base.image) {
		free(config->filesrc.base.image);
		config->filesrc.base.image = NULL;
	}
	if (config->filesrc.base.dir) {
		free(config->filesrc.base.dir);
		config->filesrc.base.dir = NULL;
	}
	if (config->filesrc.base.list) {
		free(config->filesrc.base.list);
		config->filesrc.base.list = NULL;
	}
	if (config->filesrc.common.dir) {
		free(config->filesrc.common.dir);
		config->filesrc.common.dir = NULL;
	}
	if (config->filesrc.common.list) {
		free(config->filesrc.common.list);
		config->filesrc.common.list = NULL;
	}
	if (config->filesrc.mic.dir) {
		free(config->filesrc.mic.dir);
		config->filesrc.mic.dir = NULL;
	}
	if (config->filesrc.mic.list) {
		free(config->filesrc.mic.list);
		config->filesrc.mic.list = NULL;
	}
	if (config->filesrc.k1om_rpms) {
		free(config->filesrc.k1om_rpms);
		config->filesrc.k1om_rpms = NULL;
	}
	if (config->misc.crashdumpDir) {
		free(config->misc.crashdumpDir);
		config->misc.crashdumpDir = NULL;
	}
	if (config->misc.crashdumplimitgb) {
		free(config->misc.crashdumplimitgb);
		config->misc.crashdumplimitgb = NULL;
	}

	while (dir) {
		freedir = dir;
		dir = dir->next;
		free(freedir);
	}

	config->filesrc.overlays.next = NULL;
}

void
mpss_clear_elist(struct mpss_elist *perrs)
{
	struct melist *tmp1 = perrs->ehead;
	struct melist *tmp2;

	if (tmp1 != NULL) {
		tmp2 = tmp1;
		tmp1 = tmp1->next;
		free(tmp2);
	}

	init_perror(perrs);
}

void
init_perror(struct mpss_elist *perrs)
{
	memset(perrs->ecount, 0, sizeof(perrs->ecount));
	perrs->ehead = NULL;
	perrs->etail = NULL;
}

void
add_perr(struct mpss_elist *perrs, unsigned char level, char *format, ...)
{
	struct melist *elem;
	char buffer[4096];
	va_list args;

	if (perrs == NULL)
		return;

	if ((elem = (struct melist *) malloc(sizeof(struct melist))) == NULL)
		return;

	if (perrs->ehead == NULL) {
		perrs->ehead = elem;
		perrs->etail = elem;
	} else {
		perrs->etail->next = elem;
		perrs->etail = elem;
	}

	if (!(level & PADD))
		perrs->ecount[(int)level]++;

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	if ((elem->string = (char *) malloc(strlen(buffer) + 1)) != NULL)
		strcpy(elem->string, buffer);

	elem->level = level & ~PADD;
	elem->next = NULL;
}

void
mpss_print_elist(struct mpss_elist *perrs, unsigned char lev, void (*printfunc)(unsigned char lev, char *format, ...)) {
	struct melist *elem = perrs->ehead;;

	while (elem != NULL) {
		if (lev >= elem->level)
			printfunc(elem->level, "%s\n", elem->string);
		elem = elem->next;
	}
}

int
mpss_remove_config(struct mpss_env *menv, struct mic_info *mic, char *line)
{
	char *confname;
	char *tmpname;
	char buffer[1024];
	int skip_next_blank = 0;
	FILE *ifp;
	FILE *ofp;

	// Modify the common file instead of mic specific if no mic card specified
	if (mic == NULL)
		confname = mpssut_configname(menv, "default");
	else
		confname = mic->config.name;

	if ((ifp = fopen(confname, "r")) == NULL) {
		return EEXIST;
	}

	if ((tmpname = mpssut_tempnam(confname)) == NULL) {
		fclose(ifp);
		return ENOMEM;
	}

	if ((ofp = fopen(tmpname, "w")) == NULL) {
		fclose(ifp);
		free(tmpname);
		return EBADF;
	}

	while (fgets(buffer, 1024, ifp)) {
		if (skip_next_blank && (buffer[0] == '\n')) {
			skip_next_blank = 0;
			continue;
		}

		if (strncmp(buffer, line, strlen(line))) {
			fprintf(ofp, "%s", buffer);
			skip_next_blank = 0;
		} else {
			skip_next_blank = 1;
		}
	}
	fclose(ifp);
	fclose(ofp);

	if (mpssut_rename(menv, tmpname, confname)) {
			free(tmpname);
			return EIO;
	}

	free(tmpname);

	return 0;
}

int
mpss_update_config(struct mpss_env *menv, char *confname, char *match, char *desc, char *format, ...)
{
	char buffer[1024];
	char *oname;
	va_list args;
	FILE *icfp;
	FILE *ocfp;
	int changed = 0;

	if (match == NULL)
		match = "DONOTMATCH";

	oname = mpssut_tempnam(confname);

	if ((ocfp = fopen(oname, "w")) == NULL) {
		free(oname);
		return EBADF;
	}

	if ((icfp = fopen(confname, "r")) == NULL) {
		goto check_changed;
	}

	while (fgets(buffer, 1024, icfp)) {
		if (!strncasecmp(buffer, match, strlen(match))) {
			va_start(args, format);
			vsnprintf(buffer, 1024, format, args);
			va_end(args);
			changed = 1;
		}

		fprintf(ocfp, "%s", buffer);
	}

check_changed:
	if (!changed) {
		if (desc != NULL)
			fprintf(ocfp, "%s\n", desc);

		va_start(args, format);
		vsnprintf(buffer, 1024, format, args);
		va_end(args);
		fprintf(ocfp, "%s", buffer);
	}

	if (icfp != NULL) fclose(icfp);
	fclose(ocfp);

	if (mpssut_rename(menv, oname, confname)) {
			free(oname);
			return EIO;
	}

	free(oname);

	return 0;
}

struct tokens tokens[] = {
	{"Include", 7, 1, 1, mc_doinclude},
	{"Version", 7, 1, 2, mc_version},
	{"OSimage", 7, 0, 2, mc_osimage},
	{"EFIimage", 8, 0, 1, mc_efiimage},
	{"BootOnStart", 11, 1, 1, mc_bootonstart},
	{"VerboseLogging", 14, 1, 1, mc_verboselog},
	{"Hostname", 8, 1, 1, mc_hostname},
	{"Network", 7, 2, 7, mc_network},
	{"Bridge", 6, 2, 6, mc_bridge},
	{"MacAddrs", 8, 1, 2, mc_mac},
	{"ExtraCommandLine", 16, 1, 1, mc_extra},
	{"Console", 7, 1, 1, mc_console},
	{"PowerManagement", 15, 1, 1, mc_pm},
	{"Base", 4, 2, 2, mc_base},
	{"CommonDir", 10, 1, 2, mc_commondir},
	{"MicDir", 10, 1, 2, mc_micdir},
	{"UserAuthentication", 18, 1, 3, mc_userauth},
	{"Overlay", 10, 2, 4, mc_overlay},
	{"K1omRpms", 11, 1, 1, mc_k1omrpms},
	{"RootDevice", 10, 1, 3, mc_setroot},
	{"ShutdownTimeout", 15, 1, 1, mc_shutdown_to},
	{"CrashDump", 12, 2, 2, mc_crashdump},
	{"Service", 7, 4, 4, mc_service},
	{"Cgroup", 6, 1, 8, mc_cgroup},
	{"Family", 6, 0, 1, mc_family},
	{"MPSSVersion", 11, 0, 1, mc_mpss_version},
	{NULL, 0, 0, 0, NULL}
};

#define MAX_ARGS 10

int
parse_config_file(struct mpss_env *menv, struct mic_info *mic, struct mbridge **brlist,
		  char *filename, struct mpss_elist *perrs, int level)
{
	struct stat sbuf;
	char *file_image;
	char *config;
	char *args[MAX_ARGS];
	char *end;
	int fd;
	int size;
	int lineno = 1;
	struct tokens *tok;
	int arg;
	int err;

	if (stat(filename, &sbuf)) {
		mic->config.valid = FALSE;
		return EEXIST;
	}

	if ((fd = open(filename, O_RDONLY)) < 0) {
		add_perr(perrs, PERROR, "[Parse FATAL] %s open fail %s", filename, strerror(errno));
		return EBADF;
	}

	if ((file_image = (char *) malloc(sbuf.st_size + 1)) == NULL) {
		close(fd);
		add_perr(perrs, PERROR, "[Parse FATAL] %s malloc fail %s", filename, strerror(errno));
		return ENOMEM;
	}

	if ((size = read(fd, file_image, sbuf.st_size)) != sbuf.st_size) {
		free(file_image);
		close(fd);
		return EBADF;
	}

	add_perr(perrs, PINFO, "%s: [Parse] %s", mic->name, filename);
	mic->config.valid = TRUE;
	file_image[sbuf.st_size] = '\0';
	config = file_image;
	while (config < (file_image + size -1)) {
		memset(args, 0, sizeof(args));
		end = config;
		while ((*end != '\n') && (*end != '\0'))
			end++;
		*end++ = '\0';

		arg = 0;
		args[0] = config;
		while ((arg < (MAX_ARGS - 1))) {
			while (isspace(*args[arg]))
				args[arg]++;

			switch (*args[arg]) {
			case '#':
				if (arg == 0)
					goto next_line;

				goto done_lexing;
			case '\"':
				args[arg]++;
				if ((args[arg + 1] = strchr(args[arg], '\"')) == NULL) {
					add_perr(perrs, PWARN, "[Parse] config file %s line %d: Quoted argument "
					       "missing ending \"", filename, lineno);
					goto done_lexing;
				}

				break;

			default:
				if (strlen(args[arg]) == 0)
					goto done_lexing;

				if (((args[arg + 1] = strchr(args[arg], ' ')) == NULL) &&
				    ((args[arg + 1] = strchr(args[arg], '\t')) == NULL))
					goto done_lexing;
			}

			*args[arg + 1] = '\0';
			args[arg + 1]++;
			while(*args[arg + 1] == ' ')
				args[arg + 1]++;

			if (*args[arg + 1] == '\0') {
				args[arg + 1] = NULL;
				goto done_lexing;
			}

			arg++;
		}

done_lexing:
		if ((arg == 0) && (strlen(config) == 0))
			goto next_line;

		tok = &tokens[0];
		while (tok->name) {
			if ((strlen(config) == strlen(tok->name) && !strcasecmp(config, tok->name))) {
				if ((arg < tok->minargs) || (arg > tok->maxargs)) {
					add_perr(perrs, PERROR, "[Parse] config file %s line %d: invalid argument "
					       "count %d", filename, lineno, arg);
					goto next_line;
				}

				if ((err = tok->func(&args[1], menv, mic, brlist, perrs, level, filename, lineno)))
					return err;
				break;
			}
			tok++;
		}

		if (tok->name == NULL)
			add_perr(perrs, PERROR, "%s: [Parse] %s invalid config parameter", mic->name, config);

next_line:
		lineno++;
		config = end;
	}

	close(fd);
	free(file_image);
	return 0;
}

struct mic_info *
_get_miclist_present(struct mpss_env *menv, int *cnt)
{
	struct mic_info miclist;
	struct mic_info *mic = &miclist;
	struct mic_info *firstmic = mic, *tmp;
	int id = 0;
	struct dirent *file;
	DIR *dp;

	*cnt = 0;

	if ((dp = opendir(MICSYSFSDIR)) == NULL) {
		return NULL;
	}

	memset(mic, 0, sizeof(struct mic_info));

	while ((file = readdir(dp)) != NULL) {
		if (strncmp(file->d_name, "mic", 3))
			continue;

		if (sscanf(file->d_name, "mic%d", &id) <= 0)
			continue;

		/* list of micN devices may be out of order; find the correct spot to insert it */
		mic = firstmic;
		while (mic->id < id && mic->next && mic->next->id < id) {
			mic = mic->next;
		}

		if ((tmp = (struct mic_info *) malloc(sizeof(struct mic_info))) != NULL) {
			memset(tmp, 0, sizeof(struct mic_info));
			/* two possibilities: 
			   - First element of linked list needs a bump
			   - we need to squeeze ourselves in 
			*/
			if (mic->id > id) {
				tmp->next = mic;
				firstmic = tmp;
			} else {
				tmp->next = mic->next;
				mic->next = tmp;
			}
			mic = tmp;
			mic->id = id;
			mic->present = TRUE;
			mic->name = mpssut_alloc_fill(file->d_name);
			mic->config.name = mpssut_configname(menv, mic->name);
			mic->config.dname = mpssut_configname(menv, "default");
			mpss_clear_config(&mic->config);
			(*cnt)++;
		}
	}

	closedir(dp);
	return mic;
}

struct mic_info *
_add_miclist_not_present(struct mpss_env *menv, struct mic_info *miclist, int *cnt)
{
	struct mic_info *mic;
	struct mic_info *tmp;
	char confdir[PATH_MAX];
	char name[64];
	int id;
	struct dirent *file;
	DIR *dp;

	mpssut_filename(menv, NULL, confdir, PATH_MAX, "%s", menv->confdir);
	if ((dp = opendir(confdir)) == NULL)
		return miclist->next;

	while ((file = readdir(dp)) != NULL) {
		if (sscanf(file->d_name, "mic%d.conf", &id) <= 0)
			continue;

		snprintf(name, sizeof(name), "mic%d", id);

		mic = miclist;
		while (mic->next) {
			if (mic->next->id > id) {
				if ((tmp = (struct mic_info *) malloc(sizeof(struct mic_info))) != NULL) {
					memset(tmp, 0, sizeof(struct mic_info));
					tmp->next = mic->next;
					mic->next = tmp;
					mic = tmp;
					mic->id = id;
					mic->present = FALSE;
					mic->name = mpssut_alloc_fill(name);
					mpss_clear_config(&mic->config);
					mic->config.name = mpssut_configname(menv, mic->name);
					mic->config.dname = mpssut_configname(menv, "default");
					(*cnt)++;
				}
				break;
			} else if (mic->next->id == id) {
				break;
			}

			mic = mic->next;
		}

		if (mic->next == NULL) {
			if ((tmp = (struct mic_info *) malloc(sizeof(struct mic_info))) != NULL) {
				memset(tmp, 0, sizeof(struct mic_info));
				mic->next = tmp;
				mic = tmp;
				mic->next = NULL;
				mic->id = id;
				mic->present = FALSE;
				mic->name = mpssut_alloc_fill(name);
				mpss_clear_config(&mic->config);
				mic->config.name = mpssut_configname(menv, mic->name);
				mic->config.dname = mpssut_configname(menv, "default");
				(*cnt)++;
			}
		}
	}
	closedir(dp);

	return miclist->next;
}

struct mic_info *
mpss_get_miclist(struct mpss_env *menv, int *miccnt)
{
	struct mic_info miclist = {0};
	int cnt = -1;

	miclist.next = _get_miclist_present(menv, &cnt);
	miclist.next = _add_miclist_not_present(menv, &miclist, &cnt);

	if (miccnt != NULL)
		*miccnt = cnt;

	return miclist.next;
}

void
mpss_free_miclist(struct mic_info *miclist)
{
	struct mic_info *mic = miclist;
	struct mic_info *oldmic;

	while (mic) {
		mpss_clear_config(&mic->config);
		free(mic->config.name);
		free(mic->config.dname);
		free(mic->name);
		oldmic = mic;
		mic = mic->next;
		free(oldmic);
	}
}

struct mic_info *
mpss_find_micname_inlist(struct mic_info *miclist, char *name)
{
	struct mic_info *mic = miclist;

	while (mic != NULL) {
		if (!strcmp(mic->name, name))
			return mic;

		mic = mic->next;
	}

	return NULL;
}

struct mic_info *
mpss_find_micid_inlist(struct mic_info *miclist, int micid)
{
	struct mic_info *mic = miclist;

	while (mic != NULL) {
		if (mic->id == micid)
			return mic;

		mic = mic->next;
	}

	return NULL;
}

const char *family_to_str(int family)
{
	switch (family) {
		case MIC_FAMILY_KNC_VALUE:	return MIC_FAMILY_KNC;
		case MIC_FAMILY_KNL_VALUE:	return MIC_FAMILY_KNL;
		default:			return MIC_FAMILY_UNKNOWN;
	}
}

const char *mpss_version_to_str(int version)
{
	switch (version) {
		case MPSS_VERSION_3_VALUE:	return MPSS_VERSION_3;
		case MPSS_VERSION_4_VALUE:	return MPSS_VERSION_4;
		default:			return MPSS_VERSION_UNKNOWN;
	}
}
