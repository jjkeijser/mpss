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

#ifndef __LIBMPSSCOMMON_H_
#define __LIBMPSSCOMMON_H_

#define TRUE	1
#define FALSE	0
#define NOTSET	-1

// Defines used in the micctrl protocol to the card's mpssd daemon.
// TODO: Should be moved to a common include file at some time.
#define MONITOR_START		1
#define MONITOR_START_ACK	2
#define MONITOR_START_NACK	3
#define REQ_CREDENTIAL		4
#define REQ_CREDENTIAL_ACK	5
#define REQ_CREDENTIAL_NACK	6
#define MONITOR_STOPPING	7
#define MICCTRL_ADDUSER		8
#define MICCTRL_AU_CONTINUE	9
#define MICCTRL_AU_NOHOME	10
#define MICCTRL_AU_FILE		11
#define MICCTRL_AU_DONE		12
#define MICCTRL_AU_ACK		13
#define MICCTRL_AU_NAK_NAME	14
#define MICCTRL_AU_NAK_UID	15
#define MICCTRL_AU_NAK_PROTO	16
#define MICCTRL_DELUSER		17
#define MICCTRL_DU_ACK		18
#define MICCTRL_DU_NACK		19
#define MICCTRL_COOKIE_NACK	20
#define MICCTRL_ADDGROUP	21
#define MICCTRL_AG_ACK		22
#define MICCTRL_AG_NACK_NAME	23
#define MICCTRL_AG_NACK_GID	24
#define MICCTRL_DELGROUP	25
#define MICCTRL_DG_ACK		26
#define MICCTRL_DG_NACK		27
#define MICCTRL_CHANGEPW	28
#define MICCTRL_PW_ACK		29
#define MICCTRL_PW_NACK		30
#define MICCTRL_SYSLOG_FILE	31
#define MICCTRL_SYSLOG_RESET	32
#define MICCTRL_SL_ACK		33
#define MICCTRL_SL_NACK		34

#define CRED_SUCCESS		0
#define CRED_FAIL_UNKNOWNUID	1
#define CRED_FAIL_READCOOKIE	2
#define CRED_FAIL_MALLOC	3

// From utilfuncs.c - Used for setting and updating strings in the configuration structure
char *mpssut_alloc_fill(char *string);
int mpssut_realloc_fill(char **addr, char *new_addr);

// From utilfuncs.c - Builds file name path based on mpss environment structure
int mpssut_filename(struct mpss_env *menv, struct stat *sbuf, char *buffer, int maxlen, char *format, ...) __attribute__((format(printf, 5, 6)));
char *mpssut_configname(struct mpss_env *menv, char *name);

// From genfs.c - called by mpssd and micctrl -b to generate file systems based on config parameters
// These will probably be moved to mpssconfig.h at some time.
int mpssfs_gen_initrd(struct mpss_env *menv, struct mic_info *mic, struct mpss_elist *perrs);
int mpssfs_gen_nfsdir(struct mpss_env *menv, struct mic_info *mic, int createusr, struct mpss_elist *perrs);
int mpssfs_unzip_base_cpio(char *name, char *zfile, char *ofile, struct mpss_elist *perrs);
int mpssfs_cpioin(char *cfile, char *cdir, int dousr);
int mpssfs_cpioout(char *cfile, char *cdir);

// From netutils.c - Routines to help with networking functions
void mpssnet_genmask(char *prefix, char* netMask);
int mpssnet_decode_ipaddr(char *caddr, int *ipaddr, int num);
int mpssnet_set_ipaddrs(struct mic_info *mic, char *micip0, char *hostip0);
char *mpssnet_brtypes(unsigned int type);

// From utilsfuncs.c - Create and remove directory trees based on mpss environment structure
int mpssut_mkdir(struct mpss_env *menv, char *dir, uid_t uid, gid_t gid, int mode);
int mpssut_mksubtree(struct mpss_env *menv, char *base, char *dir, uid_t uid, gid_t gid, int mode);
int mpssut_rmsubtree(struct mpss_env *menv, char *base, char *dir);
int mpssut_mktree(struct mpss_env *menv, char *dir, uid_t uid, gid_t gid, int mode);
int mpssut_deltree(struct mpss_env *menv, char *dir);
int mpssut_copytree(struct mpss_env *menv, char *to, char *from);
int mpssut_copyfile(char *to, char *from, struct stat *sbuf);
char *mpssut_tilde(struct mpss_env *menv, char *filename);
char *mpssut_tempnam(char *name);
int mpssut_alloc_and_load(char *filename, char **buf);
int mpssut_rename(struct mpss_env *menv, char *ifilename, char *ofilename);

void display_ldap(struct mic_info *mic, int biggap);
void display_nis(struct mic_info *mic, int biggap);

// From mpssconfig.c - Error string routines used by the parser
void init_perror(struct mpss_elist *perrs);
void add_perr(struct mpss_elist *perrs, unsigned char level, char *format, ...) __attribute__((format(printf, 3, 4)));

// From micenv.c - Get the lockfile used between mpssd and micctrl
// This will probably be moved to mpssconfig.h at some time.
int mpssenv_aquire_lockfile(struct mpss_env *menv);

// From verify_bzimage.c - ensure the Linux kernel boot image is of the correct type.
int verify_bzImage(struct mpss_env *menv, char *name, char *tag);
#define VBZI_SUCCESS	 0
#define VBZI_ACCESS	 1
#define VBZI_FAIL_FORMAT 2
#define VBZI_FAIL_TYPE	 3
#define VBZI_FAIL_OTHER	 4

#endif /*__LIBMPSSCOMMON_H_ */
