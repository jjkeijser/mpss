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

#ifndef _SYSTOOLS_SYSTOOLSD_SCIFEPINTERFACE_HPP_
#define _SYSTOOLS_SYSTOOLSD_SCIFEPINTERFACE_HPP_

#include <cstdint>
#include <vector>

class scif_pollepd;

class ScifSocket
{
public:
    virtual ~ScifSocket() { };
    virtual int accept(int epd, uint16_t *peer_node, uint16_t *peer_port, int *new_epd, bool do_block=true) = 0;
    virtual int bind(int epd, uint16_t pn) = 0;
    virtual int close(int epd) = 0;
    virtual int connect(int epd, uint16_t *node, uint16_t *port) = 0;
    virtual int get_node_ids(uint16_t *nodes, int len, uint16_t *self) = 0;
    virtual int listen(int epd, int backlog) = 0;
    virtual int open() = 0;
    virtual int recv(int epd, void *msg, int len, bool do_block=true) = 0;
    virtual int send(int epd, void *msg, int len, bool do_block=true) = 0;
    virtual std::vector<int> poll_read(const std::vector<int> &fds, long timeout, int *error) = 0;
    virtual int poll(scif_pollepd *epd, unsigned int nepds, long timeout) = 0;
};

#endif //_SYSTOOLS_SYSTOOLSD_SCIFEPINTERFACE_HPP_
