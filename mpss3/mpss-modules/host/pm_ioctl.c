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

/* contains code to handle MIC IO control codes */


#include "mic_common.h"
#include <linux/module.h>

/* helper methods for debugging/unit testing /*/
static int check_test_msg(mic_ctx_t *mic_ctx, void *buf, uint32_t len);



#define PM_MMIO_REGVALUE_GET(_name, _offset)				\
int get_##_name(void *data, uint64_t *value)				\
{									\
	uint64_t bid;							\
	mic_ctx_t *mic_ctx;						\
									\
	bid = (uint64_t)data;						\
	if (bid >= mic_data.dd_numdevs) {				\
		return -EINVAL;					\
	}								\
	mic_ctx = get_per_dev_ctx(bid);					\
	if (!mic_ctx) {							\
		printk("DD");						\
		return -EINVAL;					\
	}								\
									\
	*value = pm_reg_read(mic_ctx, _offset);				\
	return 0;							\
}									\
DEFINE_SIMPLE_ATTRIBUTE(fops_##_name, get_##_name, NULL, "%llu");	\

static PM_MMIO_REGVALUE_GET(svidctrl, SBOX_SVID_CONTROL);
static PM_MMIO_REGVALUE_GET(pcuctrl, SBOX_PCU_CONTROL);
static PM_MMIO_REGVALUE_GET(hoststate,SBOX_HOST_PMSTATE);
static PM_MMIO_REGVALUE_GET(cardstate, SBOX_UOS_PMSTATE);
static PM_MMIO_REGVALUE_GET(wtimer, SBOX_C3WAKEUP_TIMER);
static PM_MMIO_REGVALUE_GET(gpmctrl, GBOX_PM_CTRL);
static PM_MMIO_REGVALUE_GET(core_volt, SBOX_COREVOLT);
static PM_MMIO_REGVALUE_GET(uos_pcuctrl, SBOX_UOS_PCUCONTROL);

static int depgraph_j2i_show(struct seq_file *s, void *pos)
{
	uint64_t bid = (uint64_t)s->private;
	mic_ctx_t *mic_ctx;
	int i, j;

	mic_ctx = get_per_dev_ctx(bid);
	if (!mic_ctx) {
		return -EINVAL;
	}

	seq_printf(s,"=================================================================\n");
	seq_printf(s,"%-10s |%-25s\n", "Scif Node" , "dependent nodes");
	seq_printf(s,"=================================================================\n");

	for ( i = 0; i <= ms_info.mi_maxid; i++) {
		seq_printf(s, "%-10d |", i);
		for (j = 0; j <= ms_info.mi_maxid; j++) {
			switch(ms_info.mi_depmtrx[j][i]) {
				case DEP_STATE_DEPENDENT:
				{
					/* (A) - active dependency on node i */
					seq_printf(s, "%d(A),", j);
					break;
				}
				case DEP_STATE_DISCONNECT_READY:
				{
					/* (R) - node j has sent PC6 ready message to the host
					 * dependency is not active so node i can go idle
					 */
					seq_printf(s, "%d(R),", j);
					break;
				}
				case DEP_STATE_DISCONNECTED:
				{
					/* (D) - node j is in idle state.
					 * dependency is not active so node i can go idle
					 */
					seq_printf(s, "%d(D),", j);
					break;
				}
			}
		}
		seq_printf(s,"\n=================================================================\n");
	}

	return 0;
}

static int depgraph_j2i_open(struct inode *inode, struct file *file)
{
	return single_open(file, depgraph_j2i_show, inode->i_private);
}

static struct file_operations depgraph_j2i_file_ops = {
	.owner   = THIS_MODULE,
	.open    = depgraph_j2i_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int depgraph_i2j_show(struct seq_file *s, void *pos)
{
	uint64_t bid = (uint64_t)s->private;
	mic_ctx_t *mic_ctx;
	int i, j;

	mic_ctx = get_per_dev_ctx(bid);
	if (!mic_ctx) {
		return -EINVAL;
	}

	seq_printf(s,"=================================================================\n");
	seq_printf(s,"%-10s |%-25s\n", "Scif Node" , "is dependent on Nodes");
	seq_printf(s,"=================================================================\n");

	for ( i = 0; i <= ms_info.mi_maxid; i++) {
		seq_printf(s, "%-10d |", i);
		for (j = 0; j <= ms_info.mi_maxid; j++) {
			switch(ms_info.mi_depmtrx[i][j]) {
				case DEP_STATE_DEPENDENT:
				{
					/* (A) - active dependency on node j */
					seq_printf(s, "%d(A),", j);
					break;
				}
				case DEP_STATE_DISCONNECT_READY:
				{
					/* (R) - node j has sent PC6 ready message to the host */
					seq_printf(s, "%d(R),", j);
					break;
				}
				case DEP_STATE_DISCONNECTED:
				{
					/* (D) - node j is in idle state.
					 * This should not happen unless node i itself is in idle state
					 */
					seq_printf(s, "%d(D),", j);
					break;
				}
			}
		}
		seq_printf(s,"\n=================================================================\n");
	}

	return 0;
}

static int depgraph_i2j_open(struct inode *inode, struct file *file)
{
	return single_open(file, depgraph_i2j_show, inode->i_private);
}

static struct file_operations depgraph_i2j_file_ops = {
	.owner   = THIS_MODULE,
	.open    = depgraph_i2j_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int connection_info_show(struct seq_file *s, void *pos) {

	uint64_t bid = (uint64_t)s->private;
	mic_ctx_t *mic_ctx;
	int count = 0;
	struct list_head *position, *tmpq;

	mic_ctx = get_per_dev_ctx(bid);
	if (!mic_ctx) {
		return -EINVAL;
	}

	seq_printf(s,"=========================================================================\n");
	if(mic_ctx->micpm_ctx.pm_epd != NULL) {
		seq_printf(s, "%-35s | %35d\n", "Local Node", mic_ctx->micpm_ctx.pm_epd->port.node);
		seq_printf(s, "%-35s | %35d\n", "Local Port", mic_ctx->micpm_ctx.pm_epd->port.port);
		seq_printf(s, "%-35s | %35d\n", "Remote Node", mic_ctx->micpm_ctx.pm_epd->peer.node);
		seq_printf(s, "%-35s | %35d\n", "Remote Port", mic_ctx->micpm_ctx.pm_epd->peer.port);
		seq_printf(s, "%-35s | %35d\n", "Connection state", mic_ctx->micpm_ctx.pm_epd->state);
		if(!list_empty(&mic_ctx->micpm_ctx.msg_list)) {
			list_for_each_safe(position, tmpq, &mic_ctx->micpm_ctx.msg_list) {
				count++;
			}
		} else {
			count = 0;
		}
		seq_printf(s, "%-35s | %35d\n", "Messages in queue", count);
	} else {
		seq_printf(s, "%s\n", "No PM connection found");
	}
	seq_printf(s,"=========================================================================\n");

	return 0;
}

static int connection_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, connection_info_show, inode->i_private);
}

static struct file_operations connection_info_file_ops = {
	.owner   = THIS_MODULE,
	.open    = connection_info_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int active_set_show(struct seq_file *s, void *pos) {

	uint64_t bid = (uint64_t)s->private;
	mic_ctx_t *mic_ctx;
	int i, j = 0;
	uint8_t *nodemask;
	uint8_t *temp_buf_ptr;

	mic_ctx = get_per_dev_ctx(bid);
	if (!mic_ctx) {
		return -EINVAL;
	}

	nodemask = (uint8_t*) kzalloc(mic_ctx->micpm_ctx.nodemask.len, GFP_KERNEL);
	if (!nodemask) {
		seq_printf(s, "%s\n", "Cannot allocate buffer");
		return 0;
	}

	if ((micscif_get_activeset(mic_ctx->bi_id + 1, nodemask))) {
		seq_printf(s, "%s\n", "Cannot calculate activation set");
		kfree(nodemask);
		return 0;
	}

	seq_printf(s, "%s\n", "Nodes in activation set:");
	temp_buf_ptr = nodemask;
	for ( i = 0; i < mic_ctx->micpm_ctx.nodemask.len; i++) {
		temp_buf_ptr = nodemask + i;
		for (j = 0; j < 8; j++) {
			if (*temp_buf_ptr & (1ULL << j))
				seq_printf(s, "%d ", j + (i * 8));
		}
	}
	seq_printf(s, "\n");
	kfree(nodemask);
	return 0;
}

static int active_set_open(struct inode *inode, struct file *file)
{
	return single_open(file, active_set_show, inode->i_private);
}

static struct file_operations activation_set_file_ops = {
	.owner = THIS_MODULE,
	.open = active_set_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int deactive_set_show(struct seq_file *s, void *pos) {

	uint64_t bid = (uint64_t)s->private;
	mic_ctx_t *mic_ctx;
	int i, j;
	uint8_t *nodemask;
	uint8_t *temp_buf_ptr;

	mic_ctx = get_per_dev_ctx(bid);
	if (!mic_ctx) {
		return -EINVAL;
	}

	nodemask = (uint8_t*) kzalloc(mic_ctx->micpm_ctx.nodemask.len, GFP_KERNEL);
	if (!nodemask) {
		seq_printf(s, "%s\n", "Cannot allocate buffer");
		return 0;
	}

	if ((micscif_get_deactiveset(mic_ctx->bi_id +1, nodemask, 1))) {
		seq_printf(s, "%s\n", "Cannot calculate activation set");
		kfree(nodemask);
		return 0;
	}

	seq_printf(s, "%s\n", "Nodes in deactivation set:");
	temp_buf_ptr = nodemask;
	for ( i = 0; i < mic_ctx->micpm_ctx.nodemask.len; i++) {
		temp_buf_ptr = nodemask + i;
		for (j = 0; j < 8; j++) {
			if (*temp_buf_ptr & (1ULL << j))
				seq_printf(s, "%d ", j + (i * 8));
		}
	}
	seq_printf(s, "\n");
	kfree(nodemask);
	return 0;
}

static int deactive_set_open(struct inode *inode, struct file *file)
{
	return single_open(file, deactive_set_show, inode->i_private);
}

static struct file_operations deactivation_set_file_ops = {
	.owner = THIS_MODULE,
	.open = deactive_set_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int ospm_restart_show(struct seq_file *s, void *pos) {

	uint64_t bid = (uint64_t)s->private;
	mic_ctx_t *mic_ctx;
	int err;

	mic_ctx = get_per_dev_ctx(bid);
	if (!mic_ctx) {
		return -EINVAL;
	}

	err = pm_stop_device(mic_ctx);
	if(err) {
		seq_printf(s, "%s:%d\n", "Error calling pm_stop_device.", err);
		return err;
	}

	err = pm_start_device(mic_ctx);
	if(err) {
		seq_printf(s, "%s:%d\n", "Error calling pm_start_device.", err);
		return err;
	}

	return 0;
}

static int ospm_restart_open(struct inode *inode, struct file *file)
{
	return single_open(file, ospm_restart_show, inode->i_private);
}

static struct file_operations ospm_restart_file_ops = {
	.owner   = THIS_MODULE,
	.open    = ospm_restart_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int testmsg_set(void *data, uint64_t value)
{
	uint64_t bid;
	mic_ctx_t *mic_ctx;
	int err;

	bid = (uint64_t)data;
	if (bid >= mic_data.dd_numdevs) {
		return -EINVAL;
	}

	mic_ctx = get_per_dev_ctx(bid);
	if (!mic_ctx) {
		return -EINVAL;
	}

	if (value == 0) {
		return -EINVAL;
	}

	err = mic_pm_send_msg(mic_ctx ,PM_MESSAGE_TEST, PM_TEST_MSG_BODY, sizeof(PM_TEST_MSG_BODY));
	return err;
}

static int testmsg_get(void *data, uint64_t *value)
{
	uint64_t bid;
	mic_ctx_t *mic_ctx;
	int err;

	bid = (uint64_t)data;
	if (bid >= mic_data.dd_numdevs) {
		return -EINVAL;
	}

	mic_ctx = get_per_dev_ctx(bid);
	if (!mic_ctx) {
		return -EINVAL;
	}

	err = check_test_msg(mic_ctx,PM_TEST_MSG_BODY, sizeof(PM_TEST_MSG_BODY));
	*value = err;

	return err;
}
DEFINE_SIMPLE_ATTRIBUTE(testmsg_fops, testmsg_get, testmsg_set, "%llu");

int
micpm_dbg_init(mic_ctx_t *mic_ctx)
{
	/* directory name will be in format micpmXXXXX
	 * so assuming the name string wont excceed 12 characters */
	const uint32_t DBG_DIRNAME_LENGTH = 12;
	char pmdbg_dir_name[DBG_DIRNAME_LENGTH];
	micpm_ctx_t *micpm_ctx = &mic_ctx->micpm_ctx;
	struct dentry *mmiodir;
	uint64_t id = mic_ctx->bi_id;


	if(!mic_data.dd_pm.pmdbgparent_dir) {
		printk(KERN_ERR "%s: %d Parent debugfs directory does not exist.\n"
			"debugfs may not be supported in kernel", __func__, __LINE__);
		return -EOPNOTSUPP;
	}

	snprintf(pmdbg_dir_name, sizeof(pmdbg_dir_name), "micpm%d", mic_ctx->bi_id);
	micpm_ctx->pmdbg_dir = debugfs_create_dir
			(pmdbg_dir_name, mic_data.dd_pm.pmdbgparent_dir);
	if (!micpm_ctx->pmdbg_dir) {
		printk(KERN_ERR "%s: %d Failed in creating debugfs directory\n"
			"debugfs may noe be supported in kernel", __func__, __LINE__);
		return -EOPNOTSUPP;
	}

	/* Create debugfs entry to get/set idle state of the card known by host*/
	debugfs_create_u32("idle_state", S_IRUGO | S_IWUSR, micpm_ctx->pmdbg_dir, &micpm_ctx->idle_state);

	/*
	 * Create debugfs entry for sending PM_TEST_MESSAGE for testing communication to card
	 * set value = PM_MESSAGE_TEST to send the message to card
	 * get value to verfy that message was successfully sent, looped back by card and received.(0 = success)
	*/
	debugfs_create_file("testmsg", S_IRUGO | S_IWUSR, micpm_ctx->pmdbg_dir, (void*)id, &testmsg_fops);

	/* Create debugfs entry for showing for each node 'i' , all nodes 'j' i is dependent on */
	debugfs_create_file("depgraph_i2j",
			S_IRUGO,
			micpm_ctx->pmdbg_dir,
			(void*)id,
			&depgraph_i2j_file_ops);

	/* Create debugfs entry for showing for each node 'i', all nodes 'j' which are dependent on 'i' */
	debugfs_create_file("depgraph_j2i",
			S_IRUGO,
			micpm_ctx->pmdbg_dir,
			(void*)id,
			&depgraph_j2i_file_ops);

	/* Create debugfs entry for showing connection info for a node */
	debugfs_create_file("connection_info",
			S_IRUGO,
			micpm_ctx->pmdbg_dir,
			(void*)id,
			&connection_info_file_ops);

	/* Create debugfs entry to initiate OSPM restart for a node */
	debugfs_create_file("ospm_restart",
			S_IRUGO,
			micpm_ctx->pmdbg_dir,
			(void*)id,
			&ospm_restart_file_ops);

	/* Create debugfs entry to display activation set for a node */
	debugfs_create_file("activation_set",
			S_IRUGO,
			micpm_ctx->pmdbg_dir,
			(void*)id,
			&activation_set_file_ops);

	/* Create debugfs entry to display de-activation set for a node */
	debugfs_create_file("deactivation_set",
			S_IRUGO,
			micpm_ctx->pmdbg_dir,
			(void*)id,
			&deactivation_set_file_ops);

	/* Create debugfs entries for reading power management status/control register value in MMIO region */
	mmiodir = debugfs_create_dir("mmio", micpm_ctx->pmdbg_dir);
	if (!mmiodir) {
		printk(KERN_ERR "%s: %d Failed in creating debugfs directory\n"
			"debugfs may noe be supported in kernel", __func__, __LINE__);
		return -EOPNOTSUPP;
	}
	debugfs_create_file("svidctrl", S_IRUGO, mmiodir,(void*)id, &fops_svidctrl);
	debugfs_create_file("pcuctrl", S_IRUGO, mmiodir,(void*)id, &fops_pcuctrl);
	debugfs_create_file("hoststate", S_IRUGO, mmiodir,(void*)id, &fops_hoststate);
	debugfs_create_file("cardstate", S_IRUGO, mmiodir,(void*)id, &fops_cardstate);
	debugfs_create_file("wtimer", S_IRUGO, mmiodir,(void*)id, &fops_wtimer);
	debugfs_create_file("gpmctrl", S_IRUGO, mmiodir,(void*)id, &fops_gpmctrl);
	debugfs_create_file("core_volt", S_IRUGO, mmiodir,(void*)id, &fops_core_volt);
	debugfs_create_file("uos_pcuctrl", S_IRUGO, mmiodir,(void*)id, &fops_uos_pcuctrl);

	return 0;
}

void micpm_dbg_parent_init(void) {
	mic_data.dd_pm.pmdbgparent_dir = debugfs_create_dir("micpm", NULL);
	if (!mic_data.dd_pm.pmdbgparent_dir) {
		PM_DEBUG("%s: %d Failed in creating debugfs directory\n"
			"debugfs may not be supported in kernel", __func__, __LINE__);
	}

	debugfs_create_u32("enable_pm_logging", S_IRUGO | S_IWUSR,
			mic_data.dd_pm.pmdbgparent_dir, &mic_data.dd_pm.enable_pm_logging);

	return;
}


/*
 * Test message is looped back to driver and lives in the message list.
 * This function retrieves the message and send it to user space which
 * can check if its the same message as that was sent.
 */
static int
check_test_msg(mic_ctx_t *mic_ctx, void *buf, uint32_t len)
{
	int err = -EINVAL;
	pm_recv_msg_t *recv_msg = NULL;
	struct list_head *pos = NULL, *tmpq = NULL;
	bool msg_found = false;

	if(len != sizeof(pm_msg_unit_test)) {
		pr_debug("Invalid Args: Size of buffer\n");
		return -EINVAL;
	}

	mutex_lock(&mic_ctx->micpm_ctx.msg_mutex);
	if(!list_empty_careful(&mic_ctx->micpm_ctx.msg_list)) {
		list_for_each_safe(pos, tmpq, &mic_ctx->micpm_ctx.msg_list) {
			recv_msg = list_entry(pos, pm_recv_msg_t, msg);
			/*Do not touch the message if its not a test message */
			if (recv_msg->msg_header.opcode == PM_MESSAGE_TEST) {
				list_del(&recv_msg->msg);
				msg_found = true;
				break;
			}
		}
	} else {
		pr_debug("empty message list \n");
		goto no_msg;
	}

	if (msg_found == false) {
		pr_debug("Test msg not found \n");
		goto no_msg;
	}

	if(recv_msg->msg_body == NULL) {
		pr_debug("Invalid source buffer\n");
		goto list_free;
	}

	err = strncmp((char*)recv_msg->msg_body, (char*)buf, len);
	kfree(recv_msg->msg_body);

list_free:
	kfree(recv_msg);

no_msg:
	mutex_unlock(&mic_ctx->micpm_ctx.msg_mutex);
	return err;

}
