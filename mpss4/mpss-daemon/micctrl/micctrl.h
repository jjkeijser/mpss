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

#pragma once

#include <libmpsscommon.h>
#include <mpssconfig.h>

#include <boost/program_options.hpp>
#include <ostream>
#include <memory>
#include <string>
#include <vector>

namespace po = boost::program_options;

/* Global data defined in micctrl.c and used all functions */
extern struct mpss_elist mpssperr;

void run_command_config(po::variables_map &vm, const std::vector<std::string> &options);
void run_command_state(po::variables_map &vm, const std::vector<std::string> &options, int mode);
void run_command_wait(po::variables_map &vm, const std::vector<std::string> &options);
void run_command_log(po::variables_map &vm, const std::vector<std::string> &options);
void run_command_initdefaults(po::variables_map &vm, const std::vector<std::string> &pptions);
void run_command_status(po::variables_map &vm, const std::vector<std::string> &options);

/* Used in micctrl.c to find all the various command line parser function for options */
void parse_state_args(int mode, int argc, char *argv[]);
#define STATE_BOOT	0
#define STATE_RESET	1
#define STATE_SHUTDOWN	2
#define STATE_REBOOT	3
void parse_wait_args(int argc, char *argv[]);
void parse_status_args(int argc, char *argv[]);
void parse_config_args(int argc, char *argv[]);
enum
{
	REMOVE_CONFIG = 0x1,
	REMOVE_IMAGE = 0x2,
	REMOVE_ALL = 0x3
};
void parse_showconfig_args(int argc, char *argv[]);
void parse_cardlog_args(int argc, char *argv[]);

/* Utility functions from utilfuncs.c */
int micctrl_parse_config(struct mic_info *mic, mpss_elist& perrs, int errlev);

std::shared_ptr<mic_info> create_miclist(const po::variables_map &vm, bool use_config_files = false);
void check_rootid(void);
void memory_alloc_error(void);

/* Supplied by rootdev.c for init.c and config.c */
void report_rootdev(struct mic_info *mic);
void prepare_pfs_root_device(struct mic_info *mic, int remove_mode);

/* help.c */
void micctrl_help(const char *buffer);

/* Message display functions from utilfuncs.c */
#define PFS	4	// Display levels for micctrl not known by the perr library code
#define PNET	5

extern unsigned char display_level;

void display(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));
void display_raw(int  level, const char *format, ...) __attribute__((format(printf, 2, 3)));
const char *display_indent(void);
void bump_display_level(void);
void set_display_level(unsigned char level);

bool is_mic_images_being_used(struct mic_info *mic);

bool prompt_user(void);

void output_mpss_config(struct mic_info *mic, std::ostream &stream);
void output_mpss_default_config(std::ostream &stream);

void set_pfs_root_device(struct mic_info *mic, std::ostream &stream);
void set_pfs_repo_device(struct mic_info *mic, std::ostream &stream);

const int CONFIG_DISPLAY_LENGTH = 32;
const int FIRST_LEVEL_INDENT = 4;
const int SECOND_LEVEL_INDENT = 8;

void println_indent(int level, const std::string& key, const std::string& value);
void print_indent(int level, const std::string& key, const std::string& value);

std::string get_default_card_ip_address(int mic_id);
std::string get_default_host_ip_address(int mic_id);
std::string get_netmask();
