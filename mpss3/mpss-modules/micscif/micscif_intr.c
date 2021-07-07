/*
 * Copyright 2010-2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * Disclaimer: The codes contained in these modules may be specific to
 * the Intel Software Development Platform codenamed Knights Ferry,
 * and the Intel product codenamed Knights Corner, and are not backward
 * compatible with other Intel products. Additionally, Intel will NOT
 * support the codes or instruction set in future products.
 *
 * Intel offers no warranty of any kind regarding the code. This code is
 * licensed on an "AS IS" basis and Intel is not obligated to provide
 * any support, assistance, installation, training, or other services
 * of any kind. Intel is also not obligated to provide any updates,
 * enhancements or extensions. Intel specifically disclaims any warranty
 * of merchantability, non-infringement, fitness for any particular
 * purpose, and any other warranty.
 *
 * Further, Intel disclaims all liability of any kind, including but
 * not limited to liability for infringement of any proprietary rights,
 * relating to the use of the code, even if Intel is notified of the
 * possibility of such liability. Except as expressly stated in an Intel
 * license agreement provided with this code and agreed upon with Intel,
 * no license, express or implied, by estoppel or otherwise, to any
 * intellectual property rights is granted herein.
 */

#include "mic/micscif.h"
#include "mic/micscif_intr.h"
#include "mic/micscif_nodeqp.h"
#include "mic_common.h"

/* Runs in the context of sd_intr_wq */
static void micscif_intr_bh_handler(struct work_struct *work)
{
	struct micscif_dev *scifdev =
			container_of(work, struct micscif_dev, sd_intr_bh);

	/* figure out which qp we got a recv on */
	struct micscif_qp *qp = micscif_nodeqp_nextmsg(scifdev);
	if (qp != NULL) {
		if (is_self_scifdev(scifdev))
			micscif_loopb_msg_handler(scifdev, qp);
		else
			micscif_nodeqp_intrhandler(scifdev, qp);
	}
}

int micscif_setup_interrupts(struct micscif_dev *scifdev)
{
	if (!scifdev->sd_intr_wq) {
		snprintf(scifdev->sd_intr_wqname, sizeof(scifdev->sd_intr_wqname),
			"SCIF INTR %d", scifdev->sd_node);

		/* FIXME: Fix windows */
		if (!(scifdev->sd_intr_wq =
			__mic_create_singlethread_workqueue(scifdev->sd_intr_wqname)))
			return -ENOMEM;

		INIT_WORK(&scifdev->sd_intr_bh, micscif_intr_bh_handler);
	}
	return 0;
}

void micscif_destroy_interrupts(struct micscif_dev *scifdev)
{
	destroy_workqueue(scifdev->sd_intr_wq);
}

#ifdef _MIC_SCIF_
irqreturn_t micscif_intr_handler(int irq, void *dev_id)
{
	struct micscif_dev *dev = (struct micscif_dev *)dev_id;
	queue_work(dev->sd_intr_wq, &dev->sd_intr_bh);
	return IRQ_HANDLED;
}

/*
 * register_scif_intr_handler() - Registers SCIF interrupt handler with
 * appropriate IRQ
 * @dev:   per node dev structure to store the intr handle
 *
 * IRQ 17 - 24 Corresponds to RDMASR registers RDMASR0 - RRDMASR7.
 * RDMASR registers are chosen based on the lowest ref count.
 * There are 8 RDMASRS for the host and the nodes. So When the number of
 * nodes added to the current node's p2p network increases beyond
 * 7, it starts sharing the interrupt.
 */
int
register_scif_intr_handler(struct micscif_dev *dev)
{
	unsigned int handle = 0;
	unsigned int i;
	int ret;

	mutex_lock(&ms_info.mi_conflock);

	/* Find the first lowest ref count */
	for (i = 0; i < MAX_RDMASR; i++)
		if (ms_info.mi_intr_rcnt[handle] >
				ms_info.mi_intr_rcnt[i])
			handle = i;

	if ((ret = request_irq(get_rdmasr_irq(handle), micscif_intr_handler,
			IRQF_SHARED, dev->sd_intr_wqname, dev))) {
		printk(KERN_ERR "Cannot request irq number %d, ret = %d\n"
						, get_rdmasr_irq(handle), ret);
		goto error;
	}

	ms_info.mi_intr_rcnt[handle]++;
	dev->sd_intr_handle = handle;

	printk("Registered interrupt handler for node %d, for IRQ = %d,"
		"handle = %d\n", dev->sd_node, get_rdmasr_irq(handle), handle);

error:
	mutex_unlock(&ms_info.mi_conflock);
	return ret;
}

/*
 * deregister_scif_intr_handler() - Deregisters SCIF interrupt
 * handler from appropriate IRQ
 * @dev:   per node dev structure to retrieve the intr handle
 *
 */
void
deregister_scif_intr_handler(struct micscif_dev *dev)
{
	unsigned int handle = dev->sd_intr_handle;

	if (handle >= MAX_RDMASR)
		return;

	mutex_lock(&ms_info.mi_conflock);
	ms_info.mi_intr_rcnt[handle]--;

	if (ms_info.mi_intr_rcnt[handle] < 0) {
		printk("scif intr deregister negative ref count"
			" for node %d, handle = %d, IRQ = %d\n", dev->sd_node, 
				handle, get_rdmasr_irq(handle));
		WARN_ON(1);
	}

	mutex_unlock(&ms_info.mi_conflock);
	free_irq(get_rdmasr_irq(handle), dev);
	printk("Deregistered interrupt handler for node %d, for IRQ = %d,"
			"handle = %d\n", dev->sd_node, get_rdmasr_irq(handle), handle);
}
#endif /* _MIC_SCIF_ */
