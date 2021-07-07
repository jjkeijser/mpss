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

#include "micint.h"

/* TODO: Improve debug messages */

static int micvcons_open(struct tty_struct * tty, struct file * filp);
static void micvcons_close(struct tty_struct * tty, struct file * filp);
static int micvcons_write(struct tty_struct * tty, const unsigned char *buf, 
								int count);
static int micvcons_write_room(struct tty_struct *tty);
static void micvcons_set_termios(struct tty_struct *tty, struct ktermios * old);
static void micvcons_timeout(unsigned long);
static void micvcons_throttle(struct tty_struct *tty);
static void micvcons_unthrottle(struct tty_struct *tty);
static void micvcons_wakeup_readbuf(struct work_struct *work);
static int micvcons_resume(struct _mic_ctx_t *mic_ctx);

static struct tty_operations micvcons_tty_ops = {
	.open = micvcons_open,
	.close = micvcons_close,
	.write = micvcons_write,
	.write_room = micvcons_write_room,
	.set_termios = micvcons_set_termios,
	.throttle = micvcons_throttle,
	.unthrottle = micvcons_unthrottle,
};

static struct tty_driver *micvcons_tty = NULL;
static u16 extra_timeout = 0;
static u8 restart_timer_flag = MICVCONS_TIMER_RESTART;
static struct timer_list vcons_timer;
static struct list_head timer_list_head;
static spinlock_t timer_list_lock;

int
micvcons_create(int num_bds)
{
	micvcons_port_t *port;
	bd_info_t *bd_info;
	int bd, ret = 0;
	char wq_name[14];
	struct device *dev;

	INIT_LIST_HEAD(&timer_list_head);

	if (micvcons_tty)
		goto exit;

	micvcons_tty = alloc_tty_driver(num_bds);
	if (!micvcons_tty) {
		ret = -ENOMEM;
		goto exit;
	}
	micvcons_tty->owner = THIS_MODULE;
	micvcons_tty->driver_name = MICVCONS_DEVICE_NAME;
	micvcons_tty->name = MICVCONS_DEVICE_NAME;
	micvcons_tty->major = 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
	micvcons_tty->minor_num = num_bds;
#endif
	micvcons_tty->minor_start = 0;
	micvcons_tty->type = TTY_DRIVER_TYPE_SERIAL;
	micvcons_tty->subtype = SERIAL_TYPE_NORMAL;
	micvcons_tty->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	micvcons_tty->init_termios = tty_std_termios;
	micvcons_tty->init_termios.c_iflag = IGNCR;
	micvcons_tty->init_termios.c_oflag = 0;
	micvcons_tty->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	micvcons_tty->init_termios.c_lflag = 0;

	tty_set_operations(micvcons_tty, &micvcons_tty_ops);

	if ((ret = tty_register_driver(micvcons_tty)) != 0) {
		printk("Failed to register vcons tty driver\n");
		put_tty_driver(micvcons_tty);
		micvcons_tty = NULL;
		goto exit;
	}

	for (bd = 0; bd < num_bds; bd++) {
		port = &mic_data.dd_ports[bd];
		port->dp_bdinfo = mic_data.dd_bi[bd];

		spin_lock_init(&port->dp_lock);
		mutex_init (&port->dp_mutex);

		bd_info = (bd_info_t *)port->dp_bdinfo;
		bd_info->bi_port = port;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
		tty_port_init(&port->port);
		dev = tty_port_register_device(&port->port, micvcons_tty, bd, NULL);
#else
		dev = tty_register_device(micvcons_tty, bd, NULL);
		if (IS_ERR(dev)) {
			printk("Failed to register vcons tty device\n");
			micvcons_destroy(bd);
			ret = PTR_ERR(dev);
			goto exit;
		}
#endif
		snprintf(wq_name, sizeof(wq_name), "VCONS MIC %d", bd);
		port->dp_wq = __mic_create_singlethread_workqueue(wq_name);
		if (!port->dp_wq) {
			printk(KERN_ERR "%s: create_singlethread_workqueue\n", 
								__func__);
			tty_unregister_device(micvcons_tty, bd);
			micvcons_destroy(bd);
			ret = -ENOMEM;
			goto exit;
		}
		INIT_WORK(&port->dp_wakeup_read_buf, micvcons_wakeup_readbuf);
	}
	vcons_timer.function = micvcons_timeout;
	vcons_timer.data = (unsigned long)(&timer_list_head);
	init_timer(&vcons_timer);
exit:
	return ret;
}

void micvcons_destroy(int num_bds)
{
	int bd, ret;
	micvcons_port_t *port;

	if (!micvcons_tty)
		return;
	for (bd = 0; bd < num_bds; bd++) {
		port = &mic_data.dd_ports[bd];
		destroy_workqueue(port->dp_wq);
		tty_unregister_device(micvcons_tty, bd);
	}
	ret = tty_unregister_driver(micvcons_tty);
	put_tty_driver(micvcons_tty);
	micvcons_tty = NULL;

	if (ret)
		printk(KERN_ERR "tty unregister_driver failed with code %d\n", ret);
}

static int
micvcons_open(struct tty_struct * tty, struct file * filp)
{
	micvcons_port_t *port = &mic_data.dd_ports[tty->index];
	int ret = 0;
	mic_ctx_t *mic_ctx = get_per_dev_ctx(tty->index);

	tty->driver_data = port;

	mutex_lock(&port->dp_mutex);
	spin_lock_bh(&port->dp_lock);

	if ((filp->f_flags & O_ACCMODE) != O_RDONLY) {
		if (port->dp_writer) {
			ret = -EBUSY;
			goto exit_locked;
		}
		port->dp_writer = filp;
		port->dp_bytes = 0;
	}

	if ((filp->f_flags & O_ACCMODE) != O_WRONLY) {
		if (port->dp_reader) {
			ret = -EBUSY;
			goto exit_locked;
		}
		port->dp_reader = filp;
		port->dp_canread = 1;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
	tty->low_latency = 0;
#endif

	if (!port->dp_tty)
		port->dp_tty = tty;
	if (!port->dp_vcons)
		port->dp_vcons = &mic_ctx->bi_vcons;
	if (tty->count == 1) {
		ret = micvcons_start(mic_ctx);
		if (ret != 0)
			goto exit_locked;
		spin_lock(&timer_list_lock);
		list_add_tail_rcu(&port->list_member, &timer_list_head);
		if (list_is_singular(&timer_list_head)) {
			restart_timer_flag = MICVCONS_TIMER_RESTART;
			mod_timer(&vcons_timer, jiffies + 
				msecs_to_jiffies(MICVCONS_SHORT_TIMEOUT));
		}
		spin_unlock(&timer_list_lock);
	}

exit_locked:
	spin_unlock_bh(&port->dp_lock);
	mutex_unlock(&port->dp_mutex);
	return ret;
}

static inline void
micvcons_del_timer_entry(micvcons_port_t *port)
{
	spin_lock(&timer_list_lock);
	list_del_rcu(&port->list_member);
	if (list_empty(&timer_list_head)) {
		restart_timer_flag = MICVCONS_TIMER_SHUTDOWN;
		spin_unlock(&timer_list_lock);
		del_timer_sync(&vcons_timer);
	} else {
		spin_unlock(&timer_list_lock);
	}
	synchronize_rcu();
}

static void
micvcons_close(struct tty_struct * tty, struct file * filp)
{
	micvcons_port_t *port = (micvcons_port_t *)tty->driver_data;

	mutex_lock(&port->dp_mutex);
	if (tty->count == 1) {
		micvcons_del_timer_entry(port);
		flush_workqueue(port->dp_wq);
	}
	spin_lock_bh(&port->dp_lock);
	if (port->dp_reader == filp)
		port->dp_reader = 0;

	if (port->dp_writer == filp)
		port->dp_writer = 0;

	if (tty->count == 1)
		port->dp_tty = 0;
	spin_unlock_bh(&port->dp_lock);
	mutex_unlock(&port->dp_mutex);
}

static int
micvcons_write(struct tty_struct * tty, const unsigned char *buf, int count)
{
	micvcons_port_t *port = (micvcons_port_t *)tty->driver_data;
	mic_ctx_t *mic_ctx = get_per_dev_ctx(tty->index);
	int bytes=0, status;
	struct vcons_buf *vcons_host_header;
	u8 card_alive = 1;

	spin_lock_bh(&port->dp_lock);
	vcons_host_header = (struct vcons_buf *)port->dp_vcons->dc_hdr_virt;
	if (vcons_host_header->mic_magic == MIC_VCONS_SLEEPING) {
		status = micvcons_resume(mic_ctx);
		if (status != 0) {
			/* If card can not wakeup, it is dead. */
			card_alive = 0;
			goto exit;
		}
	}
	if (vcons_host_header->mic_magic != MIC_VCONS_READY)
		goto exit;
	bytes = micvcons_port_write(port, buf, count);
	if (bytes) {
		mic_send_hvc_intr(mic_ctx);
		extra_timeout = 0;
	}
exit:
	spin_unlock_bh(&port->dp_lock);
	if (!card_alive)
		micvcons_del_timer_entry(port);
	return bytes;
}

static int
micvcons_write_room(struct tty_struct *tty)
{
	micvcons_port_t *port = (micvcons_port_t *)tty->driver_data;
	int room;

	spin_lock_bh(&port->dp_lock);
	if (port->dp_out)
		room = micscif_rb_space(port->dp_out);
	else
		room = 0;
	spin_unlock_bh(&port->dp_lock);

	return room;
}

static void
micvcons_set_termios(struct tty_struct *tty, struct ktermios * old)
{
}

static int
micvcons_readchars(micvcons_port_t *port)
{
	int len, ret, get_count;
	int bytes_total = 0;
	int bytes_read = 0;
	char buf[64];

	for (;;) {
		len = micscif_rb_count(port->dp_in, sizeof(buf));
		if (!len)
			break;
		get_count = min(len, (int)sizeof(buf));
		ret = micscif_rb_get_next(port->dp_in, buf, get_count);
		micscif_rb_update_read_ptr(port->dp_in);
		if (port->dp_reader && port->dp_canread) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
			if ((bytes_read = tty_insert_flip_string(
					&port->port, buf, get_count)) != 0)
				tty_flip_buffer_push(&port->port);
#else
			bytes_read = tty_insert_flip_string(port->dp_tty, 
								buf, get_count);
			tty_flip_buffer_push(port->dp_tty);
#endif
			bytes_total += bytes_read;
			if (bytes_read != get_count) {
				printk(KERN_WARNING "dropping characters: \
						bytes_read %d, get_count %d\n",
						bytes_read, get_count);
				break;
			}
		}
	}
	return bytes_total;
}

static int
micvcons_initport(micvcons_port_t *port)
{
	struct vcons_buf *vcons_host_header;
	struct vcons_mic_header  *vcons_mic_header;
	char *mic_hdr, *mic_buf, *host_buf;

	vcons_host_header = (struct vcons_buf *)port->dp_vcons->dc_hdr_virt;
	if (!vcons_host_header) {
		printk(KERN_ERR "vcons_host_header NULL\n");
		return -EFAULT;
	}

	host_buf = (char *)port->dp_vcons->dc_buf_virt;
	if (!host_buf) {
		printk(KERN_ERR "host_buf NULL\n");
		return -EFAULT;
	}

	if (port->dp_bdinfo->bi_ctx.bi_family == FAMILY_ABR) {
		set_pci_aperture(&port->dp_bdinfo->bi_ctx,
			(port->dp_bdinfo->bi_ctx.aper.len - PAGE_SIZE) >> PAGE_SHIFT,
			vcons_host_header->i_hdr_addr & PAGE_MASK, PAGE_SIZE);
		mic_hdr = port->dp_bdinfo->bi_ctx.aper.va +
			port->dp_bdinfo->bi_ctx.aper.len - PAGE_SIZE;
		mic_buf = mic_hdr + PAGE_SIZE/2;
	} else {
		mic_hdr = port->dp_bdinfo->bi_ctx.aper.va + vcons_host_header->i_hdr_addr;
		mic_buf = port->dp_bdinfo->bi_ctx.aper.va + vcons_host_header->i_buf_addr;
	}

	port->dp_in = kmalloc(sizeof(struct micscif_rb), GFP_ATOMIC);
	if (port->dp_in)
		port->dp_out = kmalloc(sizeof(struct micscif_rb), GFP_ATOMIC);
	else
		return -ENOMEM;

	if (port->dp_out) {
		vcons_mic_header = (struct vcons_mic_header *)mic_hdr;
		micscif_rb_init(port->dp_in,
			&vcons_mic_header->o_rd,
			&vcons_host_header->o_wr,
			host_buf,
			vcons_host_header->o_size);
		micscif_rb_init(port->dp_out, &vcons_host_header->i_rd,
				&vcons_mic_header->i_wr,
				mic_buf,
				vcons_host_header->i_size);
		wmb();
		writel(MIC_VCONS_HOST_OPEN, &vcons_mic_header->host_status);
	} else {
		kfree(port->dp_in);
		return -ENOMEM;
	}
	return 0;
}

static int
micvcons_readport(micvcons_port_t *port)
{
	int num_chars_read = 0, status;
	static uint32_t prev_mic_magic;
	struct vcons_buf *vcons_host_header;

	if (!port || !port->dp_vcons)
		return 0;

	spin_lock_bh(&port->dp_lock);
	if (!port->dp_tty) {
		spin_unlock_bh(&port->dp_lock);
		return 0;
	}

	vcons_host_header = (struct vcons_buf *)port->dp_vcons->dc_hdr_virt;
	if ((vcons_host_header->mic_magic != MIC_VCONS_READY) &&
			(vcons_host_header->mic_magic != MIC_VCONS_SLEEPING)) {
		if ((vcons_host_header->mic_magic == MIC_VCONS_RB_VER_ERR)
			&& (vcons_host_header->mic_magic != prev_mic_magic)) {
			printk(KERN_ERR "Card and host ring buffer versions mismatch.");
			printk(KERN_ERR "Card version: %d, Host version: %d \n", 
						vcons_host_header->mic_rb_ver, 
						vcons_host_header->host_rb_ver);
		}
		goto exit;
	}
	if (!port->dp_in) {
		status = micvcons_initport(port);
		if (status != 0) {
			spin_unlock_bh(&port->dp_lock);
			return status;
		}
	}

	if (port->dp_in) {
		if (vcons_host_header->mic_magic == MIC_VCONS_SLEEPING) {
			/*
			 * If the card is sleeping and there is data in the
			 * buffer, schedule work in a work queue to wake-up
			 * the card and read from the buffer.
			 */
			if (micscif_rb_count(port->dp_in, 1))
				queue_work(port->dp_wq, 
						&port->dp_wakeup_read_buf);
		} else {
			num_chars_read = micvcons_readchars(port);
			tty_wakeup(port->dp_tty);
		}
	}
exit:
	prev_mic_magic = vcons_host_header->mic_magic;
	spin_unlock_bh(&port->dp_lock);
	return num_chars_read;
}

static void
micvcons_wakeup_readbuf(struct work_struct *work)
{
	u8 card_alive = 1;
	int status;
	micvcons_port_t *port;
	struct vcons_buf *vcons_host_header;

	port = container_of(work, micvcons_port_t, dp_wakeup_read_buf);

	vcons_host_header = (struct vcons_buf *)port->dp_vcons->dc_hdr_virt;
	spin_lock_bh(&port->dp_lock);
	status = micvcons_resume(get_per_dev_ctx(port->dp_tty->index));
	if (status == 0) {
		micvcons_readchars(port);
		tty_wakeup(port->dp_tty);
	} else {
		/* If card can not wakeup, it is dead. */
		card_alive = 0;
	}
	spin_unlock_bh(&port->dp_lock);
	if (!card_alive)
		micvcons_del_timer_entry(port);
}

static void
micvcons_timeout(unsigned long data)
{
	struct list_head *timer_list_ptr = (struct list_head *)data;
	micvcons_port_t *port;
	u8 console_active = 0;
	int num_chars_read = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(port, timer_list_ptr, list_member) {
		num_chars_read = micvcons_readport(port);
		if (num_chars_read != 0)
			console_active = 1;
	}
	rcu_read_unlock();

	spin_lock(&timer_list_lock);
	if (restart_timer_flag == MICVCONS_TIMER_RESTART) {
		extra_timeout = (console_active ? 0 :
				extra_timeout + MICVCONS_SHORT_TIMEOUT);
		extra_timeout = min(extra_timeout, (u16)MICVCONS_MAX_TIMEOUT);
		mod_timer(&vcons_timer, jiffies + 
			msecs_to_jiffies(MICVCONS_SHORT_TIMEOUT+extra_timeout));
	}
	spin_unlock(&timer_list_lock);
}

static void
micvcons_throttle(struct tty_struct *tty)
{
	micvcons_port_t *port = (micvcons_port_t *)tty->driver_data;
	port->dp_canread = 0;
}

static void
micvcons_unthrottle(struct tty_struct *tty)
{
	micvcons_port_t *port = (micvcons_port_t *)tty->driver_data;
	port->dp_canread = 1;
}

int micvcons_start(mic_ctx_t *mic_ctx)
{
	struct vcons_buf *vcons_host_header;
	int status;
	micvcons_port_t *port = mic_ctx->bd_info->bi_port;

	vcons_host_header = (struct vcons_buf *)port->dp_vcons->dc_hdr_virt;
	if (vcons_host_header->mic_magic == MIC_VCONS_SLEEPING) {
		status = micvcons_resume(mic_ctx);
		if (status != 0)
			return status;
	}
	if (vcons_host_header->mic_magic == MIC_VCONS_READY) {
		if (!port->dp_in) {
			status = micvcons_initport(port);
			if (status != 0)
				return status;
		}
	}
	return 0;
}

int micvcons_port_write(struct micvcons_port *port, const unsigned char *buf,
			int count)
{
	int ret;
	uint32_t bytes = 0;

	if (port->dp_out) {
		bytes = min(count, micscif_rb_space(port->dp_out));
		ret = micscif_rb_write(port->dp_out, (void *)buf, bytes);
		BUG_ON(ret);
		port->dp_bytes += bytes;
		micscif_rb_commit(port->dp_out);
	}
	return bytes;
}

/**
 * micvcons_stop - cleans up before a node is rebooted
 * @ mic_ctx: node to clean up
 *
 * Called before rebooting a node, reads remaining characters
 * from the node's vcons output buffer, resets the input/output
 * ring buffers so that things work when the node comes up again
 */
void
micvcons_stop(mic_ctx_t *mic_ctx)
{
	micvcons_port_t	*port;
	struct vcons_buf *vcons_host_header;

	port = mic_ctx->bd_info->bi_port;
	micvcons_readport(port);
	spin_lock_bh(&port->dp_lock);
	if (port->dp_in) {
		vcons_host_header = (struct vcons_buf *)port->dp_vcons->dc_hdr_virt;
		vcons_host_header->mic_magic = 0;
		kfree(port->dp_in);
		kfree(port->dp_out);
		port->dp_in = NULL;
		port->dp_out = NULL;
	}
	spin_unlock_bh(&port->dp_lock);
}

/**
 * micvcons_resume - sets the state of a node's console to ready
 * @ mic_ctx: node to clean up
 *
 * @ return: zero if successful.
 * called before resuming a node from PC6. MUST acquire the spinlock
 * port->dp_lock with bottom-halves disabled before calling this function.
 */
static int
micvcons_resume(mic_ctx_t *mic_ctx)
{
	int status = 0;
	micvcons_port_t	*port;
	struct vcons_buf *vcons_host_header;

	port = mic_ctx->bd_info->bi_port;
	vcons_host_header = mic_ctx->bi_vcons.dc_hdr_virt;
	if (vcons_host_header->mic_magic == MIC_VCONS_SLEEPING) {
		do {
			vcons_host_header->mic_magic = MIC_VCONS_WAKINGUP;
			spin_unlock_bh(&port->dp_lock);
			status = micscif_connect_node(mic_get_scifnode_id(mic_ctx), false);
			spin_lock_bh(&port->dp_lock);
		} while ((status == 0) && 
			(vcons_host_header->mic_magic == MIC_VCONS_SLEEPING));
		if (status == 0)
			vcons_host_header->mic_magic = MIC_VCONS_READY;
	}
	return status;
}

/**
 * micvcons_pm_disconnect_node - Check if a card can be put to sleep in case
 * there is any activity on the virtual console. If yes, it also sets the
 * internal state of a node's console to sleeping.
 * @ node_bitmask: bits set indicate which cards to check.
 *		   Bit-1 for the first, Bit-2 for the second,...
 *		   Ignore Bit-0 which indicates host.
 * @ return: bits set indicating which cards can sleep.
 * This is called from PM to check if a card can be put to sleep (PC-6 state).
 * This is called when the node is disconnected from the SCIF network
 * before putting it into the PC6 state where it should no longer
 * receive an PCIe transactions until woken up by the host driver.
 */
int
micvcons_pm_disconnect_node(uint8_t *node_bitmask, enum disconn_type type)
{
	int err = 0;
	if ((type == DISCONN_TYPE_POWER_MGMT) && (node_bitmask)) {
		int i = 0;
		mic_ctx_t *mic_ctx;
		micvcons_port_t	*port;
		struct vcons_buf *vcons_host_header;

		for (i = 0; i <= mic_data.dd_numdevs; i++) {
			if (!get_nodemask_bit(node_bitmask, i))
				continue;

			if (!(mic_ctx = get_per_dev_ctx(i - 1)))
				continue;

			port = mic_ctx->bd_info->bi_port;
			micvcons_readport(port);
			/*
			* If this function is called when virtual console is
			* not active, port->dp_vcons needs to be initialized.
			*/
			if (!port->dp_vcons)
				port->dp_vcons = &mic_ctx->bi_vcons;

			vcons_host_header = (struct vcons_buf *)port->dp_vcons->dc_hdr_virt;
			spin_lock_bh(&port->dp_lock);
			vcons_host_header->mic_magic = MIC_VCONS_SLEEPING;
			spin_unlock_bh(&port->dp_lock);
		}
	}

	return err;
}

