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

#include <linux/cdev.h>
#include <linux/reboot.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34))
#include <linux/pm_qos_params.h>
#endif

#include <mic/micscif.h>
#include <mic/micscif_smpt.h>
#include <mic/micscif_rb.h>
#include <mic/micscif_intr.h>
//#include <micscif_test.h>
#include <mic/micscif_nodeqp.h>
#include <mic/mic_dma_api.h>
#include <mic/micscif_kmem_cache.h>
/* Include this for suspend/resume notifications from pm driver */
#include <mic/micscif_nm.h>

#ifdef CONFIG_MK1OM
#define MICPM_DEVEVENT_SUSPEND		1
#define MICPM_DEVEVENT_RESUME		2
#define MICPM_DEVEVENT_FAIL_SUSPEND	3
extern void micpm_device_register(struct notifier_block *n);
extern void micpm_device_unregister(struct notifier_block *n);
#endif

int scif_id = 0;
module_param(scif_id, int, 0400);
MODULE_PARM_DESC(scif_id, "Set scif driver node ID");

ulong scif_addr = 0;
module_param(scif_addr, ulong, 0400);
MODULE_PARM_DESC(scif_addr, "Set scif driver host address");

struct kmem_cache *unaligned_cache;

struct mic_info {
	dev_t		 m_dev;
	struct cdev	 m_cdev;
	struct class *	 m_class;
	struct device *	 m_scifdev;
} micinfo;

int micscif_major = SCIF_MAJOR;
int micscif_minor = 0;

struct micscif_info ms_info;

// MAX MIC cards + 1 for the Host
struct micscif_dev scif_dev[MAX_BOARD_SUPPORTED + 1];

extern mic_dma_handle_t mic_dma_handle;

static int mic_pm_qos_cpu_dma_lat = -1;
static int mic_host_numa_node = -1;
static unsigned long mic_p2p_proxy_thresh = -1;

#ifdef CONFIG_MK1OM
static int micscif_devevent_handler(struct notifier_block *nb,
				    unsigned long event,
				    void *msg)
{
	if (event == MICPM_DEVEVENT_SUSPEND)
		return micscif_suspend_handler(nb, event, msg);
	else if (event == MICPM_DEVEVENT_RESUME)
		return micscif_resume_handler(nb, event, msg);
	else if (event == MICPM_DEVEVENT_FAIL_SUSPEND)
		return micscif_fail_suspend_handler(nb, event, msg);
	return 0;
}

static struct notifier_block mic_deviceevent = {
	.notifier_call = micscif_devevent_handler,
};
#endif

static int micscif_open(struct inode *in, struct file *f)
{
	dev_t dev = in->i_rdev;

	switch (MINOR(dev)) {
	case 0:
		/*  base mic device access for testing */
		return 0;
	case 1:
		return scif_fdopen(f);
	}

	return -EINVAL;
}

static int micscif_ioctl(struct inode *in, struct file *f,
	unsigned int cmd, unsigned long arg)
{
	dev_t dev = in->i_rdev;

	if (MINOR(dev) == 1) {
		/*  SCIF device */
		return scif_process_ioctl(f, cmd, arg);
	}
	return -EINVAL;
}

static long micscif_unlocked_ioctl(struct file *f,
	unsigned int cmd, unsigned long arg)
{
	return (long) micscif_ioctl(f->f_path.dentry->d_inode, f, cmd, arg);
}

static int micscif_release(struct inode *in, struct file *f)
{
	dev_t dev = in->i_rdev;

	switch (MINOR(dev)) {
	case 0:
		/*  base mic device access for testing */
		return 0;
	case 1:
		return scif_fdclose(f);
	}

	return -EINVAL;
}

/* TODO: Need to flush the queue, grab some lock, and probably
 * notify the remote node we're going down ... right now, we're
 * just freeing things, which is probably a bad idea :-)
 */
static int micscif_uninit_qp(struct micscif_dev *scifdev)
{
	int i;
	/* first, iounmap/unmap/free any memory we mapped */
	for (i = 0; i < scifdev->n_qpairs; i++) {
		iounmap(scifdev->qpairs[i].remote_qp);
		iounmap(scifdev->qpairs[i].outbound_q.rb_base);
		kfree((void *)scifdev->qpairs[i].inbound_q.rb_base);
	}
	kfree(scifdev->qpairs);
	scifdev->n_qpairs = 0;

	return 0;
}

static int micscif_reboot(struct notifier_block *notifier, unsigned long unused1, void *unused2);

static struct notifier_block micscif_reboot_notifier = {
	.notifier_call = micscif_reboot,
	.priority = 0,
};

extern struct attribute_group scif_attr_group;

void micscif_destroy_base(void)
{
#ifdef CONFIG_MMU_NOTIFIER
	destroy_workqueue(ms_info.mi_mmu_notif_wq);
#endif
	destroy_workqueue(ms_info.mi_misc_wq);
	destroy_workqueue(ms_info.mi_conn_wq);

	sysfs_remove_group(&micinfo.m_scifdev->kobj, &scif_attr_group);
	device_destroy(micinfo.m_class, micinfo.m_dev + 1);
	device_destroy(micinfo.m_class, micinfo.m_dev);
	class_destroy(micinfo.m_class);
	cdev_del(&(micinfo.m_cdev));
	unregister_chrdev_region(micinfo.m_dev, 2);
}

static void _micscif_exit(void)
{
	struct list_head *pos, *unused;
	struct scif_callback *temp;
	struct micscif_dev *dev;
	int i;

	pr_debug("Goodbye SCIF!\n");
	/* Cleanup P2P Node Qp/ Interrupt Handlers */
	for (i = SCIF_HOST_NODE + 1; i <= MAX_BOARD_SUPPORTED; i++) {
		dev = &scif_dev[i];

		if (is_self_scifdev(dev))
			continue;

		micscif_cleanup_scifdev(dev, DESTROY_WQ);
	}

	list_for_each_safe(pos, unused, &ms_info.mi_event_cb) {
		temp = list_entry(pos, struct scif_callback, list_member);
		list_del(pos);
		kfree(temp);
	}
	mutex_destroy(&ms_info.mi_event_cblock);

#ifdef CONFIG_MK1OM
	micpm_device_unregister(&mic_deviceevent);
#endif

	scif_dev[ms_info.mi_nodeid].sd_state = SCIFDEV_STOPPING;
	scif_dev[SCIF_HOST_NODE].sd_state = SCIFDEV_STOPPING;

	/* The EXIT message is the last message from MIC to the Host */
	micscif_send_exit();

	/*
	 * Deliberate infinite wait for a host response during driver
	 * unload since the host must inform other SCIF nodes about
	 * this node going away and then only send a response back
	 * to this node to avoid this nodes host shutdown handler racing
	 * with disconnection from the SCIF network. There is a timeout
	 * on the host for sending a response back so a response will
	 * be sent else the host has crashed.
	 */
	wait_event(ms_info.mi_exitwq,
		scif_dev[ms_info.mi_nodeid].sd_state == SCIFDEV_STOPPED);
	scif_proc_cleanup();
	mic_debug_uninit();
	micscif_kmem_cache_destroy();

	micscif_destroy_base();

	/* Disable interrupts */
	deregister_scif_intr_handler(&scif_dev[SCIF_HOST_NODE]);
	destroy_workqueue(scif_dev[SCIF_HOST_NODE].sd_intr_wq);
	micscif_destroy_loopback_qp(&scif_dev[ms_info.mi_nodeid]);

	/* Close DMA device */
	close_dma_device(0, &mic_dma_handle);

	micscif_uninit_qp(&scif_dev[SCIF_HOST_NODE]);
	iounmap(scif_dev[SCIF_HOST_NODE].mm_sbox);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34))
	pm_qos_remove_requirement(PM_QOS_CPU_DMA_LATENCY, "micscif");
#endif
}

static void micscif_exit(void)
{
	unregister_reboot_notifier(&micscif_reboot_notifier);
	_micscif_exit();
}

static int micscif_reboot(struct notifier_block *notifier, unsigned long unused1, void *unused2)
{
	_micscif_exit();
	return NOTIFY_OK;
}

struct file_operations micscif_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = micscif_unlocked_ioctl,
	.mmap = micscif_mmap,
	.poll = micscif_poll,
	.flush = micscif_flush,
	.open  = micscif_open,
	.release = micscif_release,
};

static char * scif_devnode(struct device *dev, mode_t *mode)
{
	return kasprintf(GFP_KERNEL, "mic/%s", dev_name(dev));
}

// Setup the base informaiton for the driver.  No interface specific code.
static int micscif_setup_base(void)
{
	long int result;

	if (micscif_major) {
		micinfo.m_dev = MKDEV(micscif_major, micscif_minor);
		result = register_chrdev_region(micinfo.m_dev, 2, "micscif");
	} else {
		result = alloc_chrdev_region(&micinfo.m_dev, micscif_minor, 2, "micscif");
		micscif_major = MAJOR(micinfo.m_dev);
	}

	if (result >= 0) {
		cdev_init(&(micinfo.m_cdev), &micscif_ops);
		micinfo.m_cdev.owner = THIS_MODULE;
		if ((result = cdev_add(&(micinfo.m_cdev), micinfo.m_dev, 2)))
			goto unreg_chrdev;
	} else {
		goto unreg_chrdev;
	}

	micinfo.m_class = class_create(THIS_MODULE, "micscif");
	if (IS_ERR(micinfo.m_class)) {
		result = PTR_ERR(micinfo.m_class);
		goto del_m_dev;
	}

	micinfo.m_class->devnode = scif_devnode;
	if (IS_ERR((int *)(result =
		(long int)device_create(micinfo.m_class, NULL, micinfo.m_dev, NULL, "mic")))) {
		result = PTR_ERR((int *)result);
		goto class_destroy;
	}
	if (IS_ERR(micinfo.m_scifdev =
		device_create(micinfo.m_class, NULL, micinfo.m_dev + 1, NULL, "scif"))) {
		result = PTR_ERR(micinfo.m_scifdev);
		goto device_destroy;
	}
	if ((result = sysfs_create_group(&micinfo.m_scifdev->kobj, &scif_attr_group)))
		goto device_destroy1;

	spin_lock_init(&ms_info.mi_eplock);
	spin_lock_init(&ms_info.mi_connlock);
	spin_lock_init(&ms_info.mi_rmalock);
	mutex_init(&ms_info.mi_fencelock);
	spin_lock_init(&ms_info.mi_nb_connect_lock);
	INIT_LIST_HEAD(&ms_info.mi_uaccept);
	INIT_LIST_HEAD(&ms_info.mi_listen);
	INIT_LIST_HEAD(&ms_info.mi_zombie);
	INIT_LIST_HEAD(&ms_info.mi_connected);
	INIT_LIST_HEAD(&ms_info.mi_disconnected);
	INIT_LIST_HEAD(&ms_info.mi_rma);
	INIT_LIST_HEAD(&ms_info.mi_rma_tc);
	INIT_LIST_HEAD(&ms_info.mi_nb_connect_list);

#ifdef CONFIG_MMU_NOTIFIER
	INIT_LIST_HEAD(&ms_info.mi_mmu_notif_cleanup);
#endif
	INIT_LIST_HEAD(&ms_info.mi_fence);
	if (!(ms_info.mi_misc_wq = create_singlethread_workqueue("SCIF_MISC"))) {
		result = -ENOMEM;
		goto remove_group;
	}
	INIT_WORK(&ms_info.mi_misc_work, micscif_misc_handler);
	if (!(ms_info.mi_conn_wq = create_singlethread_workqueue("SCIF_NB_CONN"))) {
		result = -ENOMEM;
		goto destroy_misc_wq;
	}
	INIT_WORK(&ms_info.mi_conn_work, micscif_conn_handler);
#ifdef CONFIG_MMU_NOTIFIER
	if (!(ms_info.mi_mmu_notif_wq = create_singlethread_workqueue("SCIF_MMU"))) {
		result = -ENOMEM;
		goto destroy_conn_wq;
	}
	INIT_WORK(&ms_info.mi_mmu_notif_work, micscif_mmu_notif_handler);
#endif
	ms_info.mi_watchdog_to = DEFAULT_WATCHDOG_TO;
#ifdef MIC_IS_EMULATION
	ms_info.mi_watchdog_enabled = 0;
#else
	ms_info.mi_watchdog_enabled = 1;
#endif
	ms_info.mi_rma_tc_limit = SCIF_RMA_TEMP_CACHE_LIMIT;
	ms_info.mi_proxy_dma_threshold = mic_p2p_proxy_thresh;
	ms_info.en_msg_log = 0;
	return result;
#ifdef CONFIG_MMU_NOTIFIER
destroy_conn_wq:
	destroy_workqueue(ms_info.mi_conn_wq);
#endif
destroy_misc_wq:
	destroy_workqueue(ms_info.mi_misc_wq);
remove_group:
	sysfs_remove_group(&micinfo.m_scifdev->kobj, &scif_attr_group);
device_destroy1:
	device_destroy(micinfo.m_class, micinfo.m_dev + 1);
device_destroy:
	device_destroy(micinfo.m_class, micinfo.m_dev);
class_destroy:
	class_destroy(micinfo.m_class);
del_m_dev:
	cdev_del(&(micinfo.m_cdev));
unreg_chrdev:
	unregister_chrdev_region(micinfo.m_dev, 2);
//error:
	return result;
}

#define SBOX_MMIO_LENGTH 0x10000

static int micscif_init(void)
{
	int result = 0;
	int i;
	phys_addr_t host_queue_phys;
	phys_addr_t gtt_phys_base;

	pr_debug("HELLO SCIF!\n");

#if defined(CONFIG_ML1OM)
	pr_debug("micscif_init(): Hello KNF!\n");
#elif defined(CONFIG_MK1OM)
	pr_debug("micscif_init(): Hello KNC!\n");
#endif

	if (!scif_id || !scif_addr) {
		printk(KERN_ERR "%s %d scif_id 0x%x scif_addr 0x%lx"
			"not provided as module parameter. Fail module load",
			__func__, __LINE__, scif_id, scif_addr);
		return -EINVAL;
	}

	for (i = 1; i <= MAX_BOARD_SUPPORTED; i++) {
		scif_dev[i].sd_state = SCIFDEV_INIT;
		scif_dev[i].sd_node = i;
		scif_dev[i].sd_numa_node = -1;
		mutex_init (&scif_dev[i].sd_lock);
		init_waitqueue_head(&scif_dev[i].sd_mmap_wq);
		init_waitqueue_head(&scif_dev[i].sd_wq);
		init_waitqueue_head(&scif_dev[i].sd_p2p_wq);
		INIT_DELAYED_WORK(&scif_dev[i].sd_p2p_dwork,
			scif_poll_qp_state);
		scif_dev[i].sd_p2p_retry = 0;
	}

	// Setup the host node access information
	// Initially only talks to the host => node 0
	scif_dev[SCIF_HOST_NODE].sd_node = SCIF_HOST_NODE;
	scif_dev[SCIF_HOST_NODE].sd_state = SCIFDEV_RUNNING;
	if (!(scif_dev[SCIF_HOST_NODE].mm_sbox =
		ioremap_nocache(SBOX_BASE, SBOX_MMIO_LENGTH))) {
		result = -ENOMEM;
		goto error;
	}
	scif_dev[SCIF_HOST_NODE].scif_ref_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
	scif_dev[SCIF_HOST_NODE].scif_map_ref_cnt = 0;
	init_waitqueue_head(&scif_dev[SCIF_HOST_NODE].sd_wq);
	init_waitqueue_head(&scif_dev[SCIF_HOST_NODE].sd_mmap_wq);
	mutex_init(&scif_dev[SCIF_HOST_NODE].sd_lock);
	gtt_phys_base = readl(scif_dev[SCIF_HOST_NODE].mm_sbox + SBOX_GTT_PHY_BASE);
	gtt_phys_base *= ((4) * 1024);
	pr_debug("GTT PHY BASE in GDDR 0x%llx\n", gtt_phys_base);
	pr_debug("micscif_init(): gtt_phy_base x%llx\n", gtt_phys_base);

	/* Get handle to DMA device */
	if ((result = open_dma_device(0, 0, &mic_dma_handle)))
		goto unmap_sbox;

	ms_info.mi_nodeid = scif_id;
	ms_info.mi_maxid = scif_id;
	ms_info.mi_total = 2;	// Host plus this card

#ifdef RMA_DEBUG
	ms_info.rma_unaligned_cpu_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
	ms_info.rma_alloc_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
	ms_info.rma_pin_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
#ifdef CONFIG_MMU_NOTIFIER
	ms_info.mmu_notif_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
#endif
#endif

	pr_debug("micscif_init(): setup_card_qp \n");
	host_queue_phys = scif_addr;
	mutex_init(&ms_info.mi_event_cblock);
	mutex_init(&ms_info.mi_conflock);
	INIT_LIST_HEAD(&ms_info.mi_event_cb);

	pr_debug("micscif_init(): setup_interrupts \n");
	/*
	 * Set up the workqueue thread for interrupt handling
	 */
	if ((result = micscif_setup_interrupts(&scif_dev[SCIF_HOST_NODE])))
		goto close_dma;

	pr_debug("micscif_init(): host_intr_handler \n");
	if ((result = micscif_setup_card_qp(host_queue_phys, &scif_dev[SCIF_HOST_NODE]))) {
		if (result == -ENXIO)
			goto uninit_qp;
		else
			goto destroy_intr_wq;
	}
	/* need to do this last -- as soon as the dev is setup, userspace
	 * can try to use the device
	 */
	pr_debug("micscif_init(): setup_base \n");
	if ((result = micscif_setup_base()))
		goto uninit_qp;
	/*
	 * Register the interrupt
	 */
	if ((result = register_scif_intr_handler(&scif_dev[SCIF_HOST_NODE])))
		goto destroy_base;

	// Setup information for self aka loopback.
	scif_dev[ms_info.mi_nodeid].sd_node = ms_info.mi_nodeid;
	scif_dev[ms_info.mi_nodeid].sd_numa_node = mic_host_numa_node;
	scif_dev[ms_info.mi_nodeid].mm_sbox = scif_dev[SCIF_HOST_NODE].mm_sbox;
	scif_dev[ms_info.mi_nodeid].scif_ref_cnt = (atomic_long_t) ATOMIC_LONG_INIT(0);
	scif_dev[ms_info.mi_nodeid].scif_map_ref_cnt = 0;
	init_waitqueue_head(&scif_dev[ms_info.mi_nodeid].sd_wq);
	init_waitqueue_head(&scif_dev[ms_info.mi_nodeid].sd_mmap_wq);
	mutex_init(&scif_dev[ms_info.mi_nodeid].sd_lock);
	if ((result = micscif_setup_loopback_qp(&scif_dev[ms_info.mi_nodeid])))
		goto dereg_intr_handle;
	scif_dev[ms_info.mi_nodeid].sd_state = SCIFDEV_RUNNING;

	unaligned_cache = micscif_kmem_cache_create();
	if (!unaligned_cache) {
		result = -ENOMEM;
		goto destroy_loopb;
	}
	scif_proc_init();
	mic_debug_init();

	pr_debug("micscif_init(): Setup successful: 0x%llx \n", host_queue_phys);

#ifdef CONFIG_MK1OM
	micpm_device_register(&mic_deviceevent);
#endif
	if ((result = register_reboot_notifier(&micscif_reboot_notifier)))
		goto cache_destroy;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34))
	result = pm_qos_add_requirement(PM_QOS_CPU_DMA_LATENCY, "micscif", mic_pm_qos_cpu_dma_lat);
	if (result) {
		printk("%s %d mic_pm_qos_cpu_dma_lat %d result %d\n",
			__func__, __LINE__, mic_pm_qos_cpu_dma_lat, result);
		result = 0;
		/* Dont fail driver load due to PM QoS API. Fall through */
	}
#endif

	return result;
cache_destroy:
#ifdef CONFIG_MK1OM
	micpm_device_unregister(&mic_deviceevent);
#endif
	micscif_kmem_cache_destroy();
destroy_loopb:
	micscif_destroy_loopback_qp(&scif_dev[ms_info.mi_nodeid]);
dereg_intr_handle:
	deregister_scif_intr_handler(&scif_dev[SCIF_HOST_NODE]);
destroy_base:
	pr_debug("Unable to finish scif setup for some reason: %d\n", result);
	micscif_destroy_base();
uninit_qp:
	micscif_uninit_qp(&scif_dev[SCIF_HOST_NODE]);
destroy_intr_wq:
	micscif_destroy_interrupts(&scif_dev[SCIF_HOST_NODE]);
close_dma:
	close_dma_device(0, &mic_dma_handle);
unmap_sbox:
	iounmap(scif_dev[SCIF_HOST_NODE].mm_sbox);
error:
	return result;
}

module_init(micscif_init);
module_exit(micscif_exit);

module_param_named(huge_page, mic_huge_page_enable, bool, 0600);
MODULE_PARM_DESC(huge_page, "SCIF Huge Page Support");

module_param_named(ulimit, mic_ulimit_check, bool, 0600);
MODULE_PARM_DESC(ulimit, "SCIF ulimit check");

module_param_named(reg_cache, mic_reg_cache_enable, bool, 0600);
MODULE_PARM_DESC(reg_cache, "SCIF registration caching");
module_param_named(p2p, mic_p2p_enable, bool, 0600);
MODULE_PARM_DESC(p2p, "SCIF peer-to-peer");

module_param_named(p2p_proxy, mic_p2p_proxy_enable, bool, 0600);
MODULE_PARM_DESC(p2p_proxy, "SCIF peer-to-peer proxy DMA support");

module_param_named(pm_qos_cpu_dma_lat, mic_pm_qos_cpu_dma_lat, int, 0600);
MODULE_PARM_DESC(pm_qos_cpu_dma_lat, "PM QoS CPU DMA latency in usecs.");

module_param_named(numa_node, mic_host_numa_node, int, 0600);
MODULE_PARM_DESC(numa_node, "Host Numa node to which MIC is attached");

module_param_named(p2p_proxy_thresh, mic_p2p_proxy_thresh, ulong, 0600);
MODULE_PARM_DESC(numa_node, "Transfer size after which Proxy DMA helps DMA perf");

MODULE_LICENSE("GPL");
MODULE_INFO(build_number, BUILD_NUMBER);
MODULE_INFO(build_bywhom, BUILD_BYWHOM);
MODULE_INFO(build_ondate, BUILD_ONDATE);
MODULE_INFO(build_scmver, BUILD_SCMVER);
