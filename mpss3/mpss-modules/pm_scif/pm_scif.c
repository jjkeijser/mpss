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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <scif.h>
#include <mic/mic_pm.h>
#include <mic/micscif.h>
#include "pm_scif.h"

#define PM_DB(fmt, ...) printk(KERN_ALERT"[ %s : %d ]:"fmt,__func__, __LINE__, ##__VA_ARGS__)
#define FUNCTION_ENTRY PM_DB("==> %s\n", __func__)
#define FUNCTION_EXIT PM_DB("<== %s\n", __func__)

#define PM_SCIF_RETRY_COUNT 5

DEFINE_RWLOCK(pmscif_send);

static atomic_t epinuse = ATOMIC_INIT(0);
void pm_scif_exit(void);

typedef struct _mic_pm_scif {
	scif_epd_t ep;
	int lport;
	struct scif_portID rport_id;
	struct workqueue_struct *pm_recvq;
	struct work_struct pm_recv;
	PM_CONNECTION_STATE con_state;
} mic_pm_scif;

mic_pm_scif *pm_scif;

void
pm_dump(char *buf, size_t len)
{
	int i = 0;

	for ( i=0; i < len; i++) {

		if (i % 8)
			printk(KERN_ALERT"\n");
		printk(KERN_ALERT"%x ", buf[i]);
	}
}

static void pm_handle_open (void *msg, size_t len)
{
	FUNCTION_ENTRY;
	pm_dump((char*)msg, len);
}

static void pm_handle_test (void *msg, size_t len)
{
	FUNCTION_ENTRY;
	pm_dump((char*)msg, len);

}
typedef void (*_pm_msg_handler)(void*, size_t);

typedef struct _pm_msg_call {
	_pm_msg_handler handler;
	char *name;
}pm_msg_call;

#define PM_HANDLE_ADD(opcode, function) [(opcode)] = {(function), #function}

pm_msg_call pm_msg_caller[PM_MESSAGE_MAX] = {
	 PM_HANDLE_ADD(PM_MESSAGE_OPEN, pm_handle_open),
	 PM_HANDLE_ADD(PM_MESSAGE_TEST, pm_handle_test)
};

int
pm_send_to_host(PM_MESSAGE opcode, void *msg, size_t len)
{
//	FUNCTION_ENTRY;
	int err = 0;
	size_t psize = sizeof(pm_msg_header) + len;
	char *payload;
	unsigned long flags;

	if (pm_scif->con_state != PM_CONNECTED) {
		err = -EINVAL;
		goto error;
	}

	if (!(payload = kmalloc(psize, GFP_ATOMIC))) {
		err = -ENOMEM;
		goto error;
	}
	read_lock_irqsave(&pmscif_send,flags);

	if (atomic_xchg(&epinuse,1) != 0) {
		read_unlock_irqrestore(&pmscif_send,flags);
		kfree(payload);
		return -1;
	}

	((pm_msg_header*)payload)->opcode = opcode;
	((pm_msg_header*)payload)->len = len;
	if (len)
		memcpy((char*)payload + sizeof(pm_msg_header), msg, len);

	//0 for non blocking
	if ((err = scif_send(pm_scif->ep, payload, psize, 0)) < 0) {
		PM_DB("scif_recv failed\n");
	}
	atomic_set(&epinuse,0);
	//for (i = 0; i < psize; i++)
	//	printk(KERN_ALERT" buff: %X\n", payload[i]);
	read_unlock_irqrestore(&pmscif_send,flags);
	kfree(payload);
//	FUNCTION_EXIT;
error:
	return err;
}

EXPORT_SYMBOL(pm_send_to_host);

static struct mic_pmscif_handle micpmscif = {
	.pm_scif_uos2host = pm_send_to_host,
	.pm_scif_host2uos = NULL,
	.owner = THIS_MODULE,
};



static void pm_send_to_uos(pm_msg_header *header, char *msg)
{
	if(micpmscif.pm_scif_host2uos) {
		micpmscif.pm_scif_host2uos(header, msg);
	}
}

static void
pm_recv_from_host(struct work_struct *work)
{
	int err = 0;
	char *msg = NULL;
	pm_msg_header *header;
	mic_pm_scif *pm_scif_info = container_of(work, mic_pm_scif, pm_recv);

	FUNCTION_ENTRY;
	if (pm_scif->con_state != PM_CONNECTED)
		goto exit;

	header = kmalloc(sizeof(pm_msg_header), GFP_KERNEL);

	if ((err = scif_recv(pm_scif_info->ep, header, sizeof(pm_msg_header),
							SCIF_RECV_BLOCK)) < 0) {
		PM_DB("scif_recv failed\n");
		goto end_con;
	}

	msg = kmalloc(header->len, GFP_KERNEL);

	if ((err = scif_recv(pm_scif_info->ep, msg, header->len,
							SCIF_RECV_BLOCK)) < 0) {
		PM_DB("scif_recv failed\n");
		goto end_con;
	}
	if(header->opcode < PM_MESSAGE_MAX) {
		if ((header->opcode != PM_MESSAGE_CLOSE) &&
				(header->opcode != PM_MESSAGE_CLOSE_ACK)) {
			if(pm_msg_caller[header->opcode].handler)
				pm_msg_caller[header->opcode].handler(msg, header->len);
			pm_send_to_uos(header, msg);
		} else {
			if (header->opcode == PM_MESSAGE_CLOSE) {
				pm_send_to_uos(header,msg);
				pm_send_to_host(PM_MESSAGE_CLOSE_ACK, NULL, 0);
			}
			pm_scif->con_state = PM_DISCONNECTING;
			goto end_con;
		}
	}
	else
		printk("pm_scif: Recvd scif message with bad opcode %d\n",
			header->opcode);
	kfree(header);
	kfree(msg);
	queue_work(pm_scif->pm_recvq, &pm_scif->pm_recv);
	return;

end_con:
	kfree(header);
	kfree(msg);
exit:
	FUNCTION_EXIT;
}

#ifdef PM_SCIF_IOCTL
static int
spm_ioctl(struct inode *in, struct file *f, unsigned int cmd, unsigned long arg)
{
	int i = 0;
	uint32_t payload = 0xc0de0000;

	FUNCTION_ENTRY;
	for (i = 0; i < PM_MESSAGE_TEST; i++) {
		payload++;
		//PM_DB("sending %s with payload = %x \n",
		//	 pm_msg_caller[i].name, payload);
		pm_send_to_host(i, &payload, sizeof(payload));
	}

	return 0;
}

static long
spm_unlocked_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	return (long) spm_ioctl(f->f_path.dentry->d_inode, f, cmd, arg);
}

static int
spm_release(struct inode *in, struct file *f)
{
	return 0;
}

static char *
spm_devnode(struct device *dev, mode_t *mode)
{
	return kasprintf(GFP_KERNEL, "spm/%s", dev_name(dev));
}


static int
spm_open(struct inode *in, struct file *f)
{
	return 0;
}

struct file_operations spm_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = spm_unlocked_ioctl,
	.open  = spm_open,
	.release = spm_release,
};

int spm_major;
int spm_minor;
dev_t spmdev;
struct cdev spmcdev;
struct class *spmclass;

static void
spm_dev_deinit(void)
{
	device_destroy(spmclass,spmdev);
	class_destroy(spmclass);
	cdev_del(&spmcdev);
	unregister_chrdev_region(spmdev, 1);
}

static int
spm_dev_init(void)
{
	int err = 0;

	if (spm_major) {
		spmdev = MKDEV(spm_major, spm_minor);
		err = register_chrdev_region(spmdev, 1, "spm");
	}
	else {
		err = alloc_chrdev_region(&spmdev, spm_minor, 1, "spm");
		spm_major = MAJOR(spmdev);
	}

	if (err < 0) {
		unregister_chrdev_region(spmdev, 1);
		goto done;
	}

	spmdev = MKDEV(spm_major, spm_minor);
	cdev_init(&spmcdev, &spm_ops);
	spmcdev.owner = THIS_MODULE;
	err = cdev_add(&spmcdev, spmdev, 1);

	if (err)
		goto err;

	spmclass = class_create(THIS_MODULE, "spm");
	if (IS_ERR(spmclass)) {
		err = PTR_ERR(spmclass);
		goto err;
	}

	spmclass->devnode = spm_devnode;
	device_create(spmclass, NULL, spmdev, NULL, "spm");
	if (IS_ERR(spmclass)) {
		err = PTR_ERR(spmclass);
		goto err;
	}
done:
	return err;
err:
	spm_dev_deinit();
	return err;
}
#endif

int pm_scif_init(void)
{
	int err = 1;
	int retry = 0;

	FUNCTION_ENTRY;
	PM_DB("pm_scif insmoded \n");
#ifdef PM_SCIF_IOCTL
	if ((err = spm_dev_init())) {
		PM_DB(" spm_dev_init failed\n");
		goto done;
	}
#endif
	atomic_set(&epinuse,0);
	pm_scif = kzalloc(sizeof(mic_pm_scif), GFP_KERNEL);

	if (!pm_scif) {
		err = -ENOMEM;
		goto end_con;
	}

	pm_scif_register(&micpmscif);

	if ((pm_scif->ep = scif_open()) == NULL) {
		PM_DB(" scif_open failed\n");
		goto end_con;
	}

	if ((pm_scif->lport = scif_bind(pm_scif->ep, 0)) < 0) {
		PM_DB(" scif_bind failed\n");
		goto end_con;
	}

	PM_DB(" scif_bind successfull. Local port number = %d, ep =  \n",
							 pm_scif->lport);
	dump_ep(pm_scif->ep, __func__,__LINE__);
	pm_scif->rport_id.node = 0;
	pm_scif->rport_id.port = SCIF_PM_PORT_0;

	while ((err = scif_connect(pm_scif->ep, &pm_scif->rport_id)) != 0) {
		PM_DB(" scif_connect failed with err = %d ep %p\n",err,
			pm_scif->ep);
		msleep(1000);
		if (retry++ > PM_SCIF_RETRY_COUNT)
			goto end_con;
	}

	pm_scif->pm_recvq = create_singlethread_workqueue("pm_recvq");
	INIT_WORK(&pm_scif->pm_recv, pm_recv_from_host);
	queue_work(pm_scif->pm_recvq, &pm_scif->pm_recv);
	pm_scif->con_state = PM_CONNECTED;
	err = 0;
#ifdef PM_SCIF_IOCTL
done:
#endif
	return err;
end_con:
	pm_scif_exit();
	FUNCTION_EXIT;
	return err;
}
EXPORT_SYMBOL(pm_scif_init);

void pm_scif_exit(void)
{
	unsigned long flags;

	FUNCTION_ENTRY;
	PM_DB("Good Bye!, pm scif \n");

	pm_send_to_host(PM_MESSAGE_CLOSE, NULL, 0);
	write_lock_irqsave(&pmscif_send,flags);
	atomic_set(&epinuse,1);
	write_unlock_irqrestore(&pmscif_send,flags);

	if (pm_scif) {
		if(pm_scif->pm_recvq) {
			flush_workqueue(pm_scif->pm_recvq);
			PM_DB("calling destroy\n");
			destroy_workqueue(pm_scif->pm_recvq);
		}

		PM_DB("closing ep \n");
		if (pm_scif->ep)
			scif_close(pm_scif->ep);

		pm_scif_unregister(&micpmscif);
		pm_scif->con_state = PM_DISCONNECTED;
		kfree(pm_scif);
	}
	#ifdef PM_SCIF_IOCTL
	spm_dev_deinit();
	#endif
	FUNCTION_EXIT;
}

EXPORT_SYMBOL(pm_scif_exit);

module_init(pm_scif_init);
module_exit(pm_scif_exit);
MODULE_LICENSE("GPL");
