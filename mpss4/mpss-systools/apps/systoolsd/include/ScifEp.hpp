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

#ifndef _SYSTOOLS_SYSTOOLSD_SCIFEP_HPP_
#define _SYSTOOLS_SYSTOOLSD_SCIFEP_HPP_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#ifdef UNIT_TESTS
#define PROTECTED public
#else
#define PROTECTED protected
#endif

#ifdef UNIT_TESTS
#define PRIVATE public
#else
#define PRIVATE private
#endif

class ScifSocket;
class timeval;
class scif_pollepd;

class ScifEpInterface
{
public:
    using ptr = std::shared_ptr<ScifEpInterface>;
    using ScifEpVector = std::vector<ScifEpInterface::ptr>;

    virtual ~ScifEpInterface() { };
    virtual void bind(uint16_t port=0) = 0;
    virtual void listen(int backlog=5) = 0;
    virtual void connect(uint16_t node, uint16_t port) = 0;
    virtual ScifEpInterface::ptr accept(bool do_block=true) = 0;
    virtual int recv(char * const buf, int len, bool do_block=true) = 0;
    virtual int send(const char * const buf, int len) = 0;
    virtual std::pair<uint16_t, uint16_t> get_port_id() = 0;
    virtual int get_epd() = 0;
    virtual void close() = 0;
    virtual void reset() = 0;
    virtual ScifEpVector select_read(const ScifEpVector &read_eps, timeval *tv = nullptr) = 0;
    virtual int poll(scif_pollepd *epd, unsigned int  nepds, long timeout) = 0;
};

class ScifEp : public ScifEpInterface
{
public:
    ScifEp(ScifSocket *implementation);
    virtual ~ScifEp();
    typedef std::pair<uint16_t, uint16_t> scif_port_id;

    void bind(uint16_t port=0);
    void listen(int backlog=5);
    void connect(uint16_t node, uint16_t port);
    ScifEpInterface::ptr accept(bool do_block=true);
    int recv(char * const buf, int len, bool do_block=true);
    int send(const char * const buf, int len);
    std::pair<uint16_t, uint16_t> get_port_id();
    int get_epd();
    void close();
    void reset();

    ScifEpVector select_read(const ScifEpVector &read_eps, timeval *tv = nullptr);
    virtual int poll(scif_pollepd *epd, unsigned int  nepds, long timeout);

PROTECTED:
    static void die(const char *msg, int sys_errno);
    static void die(const char *msg);
    void init();
    void shutdown();
    //Special constructor used internally to create new endpoints
    // from a scif_accept() call
    ScifEp(std::shared_ptr<ScifSocket> impl, int new_ep, uint16_t node, uint16_t port);
    int ep;
    uint16_t node;
    uint16_t port;

private:
    //Make object uncopyable
    ScifEp(const ScifEp &c);
    ScifEp &operator=(ScifEp &c);

PRIVATE:
    std::shared_ptr<ScifSocket> scif_impl;
};


#endif
