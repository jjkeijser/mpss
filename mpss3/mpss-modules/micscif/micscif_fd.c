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

struct mic_priv {
	scif_epd_t	epd;
};


int
scif_fdopen(struct file *f)
{
	struct mic_priv *priv = (struct mic_priv *)
		kmalloc(sizeof(struct mic_priv), GFP_KERNEL);
	/*
	 * Not a valid errno as defined in scif.h but should be?
	 */
	if (!priv)
		return -ENOMEM;

	/* SCIF device */
	if (!(priv->epd = __scif_open())) {
		kfree(priv);
		return -ENOMEM;
	}

	((f)->private_data) = priv;
	return 0;
}

int
scif_fdclose(struct file *f)
{
	struct mic_priv *priv = ((f)->private_data);
	int err = 0;

	/* Only actually request of tear down of end point if file reference
	 * count is greater than 1.  This accounts for the fork() issue.
	 */
	if (atomic64_read(&f->f_count) == 0) {
		err = __scif_close(priv->epd);
		kfree(priv);
	}
	return err;
}

int
micscif_mmap(struct file *f, struct vm_area_struct *vma)
{
	struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
	return scif_mmap(vma, priv->epd);
}

unsigned int
micscif_poll(struct file *f, poll_table *wait)
{
	struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
	return __scif_pollfd(f, wait, (struct endpt *)priv->epd);
}

int
micscif_flush(struct file *f, fl_owner_t id)
{
	struct mic_priv *priv;
	dev_t dev;
	struct endpt *ep;

	priv = (struct mic_priv *)f->private_data;
	dev = f->f_path.dentry->d_inode->i_rdev;
	if (MINOR(dev) != 1) // SCIF MINOR
		return 0;

	ep = priv->epd;

	/* Handles fork issue, making suer an endpoint only closes when the original
	 * thread that created it tries to close it, or when there are no more
	 * references to it.
	 */
	if (ep->files == id)
		__scif_flush(ep);

	return 0;
}


static __always_inline void
scif_err_debug(int err, const char *str)
{
	/*
	 * ENOTCONN is a common uninteresting error which is
	 * flooding debug messages to the console unnecessarily.
	 */
	if (err < 0 && err != -ENOTCONN)
		pr_debug("%s err %d\n", str, err);
}



int
scif_process_ioctl(struct file *f, unsigned int cmd, uint64_t arg)
{
	struct mic_priv *priv = ((f)->private_data);
	void __user *argp = (void __user *)arg;
	int err = 0;
	struct scifioctl_msg request;
	bool non_block = false;

	non_block = !!(f->f_flags & O_NONBLOCK);

	switch (cmd) {
	case SCIF_BIND:
	{
		int pn;

		if (copy_from_user(&pn, argp, sizeof(pn))) {
			return -EFAULT;
		}

		if ((pn = __scif_bind(priv->epd, pn)) < 0) {
			return pn;
		}

		if (copy_to_user(argp, &pn, sizeof(pn))) {
			return -EFAULT;
		}

		return 0;
	}
	case SCIF_LISTEN:
		return __scif_listen(priv->epd, arg);
	case SCIF_CONNECT:
	{
		struct scifioctl_connect req;
		struct endpt *ep = (struct endpt *)priv->epd;

		if (copy_from_user(&req, argp, sizeof(struct scifioctl_connect))) {
			return -EFAULT;
		}

		if ((err = __scif_connect(priv->epd, &req.peer, non_block)) < 0) {
			return err;
		}

		req.self.node = ep->port.node;
		req.self.port = ep->port.port;

		if (copy_to_user(argp, &req, sizeof(struct scifioctl_connect))) {
			return -EFAULT;
		}


		return 0;
	}
	// Accept is done in two halves.  Thes request ioctl does the basic functility of accepting
	// the request and returning the information about it including the internal ID of the
	// end point.  The register is done with the internID on a new file desciptor opened by the
	// requesting process.
	case SCIF_ACCEPTREQ:
	{
		struct scifioctl_accept request;
		unsigned long sflags;
		scif_epd_t *ep = (scif_epd_t *)&request.endpt;

		if (copy_from_user(&request, argp, sizeof(struct scifioctl_accept))) {
			return -EFAULT;
		}

		if ((err = __scif_accept(priv->epd, &request.peer, ep, request.flags)) < 0) {
			return err;
		}

		if (copy_to_user(argp, &request, sizeof(struct scifioctl_accept))) {
			scif_close(*ep);
			return -EFAULT;
		}

		// Add to the list of user mode eps where the second half of the accept
		// is not yet completed.
		spin_lock_irqsave(&ms_info.mi_eplock, sflags);
		list_add_tail(&((*ep)->miacceptlist), &ms_info.mi_uaccept);
		list_add_tail(&((*ep)->liacceptlist), &priv->epd->li_accept);
		(*ep)->listenep = priv->epd;
		priv->epd->acceptcnt++;
		spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);

		return 0;
	}
	case SCIF_ACCEPTREG:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct endpt *newep;
		struct endpt *lisep;
		struct endpt *ep;
		struct endpt *fep = NULL;
		struct endpt *tmpep;
		struct list_head *pos, *tmpq;
		unsigned long sflags;

		// Finally replace the pointer to the accepted endpoint
		if (copy_from_user(&newep, argp, sizeof(void *)))
			return -EFAULT;

		// Remove form the user accept queue
		spin_lock_irqsave(&ms_info.mi_eplock, sflags);
		list_for_each_safe(pos, tmpq, &ms_info.mi_uaccept) {
			tmpep = list_entry(pos, struct endpt, miacceptlist);
			if (tmpep == newep) {
				list_del(pos);
				fep = tmpep;
				break;
			}
		}

		if (fep == NULL) {
			spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);
			return -ENOENT;
		}

		lisep = newep->listenep;
		list_for_each_safe(pos, tmpq, &lisep->li_accept) {
			tmpep = list_entry(pos, struct endpt, liacceptlist);
			if (tmpep == newep) {
				list_del(pos);
				lisep->acceptcnt--;
				break;
			}
		}

		spin_unlock_irqrestore(&ms_info.mi_eplock, sflags);

		// Free the resources automatically created from the open.
		micscif_teardown_ep(priv->epd);
		micscif_add_epd_to_zombie_list(priv->epd, !MI_EPLOCK_HELD);
		priv->epd = newep;
		ep = (struct endpt *)priv->epd;
		ep = ep;
		return 0;
	}
	case SCIF_SEND:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);

		if (copy_from_user(&request, argp,
			sizeof(struct scifioctl_msg))) {
			err = -EFAULT;
			goto send_err;
		}

		if ((err = scif_user_send(priv->epd, request.msg,
			request.len, request.flags)) < 0)
			goto send_err;

		if (copy_to_user(&((struct scifioctl_msg*)argp)->out_len,
					&err, sizeof(err))) {
			err = -EFAULT;
			goto send_err;
		}
		err = 0;
send_err:
		scif_err_debug(err, "scif_send");
		return err;
	}
	case SCIF_RECV:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);

		if (copy_from_user(&request, argp,
			sizeof(struct scifioctl_msg))) {
			err = -EFAULT;
			goto recv_err;
		}

		if ((err = scif_user_recv(priv->epd, request.msg,
			request.len, request.flags)) < 0)
			goto recv_err;

		if (copy_to_user(&((struct scifioctl_msg*)argp)->out_len,
			&err, sizeof(err))) {
			err = -EFAULT;
			goto recv_err;
		}
		err = 0;
recv_err:
		scif_err_debug(err, "scif_recv");
		return err;
	}
	case SCIF_REG:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct scifioctl_reg reg;
		off_t ret;

		if (copy_from_user(&reg, argp, sizeof(reg))) {
			err = -EFAULT;
			goto reg_err;
		}
		if (reg.flags & SCIF_MAP_KERNEL) {
			err = -EINVAL;
			goto reg_err;
		}
		if ((ret = __scif_register(priv->epd, reg.addr, reg.len,
				reg.offset, reg.prot, reg.flags)) < 0) {
			err = (int)ret;
			goto reg_err;
		}

		if (copy_to_user(&((struct scifioctl_reg*)argp)->out_offset,
				&ret, sizeof(reg.out_offset))) {
			err = -EFAULT;
			goto reg_err;
		}
		err = 0;
reg_err:
		scif_err_debug(err, "scif_register");
		return err;
	}
	case SCIF_UNREG:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct scifioctl_unreg unreg;

		if (copy_from_user(&unreg, argp, sizeof(unreg))) {
			err = -EFAULT;
			goto unreg_err;
		}
		err = __scif_unregister(priv->epd, unreg.offset, unreg.len);
unreg_err:
		scif_err_debug(err, "scif_unregister");
		return err;
	}
	case SCIF_READFROM:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct scifioctl_copy copy;

		if (copy_from_user(&copy, argp, sizeof(copy))) {
			err = -EFAULT;
			goto readfrom_err;
		}
		err = __scif_readfrom(priv->epd,
					copy.loffset,
					copy.len,
					copy.roffset,
					copy.flags);
readfrom_err:
		scif_err_debug(err, "scif_readfrom");
		return err;
	}
	case SCIF_WRITETO:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct scifioctl_copy copy;

		if (copy_from_user(&copy, argp, sizeof(copy))) {
			err = -EFAULT;
			goto writeto_err;
		}
		err = __scif_writeto(priv->epd,
					copy.loffset,
					copy.len,
					copy.roffset,
					copy.flags);
writeto_err:
		scif_err_debug(err, "scif_writeto");
		return err;
	}
	case SCIF_VREADFROM:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct scifioctl_copy copy;

		if (copy_from_user(&copy, argp, sizeof(copy))) {
			err = -EFAULT;
			goto vreadfrom_err;
		}
		err = __scif_vreadfrom(priv->epd,
					copy.addr,
					copy.len,
					copy.roffset,
					copy.flags);
vreadfrom_err:
		scif_err_debug(err, "scif_vreadfrom");
		return err;
	}
	case SCIF_VWRITETO:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct scifioctl_copy copy;

		if (copy_from_user(&copy, argp, sizeof(copy))) {
			err = -EFAULT;
			goto vwriteto_err;
		}
		err = __scif_vwriteto(priv->epd,
					copy.addr,
					copy.len,
					copy.roffset,
					copy.flags);
vwriteto_err:
		scif_err_debug(err, "scif_vwriteto");
		return err;
	}
	case SCIF_GET_NODEIDS:
	{
		struct scifioctl_nodeIDs nodeIDs;
		int entries;
		uint16_t *nodes;
		uint16_t self;

		if (copy_from_user(&nodeIDs, argp, sizeof(nodeIDs))) {
			err = -EFAULT;
			goto getnodes_err2;
		}

		entries = SCIF_MIN(MAX_BOARD_SUPPORTED, nodeIDs.len);

		nodes = kmalloc(sizeof(uint16_t) * entries, GFP_KERNEL);
		if ( (entries != 0) && (!nodes) ){
			err = -ENOMEM;
			goto getnodes_err2;
		}
		nodeIDs.len = scif_get_nodeIDs(nodes, entries, &self);

		if (copy_to_user(nodeIDs.nodes,
				nodes, sizeof(uint16_t) * entries)) {
			err = -EFAULT;
			goto getnodes_err1;
		}

		if (copy_to_user(nodeIDs.self,
				&self, sizeof(uint16_t))) {
			err = -EFAULT;
			goto getnodes_err1;
		}

		if (copy_to_user(argp, &nodeIDs, sizeof(nodeIDs))) {
			err = -EFAULT;
			goto getnodes_err1;
		}
getnodes_err1:
		kfree(nodes);
getnodes_err2:
		return err;
	}
	case SCIF_FENCE_MARK:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct scifioctl_fence_mark mark;
		int tmp_mark = 0;

		if (copy_from_user(&mark, argp, sizeof(mark))) {
			err = -EFAULT;
			goto fence_mark_err;
		}
		if ((err = __scif_fence_mark(priv->epd,
					mark.flags, &tmp_mark)))
			goto fence_mark_err;
		if (copy_to_user(mark.mark, &tmp_mark, sizeof(tmp_mark))) {
			err = -EFAULT;
			goto fence_mark_err;
		}
fence_mark_err:
		scif_err_debug(err, "scif_fence_mark");
		return err;
	}
	case SCIF_FENCE_WAIT:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		err = __scif_fence_wait(priv->epd, arg);
		scif_err_debug(err, "scif_fence_wait");
		return err;
	}
	case SCIF_FENCE_SIGNAL:
	{
		struct mic_priv *priv = (struct mic_priv *)((f)->private_data);
		struct scifioctl_fence_signal signal;

		if (copy_from_user(&signal, argp, sizeof(signal))) {
			err = -EFAULT;
			goto fence_signal_err;
		}

		err = __scif_fence_signal(priv->epd, signal.loff,
			signal.lval, signal.roff, signal.rval, signal.flags);
fence_signal_err:
		scif_err_debug(err, "scif_fence_signal");
		return err;
	}
	case SCIF_GET_VERSION:
	{
		return SCIF_VERSION;
	}
	}
	return -EINVAL;
}
