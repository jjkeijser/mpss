/*
 * Copyright (c) 2017, Intel Corporation.
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

#ifndef __MPSSCONFIG_H_
#define __MPSSCONFIG_H_

#define CURRENT_CONFIG_MAJOR 1
#define CURRENT_CONFIG_MINOR 1

#define MPSS_CONFIG_VER(x,y) (((x) << 16) + (y))
#define MPSS_CURRENT_CONFIG_VER ((CURRENT_CONFIG_MAJOR << 16) + CURRENT_CONFIG_MINOR)

/*
 * Section 1: Simple APIs for reading and writing parameters in the
 * configuration files.
 */
#define CONFIG_ERROR_ENV	1
#define CONFIG_ERROR_EXIST	2
#define CONFIG_ERROR_PARSE	3
#define CONFIG_ERROR_MALFORMED	4
#define CONFIG_ERROR_WRITE	5

/**
 * mpssconfig_read_pm Read and return the PowerManagement config values
 *     \param name	Name of mic card to change config for
 *     \param cfeq	Return cpufreq value
 *     \param corec6	Return corec6 value
 *     \param pc3	Return pc3 value
 *     \param pc6	Return pc6 value
 *
 * mpssconfig_read_pm parses the MPSS config file for the associated
 * MIC card.  The value of the PowerManagement parameter is then parsed
 * and its individual components returned int the cfreq, corec6,
 * pc3 and pc6 argument.
 *
 *\return
 * Upon successful completion a zero value is returned.  Othewise one
 * of the following errors is returned:
 *
 * \par Errors:
 *- CONFIG_ERROR_ENV
 * - Could not set the default values for MPSS stack environment
 *- CONFIG_ERROR_EXIST
 * - Specified MIC card does not exist
 *- CONFIG_ERROR_PARSE
 * - Failed to parse the configuration files
 *- CONFIG_ERROR_MALFORMED
 * - The PowerManagement parameter in the confuration file is malformed
**/
int mpssconfig_read_pm(char *name, int *cfreq, int *corec6, int *pc3, int *pc6);

/**
 * mpssconfig_update_pm Update PowerManagement config values
 *     \param name	Name of mic card to change config for
 *     \param cfeq	Return cpufreq value
 *     \param corec6	Return corec6 value
 *     \param pc3	Return pc3 value
 *     \param pc6	Return pc6 value
 *
 * mpssconfig_update_pm formats a new PowerManagment paramter value
 * and replaces the value in the configuration file with it.
 *
 *\return
 * Upon successful completion a zero value is returned.  Othewise one
 * of the following errors is returned:
 *
 * \par Errors:
 *- CONFIG_ERROR_ENV
 * - Could not set the default values for MPSS stack environment
 *- CONFIG_ERROR_EXIST
 * - Specified MIC card does not exist
 *- CONFIG_ERROR_WRITE
 * - Failed to open and write the configuration file
**/
int mpssconfig_update_pm(char *name, int cfreq, int corec6, int pc3, int pc6);


int mpss_sync_cookie(unsigned long *cookie, uid_t uid);
#define COOKIE_SUCCESS	0
#define COOKIE_FAIL_CONNECT	1
#define COOKIE_FAIL_SEND	2
#define COOKIE_FAIL_RECV	3
#define COOKIE_FAIL_READ	4
#define COOKIE_FAIL_UID 	5
/*
 * Section 2: APIs for controlling the enviroment mpss configuration is used in.
 *
 * The above APIs use default environment settings or manipulate the enviroment
 * with arguments to its functions.  The more general APIs below must call
 * mpssenv_init before being passwd the environment variable.
 */
#ifndef MPSS_CONFIG_FILE
#define MPSS_CONFIG_FILE "/etc/sysconfig/mpss.conf"
#endif

#ifndef MICSYSFSDIR
#define MICSYSFSDIR "/sys/class/mic"
#endif

#ifndef DEFAULT_CONFDIR
#define DEFAULT_CONFDIR "/etc/mpss"
#endif

#ifndef DEFAULT_VARDIR
#define DEFAULT_VARDIR "/var/mpss"
#endif

#ifndef DEFAULT_SRCDIR
#define DEFAULT_SRCDIR "/usr/share/mpss/boot"
#endif

#ifndef DEFAULT_BZIMAGE
#define DEFAULT_BZIMAGE	"bzImage-knightscorner"
#endif

#ifndef DEFAULT_SYSMAP
#define DEFAULT_SYSMAP	"System.map-knightscorner"
#endif

#ifndef DEFAULT_INITRD
#define DEFAULT_INITRD	"initramfs-knightscorner.cpio.gz"
#endif


/*
 * Return values from mpssenv_init, mpssenv_set_distrib, mpss_set_configdir,
 * mpss_set_vardir and mpss_set_destdir.
 */
#define SETENV_SUCCESS		0
#define SETENV_CMDLINE_EXIST	1
#define SETENV_CMDLINE_ISDIR	2
#define SETENV_ENV_EXIST	3
#define SETENV_ENV_ISDIR	4
#define SETENV_CONFFILE_ACCESS	5
#define SETENV_CONFFILE_FORMAT	6
#define SETENV_CONFFILE_CONTENT	7
#define SETENV_CONFFILE_EXIST	8
#define SETENV_CONFFILE_ISDIR	9
#define SETENV_CMDLINE_DIST	10
#define SETENV_ENV_DIST		11
#define SETENV_PROBE_DIST	12

#define SETENV_FUNC_DIST	1
#define SETENV_FUNC_CONFDIR	2
#define SETENV_FUNC_DESTDIR	3
#define SETENV_FUNC_VARDIR	4
#define SETENV_FUNC_SRCDIR	5

#define REDHAT_NETWORK_DIR "/etc/sysconfig/network-scripts"
#define SUSE_NETWORK_DIR "/etc/sysconfig/network"
#define UBUNTU_NETWORK_DIR "/etc/network"

struct mpss_env {
	int	dist;
#define DISTRIB_UNKNOWN	0
#define DISTRIB_REDHAT	1
#define DISTRIB_SUSE	2
#define DISTRIB_UBUNTU	3
	char	*home;
	char	*confdir;
	char	*destdir;
	char	*vardir;
	char	*srcdir;
	int	live_update;
	char	*lockfile;
#define LSB_LOCK_FILENAME "/var/lock/mpss"
#define UBUNTU_LOCK_FILENAME "/var/lock/mpss"
};

/**
 * mpssenv_init - Set the default environment values.
 *     \param menv	Initialized MIC environment
 *
 * mpssenv_init initializes the evironment data structure mpss_env used
 * by internal parsing routines for MPSS the MPSS configuration files
 * and process.  It calls mpssenv_set_distrib, mpss_set_configdir,
 * mpss_set_vardir and mpss_set_destdir with their second arguments set
 * to NULL.  If any one of them fails then its error code will be returned.
 *
 *\return
 * Upon successful completion of all subfunction calls SETENV_SUCCESS
 * is returned.  Otherwise the error code from the sub function will be
 * returned.
*/
int mpssenv_init(struct mpss_env *menv);

/**
 * mpssenv_set_distrib - Set the distribution type.
 *
 * mpssenv_set_distrib sets the 'dist' element of eviroment data
 * structure mpss_env.  This variable is used to determine the format
 * of networking and other files generated for the host system.
 *
 * Explictily setting this value also has the consequence of indication to
 * functions, such as those used for network configruation modicfication,
 * to not indication to the host to make real time changes to its
 * configuration.
 *
 * It will get the value from one of three different paths.
 *
 * If the 'dist' argument is not NULL, it will use its value if it is from
 * the strings 'redhat', 'suse' or 'ubuntu'.
 *
 * If the dist argument is NULL it will check for the environment variable
 * MPSS_DIST with the same list of values.
 *
 * If neither are set, it will attempt to set 'dist' based on the system
 * it is running on.
 *
 *\return
 * Upon successful completion of all subfunction calls SETENV_SUCCESS
 * is returned.  Otherwise the error code from the sub function will be
 * returned.
*/
int mpssenv_set_distrib(struct mpss_env *menv, char *dist);

/**
 * mpssenv_set_configdir - Set the configuration files location.
 *     \param menv	Initialized MIC environment
 *     \param cdir	Directory name
 *
 * mpssenv_set_configdir sets the location of the directory to find
 * the configuration files for MIC cards.
 *
 * If the 'cdir' argument is not NULL, it will use its value as the
 * directory to find the configuration files.
 *
 * If the 'cdir' argument is NULL it will check for the environment variable
 * MPSS_CONFIGDIR the configuration files directory.
 *
 * If neither are set, it will look for the file /etc/sysconfig/mpss.conf
 * to read the value from.  The file will look for the string
 * 'MPSS_CONFIGDIR=<location> and user the indicated location.
 *
 * Otherwise if all three are not found it will use configurations files
 * base on the default directory defined by MPSS_CONFIGDIR (/etc/mpss).
 *
 *\return
 * Upon successful completion SETENV_SUCCESS is returned.  Otherwise one
 * of the following errors is returned:
 *
 * \par Errors:
 *- SETENV_CMDLINE_EXIST
 * - The value specified by cdir does not exist
 *- SETENV_CMDLINE_ISDIR
 * - The value specified by cdir is not a directory
 *- SETENV_ENV_EXIST
 * - The value specified by MPSS_CONFIGDIR does not exist
 *- SETENV_ENV_ISDIR
 * - The value specified by MPSS_CONFIGDIR is not a directory
 *- SETENV_CONFFILE_ACCESS
 * - Cannot open the configuration file
 *- SETENV_CONFFILE_FORMAT
 * - Malformed configuraiton file
 *- SETENV_CONFFILE_CONTENT
 * - Cannot find the MPSS_CONFIGDIR string in the configuration file
 *- SETENV_CONFFILE_EXIST
 * - The value specified by the configuration file does not exist
 *- SETENV_CONFFILE_ISDIR
 * - The value specified by the configuration file is not a directory
*/
int mpssenv_set_configdir(struct mpss_env *menv, char *cdir);

/**
 * mpssenv_set_vardir - Set the file generation directory location.
 *     \param menv	Initialized MIC environment
 *     \param vardir	Directory name
 *
 * mpssenv_set_vardir sets the location of the directory to generate
 * file systems for the MIC cards, initial ram disk images, and
 * other temporary files into.
 *
 * If the 'vardir' argument is not NULL, it will use its value.
 *
 * If the 'vardir' argument is NULL it will check for the environment variable
 * MPSS_CONFIGDIR for the location of generated files.
 *
 * Otherwise in neither are defined it will use the value defined
 * by DEFAULT_VARDIR (/var/mpss).
 *
 *\return
 * Upon successful completion SETENV_SUCCESS is returned.  Otherwise one
 * of the following errors is returned:
 *
 * \par Errors:
 *- SETENV_CMDLINE_EXIST
 * - The value specified by vardir does not exist
 *- SETENV_CMDLINE_ISDIR
 * - The value specified by vardir is not a directory
 *- SETENV_ENV_EXIST
 * - The value specified by MPSS_VARDIR does not exist
 *- SETENV_ENV_ISDIR
 * - The value specified by MPSS_VARDIR is not a directory
 */
int mpssenv_set_vardir(struct mpss_env *menv, char *vardir);

/**
 * mpssenv_set_destdir - Set the relocation directory.
 *     \param menv	Initialized MIC environment
 *     \param destdir	Directory name
 *
 * mpssenv_set_destdir sets a directory name to prepend to all filename
 * to be generated.  This allows configuration to be generated completely
 * not affecting the currently running system.  It works much like using
 * DESTDIR in a GNU compilation environment.
 *
 * If the 'destdir' argument is not NULL, it will use its value.
 *
 * If the 'destdir' argument is NULL it will check for the environment variable
 * MPSS_DESTDIR for the location.
 *
 * Otherwise in neither are defined it will use the value is set to NULL.
 *
 *\return
 * Upon successful completion SETENV_SUCCESS is returned.  Otherwise one
 * of the following errors is returned:
 *
 * \par Errors:
 *- SETENV_CMDLINE_EXIST
 * - The value specified by destdir does not exist
 *- SETENV_CMDLINE_ISDIR
 * - The value specified by destdir is not a directory
 *- SETENV_ENV_EXIST
 * - The value specified by MPSS_DESTDIR does not exist
 *- SETENV_ENV_ISDIR
 * - The value specified by MPSS_DESTDIR is not a directory
 */
int mpssenv_set_destdir(struct mpss_env *menv, char *destdir);

/**
 * mpssenv_set_srcdir - Set the relocation directory.
 *     \param menv	Initialized MIC environment
 *     \param srcdir	Directory name
 *
 * mpssenv_set_srcdir sets a directory name to file the basic files
 * normally installed by the MPSS installation includeing the MIC card
 * kernel bzImage file and its system map file along with the default
 * initial ram disk file.
 *
 * If the 'srcdir' argument is not NULL, it will use its value.
 *
 * If the 'srcdir' argument is NULL it will check for the environment variable
 * MPSS_SRCDIR for the location.
 *
 * Otherwise in neither are defined it will use the value is set to NULL.
 *
 *\return
 * Upon successful completion SETENV_SUCCESS is returned.  Otherwise one
 * of the following errors is returned:
 *
 * \par Errors:
 *- SETENV_CMDLINE_EXIST
 * - The value specified by srcdir does not exist
 *- SETENV_CMDLINE_ISDIR
 * - The value specified by srcdir is not a directory
 *- SETENV_ENV_EXIST
 * - The value specified by MPSS_SRCDIR does not exist
 *- SETENV_ENV_ISDIR
 * - The value specified by MPSS_SRCDIR is not a directory
 */
int mpssenv_set_srcdir(struct mpss_env *menv, char *srcdir);

/**
 * mpssenv_errtype - returns an text string for catagory of the error
 *     \param err	Error value
 *
 * mpssenv_errtype returns an english string associated the catagory
 * of error returned by mpssenv_init, mpssenv_set_distrib,
 * mpss_set_configdir, mpss_set_vardir and mpss_set_destdir.  This helps
 * the generic return code from mpssenv_init indicate which of its sub
 * functions had an error.
 */
char *mpssenv_errtype(int err);

/**
 * mpssenv_errstr - returns an text string for the indicated error
 *     \param err	Error value
 *
 * mpssenv_errstr returns an english string associated with one of the
 * errors that can be returned by mpssenv_init, mpssenv_set_distrib,
 * mpss_set_configdir, mpss_set_vardir and mpss_set_destdir.
 */
char *mpssenv_errstr(int err);


/*
 * Section 3: APIs for finding MIC cards.
 */
/**
 * mpss_get_miclist - retrieve a linked list of known MIC instances
 *     \param menv	Initialized MIC environment
 *     \param mic_cnt	Returns the number of MIC configurations found
 *
 * mpss_get_miclist() searches both the /sys/class/mic directory and
 * the configuration files for instances of the MIC card configuration.
 * If found in both locations it merges the MIC card entries and returns a
 * linked list of mic_info structures.  Each structure will have an entry for
 * a MIC card with its name and ID Filled out and config structure initialized.
 *
 *\return
 * Upon successful completion mpss_get_miclist returns a pointer to the
 * first element in the chain.  If no MIC cards are found it returns a NULL
 * pointer.
**/
struct mic_info *mpss_get_miclist(struct mpss_env *menv, int *mic_cnt);

/**
 * mpss_free_miclist - free all the memory associated with the MIC list
 *     \param miclist	List to MIC structures to free
**/
void mpss_free_miclist(struct mic_info *miclist);

/**
 * mpss_find_micname_inlist - Find the correct MIC entry if the ID.s
 *     \param miclist	List to MIC structures to search in
 *     \param name	Name of MIC card to find
 *
 * mpss_findmic_namelist() searches the linked list of MIC cards for the
 * specified ID.
**/
struct mic_info *mpss_find_micname_inlist(struct mic_info *miclist, char *name);
/**
 * mpss_find_micid_inlist - Find the correct MIC entry if the ID.s
 *     \param miclist	List to MIC structures to search in
 *     \param micid	ID of MIC card to find
 *
 * mpss_findmic_inlist() searches the linked list of MIC cards for the
 * specified ID.
**/
struct mic_info *mpss_find_micid_inlist(struct mic_info *miclist, int micid);

/*
 * Section 4: APIs for calling the parser for the configuration files.
 */
/*
 * Parsing and probing functions return fatal errors immediately but also
 * return a list of various non fatal errors and their levels.
 */
struct melist {
	int		level;
#define PADD	0x80		// Ored in to indicate error message exteneded
#define PERROR	0
#define PERADD	PADD
#define PWARN	1
#define PWRADD	(PWARN | PADD)
#define PNORM	2
#define PINFO	3
	char		*string;
	struct melist	*next;
};

struct mpss_elist {
	int		ecount[4];
	struct melist	*ehead;
	struct melist	*etail;
};

/**
 * mpss_clear_elist - clear the indicated error list
 *
 * mpss_clear_elist clears out and frees all memory associatted with the
 * error list.
**/
void mpss_clear_elist(struct mpss_elist *perrs);

/**
 * mpss_print_elist - Display the error list
 *     \param perrs	List of errors
 *     \param level	Max error level to print
 *     \param prfunc	Function to print the error messages
 *
 * mpss_print_elist displays the error list using the indicated function.
**/
void mpss_print_elist(struct mpss_elist *perrs, unsigned char level,
		      void (*prfunc)(unsigned char level, char *format, ...));

/*
 * Data structure containing parsed configuration data
 */
struct mroot {		// RootDev parameter
	int	type;
#define ROOT_TYPE_RAMFS		0
#define ROOT_TYPE_STATICRAMFS	1
#define ROOT_TYPE_NFS		2
#define ROOT_TYPE_SPLITNFS	3
	char	*target;
	char	*nfsusr;
};

struct mnet {		// Bridge, NetWork, MacAddr
	int	type;
#define NETWORK_TYPE_STATPAIR	0
#define NETWORK_TYPE_STATBRIDGE	1
#define NETWORK_TYPE_BRIDGE	2
	char	*hostname;
	char	*bridge;
	char	*gateway;
	char	*micIP;
	char	*hostIP;
	char	*micMac;
	char	*hostMac;
#define MIC_MAC_SERIAL	((char *)0x1)
#define MIC_MAC_RANDOM	((char *)0x2)
	char	*prefix;
	char	*mtu;
	int	modhost;
	int	modcard;
};

struct mpersist {	// Used internally for micctrl for saving MAC info
	char	*micMac;
	char	*hostMac;
};

struct msource {	// Base, CommonDir, MicDir
	int	type;
#define SRCTYPE_UNKNOWN		0
#define SRCTYPE_CPIO		1
#define SRCTYPE_DIR		2
#define SRCTYPE_FILELIST	3
	char 	*image;
	char 	*dir;
	char	*list;
};

struct moverdir {	// Overlay
	int		type;
#define OVER_TYPE_SIMPLE	0
#define OVER_TYPE_FILELIST	1
#define OVER_TYPE_FILE		2
#define OVER_TYPE_RPM		3
	int		state;
	int		level;
	char		*dir;
	char		*target;
	struct moverdir	*next;
};

struct mfiles {		// Congregate two above
	struct msource	base;
	struct msource	common;
	struct msource	mic;
	struct moverdir	overlays;
	char		*k1om_rpms;
};

struct mservice {	// Service parameter
	char *name;
	unsigned int	start;
	unsigned int	stop;
	int		on;
	struct mservice *next;
};

struct mboot {		// BootOnStart, OsImage, misc. etc.
	int	onstart;
	char	*osimage;
	char	*systemmap;
	int	verbose;
	char	*extraCmdline;
	char	*console;
	char	*pm;
};

struct muser {		// Depricated no longer used
	char	*method;
	int	low;
	int	high;
};

struct mmisc {
	char	*shutdowntimeout;
#define DEF_SHUTDOWN_TIMEOUT	(300)
	char	*crashdumpDir;
#define DEF_CRASHDUMP_DIR	"/var/crash/mic"
	char	*crashdumplimitgb;
#define DEF_CRASHDUMP_LIMITGB	(16)
};

struct mcgroup {	// Cgroup parameter
	int	memory;
};

struct mconfig { 	// Contains all config data produced by parser
	char		*name;
	char		*dname;
	int		version;
	int		valid;
	struct mmisc	misc;
	struct mboot	boot;
	struct mroot	rootdev;
	struct mfiles	filesrc;
	struct mnet	net;
	struct mservice services;
	struct muser	user;
	struct mcgroup	cgroup;
	struct mpersist	persist;
};

/*
 * Top level data structure for configuration data.
 */
struct mic_info {
	int		id;		// First element - do not move
	char		*name;
	int		present;
	struct mconfig	config;
	void		*data;		// Void pointer for apps attaching data
	struct mic_info *next;
};

/*
 * Bridge configuration chain is seperate from mic_info because they
 * are shared resources.
 */
struct mbridge {
	char		*name;
	unsigned int	type;
#define BRIDGE_TYPE_UNKNOWN	0
#define BRIDGE_TYPE_INT		1
#define BRIDGE_TYPE_EXT		2
#define BRIDGE_TYPE_STATICEXT	3
	char		*ip;
	char		*prefix;
	char 		*mtu;
	struct mbridge	*next;
};

/**
 * mpss_parse_config - parse the MPSS config files and fill in config struct
 *     \param menv	Initialized MIC environment
 *     \param mic	Pointer to MIC card info
 *     \param brlist	List to place bridge structs into
 *     \param perrs	List to return error infomation
 *
 * mpss_parse_config() parses the MPSS configuration files for the specified
 * MIC card passed in and fills in it configuration data.
 *
 *\return
 * Upon successful completion mpss_parse_config returns 0.
 *\par Errors:
 *- ENOENT
 * -If the configuration file for the specified mic card could not be found
**/
int mpss_parse_config(struct mpss_env *menv, struct mic_info *mic,
		      struct mbridge **brlist, struct mpss_elist *perrs);

/**
 * mpss_clear_config - clear the MPSS config data structure
 *     \param config		Data to be cleated
 *
 * mpss_clear_config() frees any pointers to data in the structure
 * and sets them to NULL.  Any non pointer data is set to a default value.
**/
void mpss_clear_config(struct mconfig *config);

/**
 * mpss_insert_bridge - add a bridge to the linked list of bridges
 *     \param name	Name of bridge to be added (br0, ...)
 *     \param type	Type of bridge to be added (see struct mbridge)
 *     \param ip	Ip address
 *     \param netbits	Net prefix/netbits to create netmask
 *     \param mtu	MTU of bridge
 *     \param brlist	List to be added to
 *     \param perrs	List to return error infomation
 *
 * mpss_insert_bridge() adds a bridge to the list of know bridges in
 * "brlist".  It takes all the parameters needed to setup the bridge
 * interface.  If the bridge in empty brlist should point to NULL.

 *\return
 * Upon successful completion mpss_insert_bridge returns 0.
 *\par Errors:
 *- EEXIST
 * - Bridge is already in the list
*/
struct mbridge *mpss_insert_bridge(char *name, int type, char *ip,
				   char *netbits, char *mtu,
				   struct mbridge **brlist,
				   struct mpss_elist *perrs);

/**
 * mpss_free_bridgelist - clear the bridge list
 *     \param brlist		Linked list of bridges
 *
 * mpss_free_bridgelist() frees all the malloc memory associated with
 * the bridge list and sets it back to a NULL pointer.
*/
void mpss_free_bridgelist(struct mbridge **brlist);

/**
 * mpss_bridge_byname - find bridge in the list of bridges
 *     \param name	Name of bridge to be found (br0, ...)
 *     \param brlist	Linkes list of bridges
 *
 * mpss_bridge_byname() traverses the list of bridges for the
 * interface <name>.
 *
 *\return
 * Upon successful completion mpss_bridge_byname returns the pointer
 * to its mbridge data structure.  If the name is not found it returns
 * NULL.
*/
struct mbridge *mpss_bridge_byname(char *name, struct mbridge *brlist);

/**
 * mpss_bridge_byip - find bridge in the list of bridges
 *     \param name	Name of bridge to be found (br0, ...)
 *     \param brlist	Linkes list of bridges
 *
 * mpss_bridge_byip() traverses the list of bridges for the IP
 * address <ip>.
 *
 *\return
 * Upon successful completion mpss_bridge_byip returns the pointer
 * to its mbridge data structure.  If the ip is not found it returns
 * NULL.
*/
struct mbridge *mpss_bridge_byip(char *ip, struct mbridge *brlist);

/**
 * mpss_update_config - Replace the parameter with its new values
 *     \param menv	Initialized MIC environment
 *     \param confname	Name of configuraiton file to modify
 *     \param match	Configuraiton parameter sting to update
 *     \param descr	Comment block to add if required
 *     \param format	printf style format
 *
 * mpss_update_config() Replaces the current parameter value in the config
 * files with the new format defined by format and it following values.
 *
 *\return
 * Upon successful completion mpss_remove_config returns 0.
 *\par Errors:
 *- EBADF
 * -If the configuration file for the specified mic card could not be found
*/
int
mpss_update_config(struct mpss_env *menv, char *confname, char *match, char *desc, char *format, ...) __attribute__((format(printf, 5, 6)));

/**
 * mpss_remove_config - delete a config entry from the confiuration file
 *     \param menv	Initialized MIC environment
 *     \param mic	Pointer to MIC card info
 *     \param line	String of characters to patern match on.
 *
 * mpss_remove_config() searches the file DEFAULT_CONFDIR/<name>.conf for
 * configuration lines starting with the string <line> for a maximum length
 * defined by <len>.  If it finds one it deletes it from the file.
 *
 *\return
 * Upon successful completion mpss_remove_config returns 0.
 *\par Errors:
 *- ENOENT
 * -If the configuration file for the specified mic card could not be found
*/
int mpss_remove_config(struct mpss_env *menv, struct mic_info *mic, char *line);

/**
 * mpss_opensysfs - open the sysfs 'entry' under mic card 'name'
 *     \param name	Name of the mic card (i.e. mic0, mic1, ...)
 *     \param entry	Particular sysfs entry under 'name' to open
 *
 * mpss_opensysfs() opens the sysfs entry under /sys/class/mic/<name>/<entry>
 * and returns the file discriptor.
 *
 *\return
 * Upon successful completion an open file descriptor is returned.  If a
 * failure occurs it will return a -1 and errno values will correspond to
 * documentation for open(2).
*/
int mpss_opensysfs(char *name, char *entry);

/**
 * mpss_readsysfs - read the sysfs 'entry' under mic card 'name'
 *     \param name	Name of the mic card (i.e. mic0, mic1, ...)
 *     \param entry	Particular sysfs entry under 'name' to open
 *
 * mpss_readsysfs() reads the output from /sys/class/mic/<name>/<entry>
 * and returns a malloced string including it.  After use is finished the
 * caller should free() this address.
 *
 *\return
 * Upon successful completion an address containg the string of information.
 * If the data could not be read then NULL will be returned.
*/
char *mpss_readsysfs(char *name, char *entry);

/**
 * mpss_setsysfs - open the sysfs 'entry' under mic card 'name'
 *     \param name	Name of the mic card (i.e. mic0, mic1, ...)
 *     \param entry	Particular sysfs entry under 'name' to open
 *     \param value	String to write
 *
 * mpss_setsysfs() opens the sysfs entry under /sys/class/mic/<name>/<entry>
 * and attempts to write the string in 'value' to it.
 *
 *\return
 * Upon successful completion an open file descriptor is returned.  If a
 * failure occurs it will return zero.  If a negative value is returned then
 * the sysfs entry failed to open.  A positive return has the value of errno
 * returned from the write to the entry.
*/
int mpss_setsysfs(char *name, char *entry, char *value);

/**
 * mpss_set_cmdline - create and write the kernel command line
 *     \param mic	mic_info data structre with valid config data
 *     \param brlist	List of known bridged for NFS root
 *     \param cmdline	Space to return command line being set
 *
 * mpss_set_cmdline() uses the parsed configuartion data in the mic_info
 * structure passed in to create the kernel command line for the MIC
 * card.  It then writes it to the cards "cmdline" sysfs entry.
 *
 * If the cmdline argument is not NULL, the created cmdline will be copied
 * to this location.
 *
 *\return
 * Upon successful completion a zero value is returned.  Non zero value
 * indicates failure.
*/
char *mpss_set_cmdline(struct mic_info *mic, struct mbridge *brlist,
		       char *cmdline);

#endif /* __MPSSCONFIG_H_ */
