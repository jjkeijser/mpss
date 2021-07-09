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

#include <stdexcept>

#include "ScifImpl.hpp"

#include <scif.h>

int ScifImpl::accept(int epd, uint16_t *peer_node, uint16_t *peer_port, int *new_epd, bool do_block)
{
    if(!peer_node || !peer_port)
        throw std::invalid_argument("NULL node/port pointer");

    int flags = do_block ? SCIF_ACCEPT_SYNC : 0;
    scif_portID peer = {*peer_node, *peer_port};
    int ret = scif_accept((scif_epd_t)epd, &peer, (scif_epd_t*)new_epd, flags);
    if(ret != -1)
    {
        *peer_node = peer.node;
        *peer_port = peer.port;
    }
    return ret;
}

int ScifImpl::bind(int epd, uint16_t pn)
{
    return scif_bind((scif_epd_t)epd, pn);
}

int ScifImpl::close(int epd)
{
    return scif_close((scif_epd_t)epd);
}

int ScifImpl::connect(int epd, uint16_t *node, uint16_t *port)
{
    if(!node || !port)
        throw std::invalid_argument("NULL node/port pointer");

    scif_portID port_id = {*node, *port};
    int ret = scif_connect((scif_epd_t)epd, &port_id);
    if(ret != -1)
    {
        *node = port_id.node;
        *port = port_id.port;
    }
    return ret;
}

int ScifImpl::get_node_ids(uint16_t *nodes, int len, uint16_t *self)
{
    return scif_get_nodeIDs(nodes, len, self);
}

int ScifImpl::listen(int epd, int backlog)
{
    return scif_listen((scif_epd_t)epd, backlog);
}

int ScifImpl::open()
{
    return scif_open();
}

int ScifImpl::recv(int epd, void *msg, int len, bool do_block)
{
    int total_bytes = 0;
    int bytes_recvd = 0;
    // Negative value means infinite timeout, which is what we want when do_block==true
    long timeout = (do_block ? -1 : 0);
    struct scif_pollepd pollepd = { epd, SCIF_POLLIN, 0 };

    int ret = -1;
    if((ret = scif_poll(&pollepd, 1, timeout)) <= 0)
        //0 means time out, -1 plain error
        return ret;

    //Data is available to read.
    //Now, read non-blocking to fill buffer. Save the total amount of bytes read.
    //Add total_bytes to buf pointer to write to correct offset.
    //Pass len = len - total_bytes to avoid overflows.
    char *cbuf = (char*)msg;
    const int flags = 0;
    bytes_recvd = scif_recv(epd, cbuf + total_bytes, len - total_bytes, flags);
    //scif_recv will return 0 bytes when there is no more data available
    while(bytes_recvd != -1 && bytes_recvd != 0)
    {
        total_bytes += bytes_recvd;
        bytes_recvd = scif_recv(epd, cbuf + total_bytes, len - total_bytes, flags);
    }

    if(bytes_recvd == -1)
        return bytes_recvd;

    return total_bytes;
}

int ScifImpl::send(int epd, void *msg, int len, bool do_block)
{
    int flags = do_block ? SCIF_SEND_BLOCK : 0;
    return scif_send((scif_epd_t)epd, msg, len, flags);
}

std::vector<int> ScifImpl::poll_read(const std::vector<int> &fds, long timeout, int *error)
{
    if(!fds.size())
        throw std::invalid_argument("empty fds vector");

    if(!error)
        throw std::invalid_argument("NULL error pointer");

    std::vector<scif_pollepd> epds;
    std::vector<int> ready_fds;
    for(auto fd = fds.begin(); fd != fds.end(); ++fd)
    {
        scif_pollepd epd = { *fd, SCIF_POLLIN, 0 };
        epds.push_back(epd);
    }

    if((*error = scif_poll(&epds[0], epds.size(), timeout)) == -1)
    {
        return ready_fds;
    }

    for(auto epd = epds.begin(); epd != epds.end(); ++epd)
    {
        // Include those with SCIF_POLLIN and without any poll errors
        if(epd->revents & SCIF_POLLIN &&
           !(epd->revents & SCIF_POLLERR ||
               epd->revents & SCIF_POLLHUP ||
               epd->revents & SCIF_POLLNVAL))

            ready_fds.push_back(epd->epd);
    }
    return ready_fds;
}

int ScifImpl::poll(scif_pollepd *epd, unsigned int nepds, long timeout)
{
    return scif_poll(epd, nepds, timeout);
}

