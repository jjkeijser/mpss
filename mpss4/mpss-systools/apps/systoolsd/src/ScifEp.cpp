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

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <map>

#include <poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "ScifEp.hpp"
#include "ScifSocket.hpp"
#include "SystoolsdException.hpp"

#include "systoolsd_api.h"

///@class ScifEp
///@brief An object-oriented SCIF abstraction


///@brief This constructor opens a new SCIF endpoint descriptor ready to be
///bound to a local port, and either connect to other endpoints or listen
///for incoming connections.
ScifEp::ScifEp(ScifSocket *impl) : ep(0), node(0), port(0), scif_impl(impl)
{
    init();
}

///@brief This constructor is used internally only to create new ScifEp objects
///from a ScifEp::accept call.

ScifEp::ScifEp(std::shared_ptr<ScifSocket> impl, int new_ep, uint16_t new_node, uint16_t new_port) :
    ep(new_ep), node(new_node), port(new_port), scif_impl(std::move(impl))
{

}

ScifEp::~ScifEp()
{
    shutdown();
}

///@brief This method binds the endpoint represented by the instance to the
///specified port.
///@param [in] port [optional, default=0] The port to bind the endpoint to. When
///default value is used, libscif will internally bind the endpoint to a port
///greater than or equal to SCIF_PORT_RSVD.
void ScifEp::bind(uint16_t port)
{
    //scif_bind returns the port number to which ep is bound
    int ret = scif_impl->bind(ep, port);
    if(ret == -1)
    {
        die("bind", errno);
    }
    this->port = ret;
}

///@brief This method sets the endpoint to listening mode, or in other words,
///the endpoint is ready to start accepting connections.
///@param [in] backlog [optional, default=5] The maximum size of the pending
///connections queue.
void ScifEp::listen(int backlog)
{
    int ret = scif_impl->listen(ep, backlog);
    if(ret == -1)
    {
        die("listen", errno);
    }
}

///@brief This method connects the endpoint to the specified SCIF port ID.
///@param [in] node [required] The SCIF node to connect the endpoint to.
///@param [in] port [required] The SCIF port to connect the endpoint to.
///@note The endpoint must not be in "listening" mode when calling connect.
void ScifEp::connect(uint16_t node, uint16_t port)
{
    //scif_connect returns the port number to which ep is bound
    int ret = scif_impl->connect(ep, &node, &port);
    if(ret == -1)
    {
        die("connect", errno);
    }
    this->port = ret;
}

///@brief This method extracts the first connection request on the queue of pending connections.
///It creates a new ScifEp object, wrapped in a std::shared_ptr.
///@param [in] do_block [optional, default=true] When this parameter is set to \c true, the method call
///will block until a new connection request arrives; otherwise, the call returns immediately if there
///is a connection in the queue, or an exception is raised if no connections requests are queued.
///@return A new ScifEp object wrapped in a std::shared_ptr object.
ScifEp::ptr ScifEp::accept(bool do_block)
{
    uint16_t node, port = 0;
    int new_epd = -1;

    int ret = scif_impl->accept(ep, &node, &port, &new_epd, do_block);
    if(ret == -1)
    {
        die("accept", errno);
    }
    return ScifEp::ptr(new ScifEp(scif_impl, new_epd, node, port));
}

///@brief This method receives data from a SCIF peer.
///@aparam [out] buf [required] The buffer to put received data in.
///@param [in] len [required] The size of the buffer.
///@param [in] do_block [optional, default=true] When this parameter is set to \c true,
///the method call will block until all data sent by peer is received; otherwise, the
///call returns immediately and it will only receive those bytes that can be received
///without waiting.
///@return The number of bytes received.
int ScifEp::recv(char *const buf, int len, bool do_block)
{
    if(!buf)
    {
        die("scif_recv", EFAULT);
    }

    return scif_impl->recv(ep, (void*)buf, len, do_block);
}

///@brief This method sends data to the associated endpoint.
///@param [in] buf [required] The buffer containing the data to be sent.
///@param [in] len [required] The number of bytes to be sent.
///@return The number of bytes sent.
int ScifEp::send(const char * const buf, int len)
{
    if(!buf)
    {
        die("send", EFAULT);
    }

    return scif_impl->send(ep, (void*)buf, len, true);
}

///@brief This method returns the SCIF port ID of the associated endpoint.
///@return Returns the SCIF port ID of the endpoint, where "first" member
///represents the node, and "second" represents the port.
std::pair<uint16_t, uint16_t> ScifEp::get_port_id()
{
    return std::pair<uint16_t, uint16_t>(node, port);
}

///@brief This method returns the end point descriptor
//@return A small non-negative integer representing the end point descriptor.
int ScifEp::get_epd()
{
    return ep;
}

///@brief This method closes the SCIF endpoint. If the endpoint was in "listening" mode,
///no further connections will be accepted; if the endpoint was connected to another peer,
///the connection is closed.
void ScifEp::close()
{
    shutdown();
}

///@brief This method resets the endpoint by closing the connection, closing the SCIF
///endpoint descriptor, and getting a new endpoint descriptor afterwards.
void ScifEp::reset()
{
    shutdown();
    init();
}

///@brief This function returns a vector of ScifEp objects that have data available
///to be read.
///@param [in] read_eps [required] A std::vector containing ScifEp::ptr objects.
///@param [in] tv [optional, default=NULL] A timeval struct specifying the amount of time to wait
///for data to become available for reading. If this parameter is NULL, then the call will block
///until data becomes available for reading.
std::vector<ScifEp::ptr> ScifEp::select_read(const std::vector<ScifEp::ptr> &read_eps, timeval *tv)
{
    std::map<int, ScifEp::ptr> fds_to_eps;
    std::vector<int> fds;
    for(auto ep = read_eps.begin(); ep != read_eps.end(); ++ep)
    {
        fds_to_eps[(*ep)->get_epd()] = *ep;
        fds.push_back((*ep)->get_epd());
    }

    long timeout = -1;
    if(tv)
        timeout = (tv->tv_sec * 1000) + (tv->tv_usec / 1000);

    int error = 0;
    // Tell us which file descriptors are ready to be read
    auto ready_fds = scif_impl->poll_read(fds, timeout, &error);
    if(error == -1)
    {
        std::stringstream ss;
        ss << "poll: errno " << errno;
        throw SystoolsdException(SYSTOOLSD_SCIF_ERROR, ss.str().c_str());
    }

    std::vector<ScifEp::ptr> ready_scifeps;
    for(auto fd = ready_fds.begin(); fd != ready_fds.end(); ++fd)
        ready_scifeps.push_back(fds_to_eps[*fd]);

    return ready_scifeps;
}

int ScifEp::poll(scif_pollepd *epd, unsigned int  nepds, long timeout)
{
    return scif_impl->poll(epd, nepds, timeout);
}

void ScifEp::shutdown()
{
    scif_impl->close(ep);
    node = 0;
    port = 0;
}

void ScifEp::init()
{
    ep = scif_impl->open();
    if(ep < 0)
    {
       die("open", errno);
    }

    //Find out on which node this instance is being created
    uint16_t this_node = -1;
    errno = 0;
    int total_nodes = scif_impl->get_node_ids(NULL, 0, &this_node);
    if(total_nodes == 1 && errno != 0)
    {
        scif_impl->close(ep);
        die("get_node_ids", errno);
    }
    node = this_node;
}

void ScifEp::die(const char *msg, int sys_errno)
{
    std::stringstream ss;
    ss << msg << " : " << sys_errno << " : " << strerror(sys_errno);
    throw SystoolsdException(SYSTOOLSD_SCIF_ERROR, ss.str().c_str());
}

void ScifEp::die(const char *msg)
{
    throw SystoolsdException(SYSTOOLSD_SCIF_ERROR, msg);
}

