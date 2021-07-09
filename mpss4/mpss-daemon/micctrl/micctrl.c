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

#include <sys/types.h>
#include <sys/stat.h>

#include "help.h"
#include "micctrl.h"
#include <mic.h>

#include <vector>
#include <string>

struct mpss_elist mpssperr;

const char *main_help =
"\
micctrl main command [sub options] [list of coprocessors]\n\
\n\
The micctrl utility is a multi-purpose tool that assists the system administrator\n\
in configuring and controlling each or all coprocessors in the system.\n\
\n\
Each micctrl command must specify a main command to indicate the particular\n\
micctrl functionality. Most main commands accept a number of sub options. Not\n\
specifying the main command results in an error.\n\
\n\
The list of coprocessors is optional and always consists of coprocessors' identifiers\n\
in the micID format, where ID is an instance number of each coprocessor (for example:\n\
mic0, mic1, etc.). If the list is empty the command will be executed for all coprocessors\n\
in the system.\n\
\n\
Sub options:\n\
\n\
Each main command has its unique set of sub options. Use the -h flag to view them.\n\
For example:\n\
\n\
	micctrl --initdefaults -h\n\
\n\
    Common sub options can be used with any main command:\n\
\n\
        -h           Ignore all other sub options and print help for specified\n\
                     main command.\n\
\n\
        -v           Increment the level of verbosity. By default micctrl\n\
                     displays information about errors and warnings.\n\
                     Adding the -v flag causes additional messages\n\
                     to be displayed.\n\
\n\
Main commands:\n\
\n\
The micctrl tool provides functionalities specified by main commands.  Currently,\n\
the following commands are supported:\n\
\n\
    State modification commands:\n\
\n\
        The state modification commands apply only to coprocessors\n\
        detected by the kernel modules.\n\
\n\
        -b\n\
        --boot      Boot all or selected coprocessors if they are currently in the\n\
                    'ready' or 'shutdown' state. Their status is changed to 'online'.\n\
\n\
        -r\n\
        --reset     Reset all or selected coprocessors. Their status is changed to 'ready'.\n\
\n\
        -S\n\
        --shutdown  Shut down all or selected coprocessors. Their status is changed\n\
                    to 'shutdown'.\n\
\n\
        -R\n\
        --reboot    Reboot all or selected coprocessors that are in the\n\
                    'online' or 'error' state. Their status is changed to 'online'.\n\
\n\
    Other state commands:\n\
\n\
        Like the state modification commands they apply only to coprocessors\n\
        detected by the kernel modules.\n\
\n\
        -s\n\
        --status    Display the status (state) of each or selected coprocessors.\n\
\n\
        -w\n\
        --wait      Wait until coprocessors are in the stable state.\n\
\n\
    Miscellaneous other commands:\n\
\n\
        -l\n\
        --log                  Print kernel log of all or selected coprocessors.\n\
\n\
        --config               Display the current configuration information in human readable format.\n\
\n\
        Below commands do not accept the list of coprocessors.\n\
\n\
        --initdefaults         Initialize the defaults in the coprocessors’ configuration\n\
                               files and file system images. Create configuration files\n\
                               and file system images if they do not exist.\n\
\n\
        --load-modules         Load kernel modules required by coprocessors.\n\
\n\
        --unload-modules       Unload kernel modules required by coprocessors.\n\
\n\
        --help                 Display the help message.\n\
\n\
";

const char *short_help =
"\
usage: micctrl <command> [<arguments>]\n\
\n\
The most commonly used options are:\n\
    --initdefaults    Initialize the defaults configuration files.\n\
    --status          Display the state of the Xeon Phi cards.\n\
    --boot            Boot the Xeon Phi cards if they are in the 'ready' state.\n\
    --shutdown        Shutdown the Xeon Phi cards in the 'online' state.\n\
    --reboot          Reboot the Xeon Phi card.\n\
\n\
See 'micctrl --help' to read about all available commands.\n\
See 'micctrl <command> --help' to read about specific command.\n\
";

const char *loadmodules_help =
"\
micctrl --load-modules\n\
\n\
The --load-modules command loads kernel modules required by coprocessors.\n\
\n"
COMMON_HELP;

const char *unloadmodules_help =
"\
micctrl --unload-modules\n\
\n\
The --unload-modules command unloads kernel modules required by coprocessors.\n\
\n"
COMMON_HELP;

/*
 *
 * Main parses the first argument of the command line.  Micctrl always takes at least
 * one argument to define the particular functionality to perform.  The rest of the
 * command line is then passed to a sub parser function to determine if the rest of
 * the command is correct.
 */
int
main(int argc, char *argv[], char *envp[])
{
	mic_set_verbose_mode(MIC_VERBOSE_NORMAL | MIC_VERBOSE_MIC_NAME);

	try {
		umask(022);

		po::options_description global(main_help);
		global.add_options()
				("verbose,v", "")
				("boot,b", "")
				("reset,r", "")
				("shutdown,S", "")
				("reboot,R", "")
				("status,s", "")
				("wait,w", "")
				("log,l", "")
				("initdefaults", "")
				("config", "")
				("load-modules", "")
				("unload-modules", "")
				("help,h", "")
				("force,f", "")
				("card-list", po::value< std::vector<std::string> >());

		po::positional_options_description card_list;
		card_list.add("card-list", -1);

		po::variables_map vm;
		po::parsed_options parsed_opt = po::command_line_parser(argc, argv).options(global).
				positional(card_list).allow_unregistered().run();
		po::store(parsed_opt, vm);
		po::notify(vm);

		std::vector<std::string> options = po::collect_unrecognized(parsed_opt.options, po::exclude_positional);

		if (vm.count("verbose")) {
			set_display_level(PALL);
		}
		if (vm.count("boot")) {
			run_command_state(vm, options, STATE_BOOT);
		}
		if (vm.count("reset")) {
			run_command_state(vm, options, STATE_RESET);
		}
		if (vm.count("shutdown")) {
			run_command_state(vm, options, STATE_SHUTDOWN);
		}
		if (vm.count("reboot")) {
			run_command_state(vm, options, STATE_REBOOT);
		}
		if (vm.count("status")) {
			run_command_status(vm, options);
		}
		if (vm.count("wait")) {
			run_command_wait(vm, options);
		}
		if (vm.count("log")) {
			run_command_log(vm, options);
		}
		if (vm.count("initdefaults")) {
			run_command_initdefaults(vm, options);
		}
		if (vm.count("config")) {
			run_command_config(vm, options);
		}
		if (vm.count("load-modules")) {
			if (vm.count("card-list")) {
				boost::throw_exception(boost::program_options::unknown_option(
					(vm["card-list"].as<std::vector<std::string>>())[0]));
			}

			if (vm.count("help")) {
				micctrl_help(loadmodules_help);
			}
			if (options.size() > 0) {
				boost::throw_exception(boost::program_options::unknown_option(options[0]));
			}
			if (load_mic_modules()) {
				display(PERROR, "cannot load kernel modules\n");
				exit(EXIT_FAILURE);
			}
			exit(0);
		}
		if (vm.count("unload-modules")) {
			if (vm.count("card-list")) {
				boost::throw_exception(boost::program_options::unknown_option(
					(vm["card-list"].as<std::vector<std::string>>())[0]));
			}

			if (vm.count("help")) {
				micctrl_help(unloadmodules_help);
			}
			if (options.size() > 1) {
				boost::throw_exception(boost::program_options::unknown_option(options[1]));
			}

			po::options_description unload_options;
			unload_options.add_options()
				("force,f", "");

			po::store(po::command_line_parser(options).options(unload_options).run(), vm);

			if (!mpssenv_check_lockfile() && !vm.count("force")) {
				display(PERROR, "cannot unload kernel modules - MPSS daemon is running.\n"
					"    If you want to unload modules despite of this fact, \n"
					"    use additional --force option (or shortened version -f);\n"
					"    MPSS daemon service will be stopped after this operation.\n");
				exit(EXIT_FAILURE);
			}

			if (unload_mic_modules()) {
				display(PERROR, "cannot unload kernel modules\n");
				exit(EXIT_FAILURE);
			}
			exit(0);
		}
		if (vm.count("help")) {
			if (options.size() > 0) {
				boost::throw_exception(boost::program_options::unknown_option(options[0]));
			}
			micctrl_help(main_help);
		}

		if (options.size() > 0) {
			boost::throw_exception(boost::program_options::unknown_option(options[0]));
		} else {
			micctrl_help(short_help);
		}
	} catch (std::exception const& e) {
		display(PERROR, "%s\n", e.what());
		exit(EXIT_FAILURE);
	} catch (...) {
		display(PERROR, "general error\n");
		exit(EXIT_FAILURE);
	}
}

