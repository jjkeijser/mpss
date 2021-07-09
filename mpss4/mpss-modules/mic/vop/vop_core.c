/*
 * Intel MIC Platform Software Stack (MPSS)
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
 *
 * Intel Virtio Over PCIe (VOP) driver.
 */
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>

#include <linux/moduleparam.h>
#include <linux/module.h>

#ifdef MIC_IN_KERNEL_BUILD
#include <linux/mic_common.h>
#else
#include "../common/mic_common.h"
#endif
#include "../common/mic_dev.h"

#include "vop.h"

/*
 * Synchronize access to the device page as well as serialize
 * creation/deletion of virtio devices on the peer node
 */
DEFINE_MUTEX(vop_mutex);

static int vop_driver_probe(struct vop_device *vpdev)
{
	struct vop_info *vi;
	int rc;

	vi = kzalloc(sizeof(*vi), GFP_KERNEL);
	if (!vi) {
		rc = -ENOMEM;
		goto exit;
	}

	vpdev->priv = vi;
	vi->vpdev = vpdev;

	rc = vop_init(vi);
	if (rc < 0)
		goto free;

	vop_create_debug_dir(vi);

	return 0;
free:
	kfree(vi);
exit:
	log_mic_err(vop_get_id(vpdev), "exiting with error code %d", rc);
	return rc;
}

static void vop_driver_remove(struct vop_device *vpdev)
{
	struct vop_info *vi = vpdev->priv;

	vop_uninit(vi);
	vop_delete_debug_dir(vi);

	kfree(vi);
}

static struct vop_device_id id_table[] = {
	{ VOP_DEV_TRNSP, VOP_DEV_ANY_ID },
	{ 0 },
};

static struct vop_driver vop_driver = {
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = id_table,
	.probe = vop_driver_probe,
	.remove = vop_driver_remove,
};

static int __init vop_driver_init(void)
{
	int ret;

	vop_init_debugfs();

	ret = vop_register_driver(&vop_driver);
	if (ret) {
		pr_err("vop_register_driver failed ret %d\n", ret);
		goto cleanup_debugfs;
	}
	return 0;

cleanup_debugfs:
	vop_exit_debugfs();
	return ret;
}

static void __exit vop_driver_exit(void)
{
	vop_unregister_driver(&vop_driver);
	vop_exit_debugfs();
}

module_init(vop_driver_init);
module_exit(vop_driver_exit);

MODULE_DEVICE_TABLE(mbus, id_table);
MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) Virtio Over PCIe (VOP) driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(MIC_VERSION);
