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

#define COMMON_GLOBAL \
"\
Global Options:\n\
\n\
    Configuration files location:\n\
\n\
    /etc/mpss - default value\n\
    /etc/sysconfig/mpss.conf        - file contains replacement value\n\
    export MPSS_CONFIGDIR=<confdir> - environment variable\n\
    -c <confdir>                    - command line option\n\
    --configdir=<confdir>           - command line option\n\
\n\
        Select the location of the MPSS stack configuration files. For a more\n\
        complete discussion of this option refer to the documentation or\n\
        'micctrl -h'.\n\
\n\
    Destination directory:\n\
\n\
    export MPSS_DESTDIR=<destdir> - environment variable\n\
    -d <destdir>                  - command line option\n\
    --destdir=<destdir>           - command line option\n\
\n\
        Prepend the location of all files with the indicated 'destdir'\n\
        directory. For a more complete discussion of this option refer to the\n\
        documentation or 'micctrl -h'.\n\
\n"

#define HELP_SUBHEAD \
"\
Sub Options:\n\
\n"

#define HELP_SUBALL \
"\
    -h  Print this help and ignore all other options.\n\
\n\
    -v  Increment the level of verbosity of output. For more information\n\
        refer to the documentation or 'micctrl -h'.\n\
\n"

#define COMMON_HELP HELP_SUBHEAD HELP_SUBALL

#define VARDIR_HELP \
"\
     /var/mpss - default location\n\
     export MPSS_VARDIR=<vardir>\n\
    --vardir=<vardir>\n\
\n\
        By default the micctrl utility creates the predefined file system\n\
        information in the /var/mpss directory for Xeon Phi cards. The system\n\
        administrator may choose to create these files in a different location\n\
        to prevent overwriting files in a existing configuration.\n\
\n\
        The default location may be changed by either previously setting the\n\
        MPSS_VARDIR environment variable to the desired directory or using the\n\
        --vardir option.  Using the --vardir option overrides the MPSS_VARDIR\n\
        setting.\n\
\n\
        The --vardir option affects the values of the MicDir, CommonDir and\n\
        RootDev configuration parameters.\n\
\n"

#define SRCDIR_HELP \
"\
    /usr/share/mpss/boot - default location\n\
    export MPSS_SRCDIR=<srcdir>\n\
    --srcdir=<srcdir>\n\
\n\
        The micctrl utility by default finds the Linux kernel image and the\n\
        bulk of the root file system information for the Xeon Phi cards in the\n\
        /usr/share/mpss/boot directory. The system administrator may wish a\n\
        particular configuration to use alternate versions.\n\
\n\
        Alternate version of these files located my be specified by previously\n\
        setting the MPSS_SRCDIR environment variable or using the --srcdir\n\
        option  in the command line and instructs micctrl to look for these\n\
        files in the directory specified.\n\
\n\
	The --vardir option affects the values of the BaseDir and OSimage\n\
        configuration parameters\n\
\n"

#define NETDIR_HELP_ENTRY \
"\
    /usr/sysconfig/network-scripts - RedHat default location\n\
    /usr/sysconfig/network         - SuSE default location\n"

#define NETDIR_HELP_BODY \
"    --netdir=<netdir>\n\
\n\
        The micctrl utility creates host network configuration files and\n\
        instructs the host to make active network configuration match them.\n\
        When creating alternate configurations the systems administrator will\n\
        not want to interrupt the currently running system.\n\
\n\
        Using the --netdir option to set the network configuration directory\n\
        and creates the network control files in the specified directory\n\
        instead. It also disables micctrl updating the active network\n\
        configuration of the host system.\n\
\n"

#define NET_DISTHELP \
"\
    -w <distrib>\n\
    --distrib=<distrib>\n\
\n\
        The --distrib option will take the values 'redhat' or 'suse'. If it is\n\
        not specified the type of distribution of the host is used.  This option\n\
        would normally be used in conjunction with other options such as\n\
        --destdir for configurations to be pushed to other systems.\n\
\n"

