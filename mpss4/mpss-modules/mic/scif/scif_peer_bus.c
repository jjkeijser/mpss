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
#include <linux/version.h>

#include "scif_main.h"
#include "../bus/scif_bus.h"
#include "scif_peer_bus.h"

struct bus_type scif_peer_bus = {
	.name  = "scif_peer_bus",
};

/*
 * Used to ensure the device add_dev/remove_dev handler is executed only once
 * for each device added to/removed from the scif_peer_bus bus. Without this
 * mutex, if subsys_interface_(un)register is invoked while the device is being
 * added/removed (in function device_add/device_del), the above handler could
 * be executed twice.
 */
DEFINE_MUTEX(scif_peer_bus_mutex);

static void scif_peer_release_dev(struct device *d)
{
	struct scif_dev *scifdev = dev_get_drvdata(d);

	scif_cleanup_scifdev(scifdev);
	kfree(d);

	log_scif_info(scifdev->node, "scif peer device released");
}

void scif_add_peer_device(struct work_struct *work)
{
	struct scif_dev *scifdev = container_of(work, struct scif_dev,
						peer_add_work);

	mutex_lock(&scif_peer_bus_mutex);
	if (device_add(scifdev->peer_dev))
		log_scif_err(scifdev->node, "peer device_add failed");
	else
		log_scif_info(scifdev->node, "peer device added");
	mutex_unlock(&scif_peer_bus_mutex);
}

/*
 * Peer device registration is split into a device_initialize and a device_add.
 * The reason for doing this is as follows: First, peer device registration
 * itself cannot be done in the message processing thread and must be delegated
 * to another workqueue, otherwise if SCIF client probe, called during peer
 * device registration, calls scif_connect(..), it will block the message
 * processing thread causing a deadlock. Next, device_initialize is done in the
 * "top-half" message processing thread and device_add in the "bottom-half"
 * workqueue. If this is not done, SCIF_CNCT_REQ message processing executing
 * concurrently with SCIF_INIT message processing is unable to get a reference
 * on the peer device, thereby failing the connect request.
 */
void scif_peer_register_device(struct scif_dev *scifdev)
{
	struct device *peer_dev;

	log_scif_info(scifdev->node, "scifdev %p sdev %p", scifdev,
		      scifdev->sdev);

	mutex_lock(&scifdev->lock);

	peer_dev = kzalloc(sizeof(*peer_dev), GFP_KERNEL);
	if (!peer_dev) {
		log_scif_err(scifdev->node, "peer dev allocation failed");
		goto exit;
	}

	device_initialize(peer_dev);
	dev_set_name(peer_dev, "scif_peer-mic%u", scifdev->node);
	dev_set_drvdata(peer_dev, scifdev);
	peer_dev->bus = &scif_peer_bus;
	peer_dev->parent = scifdev->sdev->dev.parent;
	peer_dev->release = scif_peer_release_dev;

	scifdev->peer_dev = peer_dev;

	mutex_lock(&scif_info.conflock);
	scif_info.total++;
	scif_info.maxid = max_t(u32, scifdev->node, scif_info.maxid);
	mutex_unlock(&scif_info.conflock);

	schedule_work(&scifdev->peer_add_work);
exit:
	mutex_unlock(&scifdev->lock);
}

int scif_peer_unregister_device(struct scif_dev *scifdev)
{
	struct device *peer_dev;

	log_scif_info(scifdev->node, "scifdev %p sdev %p", scifdev,
		      scifdev->sdev);

	mutex_lock(&scifdev->lock);

	/* Flush work to ensure device register is complete */
	flush_work(&scifdev->peer_add_work);

	if (!scifdev->peer_dev) {
		mutex_unlock(&scifdev->lock);
		return -ENODEV;
	}

	mutex_lock(&scif_info.conflock);
	scif_info.total--;
	mutex_unlock(&scif_info.conflock);

	peer_dev = scifdev->peer_dev;
	scifdev->peer_dev = NULL;

	mutex_lock(&scif_peer_bus_mutex);
	log_scif_info(scifdev->node, "removing peer device");
	device_unregister(peer_dev);
	mutex_unlock(&scif_peer_bus_mutex);

	mutex_unlock(&scifdev->lock);

	return 0;
}

int scif_peer_bus_init(void)
{
	return bus_register(&scif_peer_bus);
}

void scif_peer_bus_exit(void)
{
	bus_unregister(&scif_peer_bus);
}
