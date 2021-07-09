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

/*
 * This header is used only for external functions.
 */
#pragma once

#ifndef MPSS_EXPORT
#define MPSS_EXPORT __attribute__((visibility("default")))
#endif

/**
 * boot_linux  Boot the MIC node
 *     \param node      Node of MIC card to boot linux
**/
extern "C" MPSS_EXPORT int boot_linux(int node);

/**
 * boot_media Boot media file on MIC node
 *     \param node        Node of MIC card to boot linux
 *     \param media_path  Path to media file (relative to /lib/firmware)
**/
extern "C" MPSS_EXPORT int boot_media(int node, const char *media_path);

/**
 * get_state Get state of MIC node
 *
 * Return sysfs state entry: Online, Booting, Ready only.
**/
extern "C" MPSS_EXPORT const char *get_state(int node);

/**
 * reset_node Reset MIC node
 *     \param node        Node of MIC card to boot linux
 *
 * reset node
**/
extern "C" MPSS_EXPORT int reset_node(int node);
