/*
 * Intel MIC Platform Software Stack (MPSS)
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Intel SCIF driver.
 */
#include <linux/idr.h>
#include <linux/version.h>

#include "scif_main.h"

#define SCIF_PORT_COUNT	0x10000	/* Ports available */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
struct ida scif_ports;

/*
 * struct scif_port - SCIF port information
 *
 * @id - port id
 * @ref_cnt - Reference count since there can be multiple endpoints
 *		created via scif_accept(..) simultaneously using a port.
 * @list - List to next scif_port structure
 */
struct scif_port {
	int id;
	int ref_cnt;
	struct list_head list;
};

/**
 * __scif_get_port - Reserve a specified port # for SCIF and add it
 * to the global list.
 * @port : port # to be reserved.
 *
 * @return : Allocated SCIF port #, or -ENOSPC if port unavailable.
 *		On memory allocation failure, returns -ENOMEM.
 */
static int __scif_get_port(int start, int end)
{
	int id;
	struct scif_port *port;

	spin_lock(&scif_info.port_lock);
	id = ida_simple_get(&scif_ports, start, end, GFP_ATOMIC);
	if (id >= 0) {
		port = kzalloc(sizeof(*port), GFP_ATOMIC);
		if (!port) {
			ida_simple_remove(&scif_ports, id);
			id = -ENOMEM;
			goto unlock;
		}
		port->ref_cnt++;
		port->id = id;
		list_add_tail(&port->list, &scif_info.ports_in_use);
	}
unlock:
	spin_unlock(&scif_info.port_lock);
	return id;
}

/**
 * scif_rsrv_port - Reserve a specified port # for SCIF.
 * @port : port # to be reserved.
 *
 * @return : Allocated SCIF port #, or -ENOSPC if port unavailable.
 *		On memory allocation failure, returns -ENOMEM.
 */
int scif_rsrv_port(u16 port)
{
	return __scif_get_port(port, port + 1);
}

/**
 * scif_get_new_port - Get and reserve any port # for SCIF in the range
 *			SCIF_PORT_RSVD + 1 to SCIF_PORT_COUNT - 1.
 *
 * @return : Allocated SCIF port #, or -ENOSPC if no ports available.
 *		On memory allocation failure, returns -ENOMEM.
 */
int scif_get_new_port(void)
{
	return __scif_get_port(SCIF_PORT_RSVD + 1, SCIF_PORT_COUNT);
}

/**
 * scif_get_port - Increment the reference count for a SCIF port
 * @id : SCIF port
 *
 * @return : None
 */
void scif_get_port(u16 id)
{
	struct list_head *item;
	struct scif_port *port;

	if (!id)
		return;
	spin_lock(&scif_info.port_lock);
	list_for_each(item, &scif_info.ports_in_use) {
		port = list_entry(item, struct scif_port, list);
		if (port->id == id) {
			port->ref_cnt++;
			break;
		}
	}
	spin_unlock(&scif_info.port_lock);
}

/**
 * scif_put_port - Release a reserved SCIF port
 * @id : SCIF port to be released.
 *
 * @return : None
 */
void scif_put_port(u16 id)
{
	struct scif_port *port;
	struct list_head *pos, *tmpq;

	if (!id)
		return;
	spin_lock(&scif_info.port_lock);
	list_for_each_safe(pos, tmpq, &scif_info.ports_in_use) {
		port = list_entry(pos, struct scif_port, list);
		if (id == port->id) {
			port->ref_cnt--;
			if (!port->ref_cnt) {
				list_del(&port->list);
				ida_simple_remove(&scif_ports, port->id);
				kfree(port);
				break;
			}
		}
	}
	spin_unlock(&scif_info.port_lock);
}

#else
struct idr scif_ports;

/*
 * struct scif_port - SCIF port information
 *
 * @ref_cnt - Reference count since there can be multiple endpoints
 *		created via scif_accept(..) simultaneously using a port.
 */
struct scif_port {
	int ref_cnt;
};

/**
 * __scif_get_port - Reserve a specified port # for SCIF and add it
 * to the global list.
 * @port : port # to be reserved.
 *
 * @return : Allocated SCIF port #, or -ENOSPC if port unavailable.
 *		On memory allocation failure, returns -ENOMEM.
 */
static int __scif_get_port(int start, int end)
{
	int id;
	struct scif_port *port = kzalloc(sizeof(*port), GFP_ATOMIC);

	if (!port)
		return -ENOMEM;
	spin_lock(&scif_info.port_lock);
	id = idr_alloc(&scif_ports, port, start, end, GFP_ATOMIC);
	if (id >= 0)
		port->ref_cnt++;
	else
		kfree(port);
	spin_unlock(&scif_info.port_lock);
	return id;
}

/**
 * scif_rsrv_port - Reserve a specified port # for SCIF.
 * @port : port # to be reserved.
 *
 * @return : Allocated SCIF port #, or -ENOSPC if port unavailable.
 *		On memory allocation failure, returns -ENOMEM.
 */
int scif_rsrv_port(u16 port)
{
	return __scif_get_port(port, port + 1);
}

/**
 * scif_get_new_port - Get and reserve any port # for SCIF in the range
 *			SCIF_PORT_RSVD + 1 to SCIF_PORT_COUNT - 1.
 *
 * @return : Allocated SCIF port #, or -ENOSPC if no ports available.
 *		On memory allocation failure, returns -ENOMEM.
 */
int scif_get_new_port(void)
{
	return __scif_get_port(SCIF_PORT_RSVD + 1, SCIF_PORT_COUNT);
}

/**
 * scif_get_port - Increment the reference count for a SCIF port
 * @id : SCIF port
 *
 * @return : None
 */
void scif_get_port(u16 id)
{
	struct scif_port *port;

	if (!id)
		return;
	spin_lock(&scif_info.port_lock);
	port = idr_find(&scif_ports, id);
	if (port)
		port->ref_cnt++;
	spin_unlock(&scif_info.port_lock);
}

/**
 * scif_put_port - Release a reserved SCIF port
 * @id : SCIF port to be released.
 *
 * @return : None
 */
void scif_put_port(u16 id)
{
	struct scif_port *port;

	if (!id)
		return;
	spin_lock(&scif_info.port_lock);
	port = idr_find(&scif_ports, id);
	if (port) {
		port->ref_cnt--;
		if (!port->ref_cnt) {
			idr_remove(&scif_ports, id);
			kfree(port);
		}
	}
	spin_unlock(&scif_info.port_lock);
}
#endif
