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

#include "mic_common.h"
#include "scif.h"
#include "mic/micscif.h"
#include "mic/mic_pm.h"
#include "mic/micveth_dma.h"
#include <linux/virtio_ring.h>
#include "linux/virtio_blk.h"
#include "mic/mic_virtio.h"

//few helper functions
int pm_reg_read(mic_ctx_t *mic_ctx, uint32_t regoffset) {
	uint32_t regval = 0;
if (mic_ctx->bi_family == FAMILY_ABR)
	regval = DBOX_READ(mic_ctx->mmio.va, regoffset);
else if (mic_ctx->bi_family == FAMILY_KNC)
	regval = SBOX_READ(mic_ctx->mmio.va, regoffset);

	return regval;
}

int pm_reg_write(uint32_t value, mic_ctx_t *mic_ctx, uint32_t regoffset) {
	int err = 0;
if (mic_ctx->bi_family == FAMILY_ABR)
	DBOX_WRITE(value, mic_ctx->mmio.va, regoffset);
else if (mic_ctx->bi_family == FAMILY_KNC)
	SBOX_WRITE(value, mic_ctx->mmio.va, regoffset);

	return err;
}

int hw_idle(mic_ctx_t *mic_ctx) {

	uint8_t is_ring_active;
	sbox_pcu_ctrl_t ctrl_regval = {0};
	uint32_t idle_wait_cnt;

	for(idle_wait_cnt = 0; idle_wait_cnt <= MAX_HW_IDLE_WAIT_COUNT;
			idle_wait_cnt++) {
		ctrl_regval.value = pm_reg_read(mic_ctx,SBOX_PCU_CONTROL);
		is_ring_active = ctrl_regval.bits.mclk_enabled;
		if(likely(!is_ring_active))
			return !is_ring_active;
		msleep(1);
	}

	PM_DEBUG("Timing out waiting for HW to become idle\n");
	return !is_ring_active;
}

int hw_active(mic_ctx_t *mic_ctx) {
	uint8_t is_ring_active;
	sbox_pcu_ctrl_t ctrl_regval;
	uint32_t idle_wait_cnt;

	for(idle_wait_cnt = 0; idle_wait_cnt <= MAX_HW_IDLE_WAIT_COUNT;
			idle_wait_cnt++) {
		ctrl_regval.value = pm_reg_read(mic_ctx,SBOX_PCU_CONTROL);
		is_ring_active = ctrl_regval.bits.mclk_enabled;
		if (likely(is_ring_active))
			return is_ring_active;
		msleep(10);
	}

	PM_DEBUG("Timing out waiting for HW to become active\n");
	return is_ring_active;

}

PM_IDLE_STATE get_card_state(mic_ctx_t *mic_ctx) {

	PM_IDLE_STATE state;
	sbox_uos_pm_state_t upmstate_regval =  {0};
	upmstate_regval.value = pm_reg_read(mic_ctx, SBOX_UOS_PMSTATE);
	state = (PM_IDLE_STATE)(upmstate_regval.bits.uos_pm_state);
	return state;

}

PM_IDLE_STATE get_host_state(mic_ctx_t *mic_ctx) {

	PM_IDLE_STATE state;
	sbox_host_pm_state_t hpmstate_regval =  {0};
	hpmstate_regval.value = pm_reg_read(mic_ctx, SBOX_HOST_PMSTATE);
	state = (PM_IDLE_STATE)(hpmstate_regval.bits.host_pm_state);
	return state;

}

int set_host_state(mic_ctx_t *mic_ctx, PM_IDLE_STATE state) {

	int err = 0;
	sbox_host_pm_state_t hpmstate_regval = {0};
	hpmstate_regval.value = pm_reg_read(mic_ctx, SBOX_HOST_PMSTATE);
	hpmstate_regval.bits.host_pm_state = 0;
	hpmstate_regval.bits.host_pm_state = state;
	pm_reg_write(hpmstate_regval.value, mic_ctx, SBOX_HOST_PMSTATE);
	return err;
}

int check_card_state(mic_ctx_t *mic_ctx, PM_IDLE_STATE state) {
	PM_IDLE_STATE card_state = get_card_state(mic_ctx);
	return (state == card_state) ? 1 : 0;
}

int check_host_state(mic_ctx_t *mic_ctx, PM_IDLE_STATE state) {
	PM_IDLE_STATE host_state = get_host_state(mic_ctx);
	return (state == host_state) ? 1 : 0;
}

uint32_t svid_cmd_fmt(unsigned int bits)
{
	unsigned int bits_set,bmask;

	bmask = bits;

	for (bits_set = 0; bmask; bits_set++) {
		/* Zero the least significant bit that is set */
		bmask &=  (bmask - 1);
	}
	bits <<= 1; /* Make way for the parity bit	*/
	if (bits_set & 1) {	/* odd number of 1s	*/
		bits |= 1;
	}

	return bits;
}

void set_vid(mic_ctx_t *mic_ctx, sbox_svid_control svidctrl_regval, unsigned int vidcode) {

	uint32_t temp;
	uint32_t svid_cmd = 0;
	uint32_t svid_dout = 0;
	temp = svid_cmd_fmt((KNC_SVID_ADDR << 13) |
			(KNC_SETVID_SLOW << 8) | vidcode);
	svid_cmd = (KNC_SVID_ADDR << 5) | KNC_SETVID_SLOW;
	svidctrl_regval.bits.svid_cmd = 0x0e0;
	svidctrl_regval.bits.svid_cmd = svid_cmd;

	svid_dout = temp & 0x1ff;
	svidctrl_regval.bits.svid_dout = 0;
	svidctrl_regval.bits.svid_dout = svid_dout;

	svidctrl_regval.bits.cmd_start = 0x1;
	pm_reg_write(svidctrl_regval.value, mic_ctx,
			SBOX_SVID_CONTROL);

	msleep(10);

	return;
}

int set_vid_knc(mic_ctx_t *mic_ctx, unsigned int vidcode)
{
	uint32_t status = 0;

	sbox_svid_control svidctrl_regval = {0};
	uint32_t svid_idle = 0;
	uint32_t svid_error = 0;
	int i = 0;
	uint32_t wait_cnt = 0;
	sbox_core_volt_t core_volt_regval = {0};
	int retry = 0;

	if (mic_ctx->bi_stepping >= KNC_B0_STEP) {
		for (retry = 0; retry < SET_VID_RETRY_COUNT; retry++) {
			status = 0;
			for (i = 0; i < KNC_SETVID_ATTEMPTS; i++) {
				svidctrl_regval.value = pm_reg_read(mic_ctx,SBOX_SVID_CONTROL);
				svid_idle = svidctrl_regval.bits.svid_idle;

				if (svid_idle) {
					set_vid(mic_ctx, svidctrl_regval, vidcode);
					svidctrl_regval.value =
							pm_reg_read(mic_ctx,SBOX_SVID_CONTROL);
					svid_idle =  svidctrl_regval.bits.svid_idle;
					svid_error = svidctrl_regval.bits.svid_error;

					if (!svid_idle) {
						printk(KERN_ERR "%s SVID command failed - Idle not set\n", 
								__func__);
						msleep(10);
						continue;
					}

					if (svid_error) {
						if (SBOX_SVIDCTRL_ACK1ACK0(svidctrl_regval.value) == 0x2) {
							printk(KERN_ERR "%s SVID command failed - rx parity error\n", 
									__func__);
						} else {
						printk(KERN_ERR "%s SVID command failed - tx parity error\n", 
								__func__);
						}
						status = -EINVAL;
						goto exit;
					} else {
						PM_DEBUG("SVID Command Successful - VID set to %d\n",vidcode);
						break;
					}
				}
			}

			if (i == KNC_SETVID_ATTEMPTS) {
				printk(KERN_ERR "%s Timed out waiting for SVID idle\n", __func__);
				status = -EINVAL;
				goto exit;
			}

			/* Verify that the voltage is set */
			for(wait_cnt = 0; wait_cnt <= 100; wait_cnt++) {
				core_volt_regval.value = pm_reg_read(mic_ctx, SBOX_COREVOLT);
				if(vidcode == core_volt_regval.bits.vid) {
					return status;
				}
				msleep(10);
				PM_DEBUG("Retry: %d Voltage not set yet. vidcode = 0x%x Current vid = 0x%x\n",
						retry, vidcode, core_volt_regval.bits.vid);
			}

			PM_PRINT("Retry: %d Failed to set vid for node %d. vid code = 0x%x Current vid = 0x%x.\n",
				retry, mic_get_scifnode_id(mic_ctx), vidcode, core_volt_regval.bits.vid);
			status = -ENODEV;
		}
	} else {
		set_vid(mic_ctx, svidctrl_regval, vidcode);

		/* SBOX_COREVOLT does not reflect the correct vid
		 * value on A0. Just wait here for sometime to
		 * allow for the vid to be set.
		 */
		msleep(20);
	}

exit:
	return status;
}

/* @print_nodemaskbuf
 *
 * @param -  buf - the nodemask buffer
 *
 * prints the nodes in the nodemask.
 *
 * @returns - none
 */
void print_nodemaskbuf(uint8_t* buf) {

	uint8_t *temp_buf_ptr;
	uint32_t i,j;

	temp_buf_ptr = buf;
	PM_DEBUG("Nodes in nodemask: ");
	for(i = 0; i <= ms_info.mi_maxid; i++) {
		temp_buf_ptr = buf + i;
		for (j = 0; j < 8; j++) {
			if (get_nodemask_bit(temp_buf_ptr, j))
				pr_debug("%d ", j + (i * 8));
		}
	}
}

void restore_pc6_registers(mic_ctx_t *mic_ctx, bool from_dpc3) {
	sbox_pcu_ctrl_t ctrl_regval = {0};
	sbox_uos_pcu_ctrl_t uos_ctrl_regval = {0};
	gbox_pm_control pmctrl_reg = {0};
	sbox_core_freq_t core_freq_reg = {0};

	if (!from_dpc3) {
		if(KNC_A_STEP == mic_ctx->bi_stepping) {
			ctrl_regval.value = pm_reg_read(mic_ctx, SBOX_PCU_CONTROL);
			ctrl_regval.bits.enable_mclk_pl_shutdown = 0;
			pm_reg_write(ctrl_regval.value, mic_ctx, SBOX_PCU_CONTROL);
		} else {
			uos_ctrl_regval.value = pm_reg_read(mic_ctx,SBOX_UOS_PCUCONTROL);
			uos_ctrl_regval.bits.enable_mclk_pll_shutdown = 0;
			pm_reg_write(uos_ctrl_regval.value, mic_ctx, SBOX_UOS_PCUCONTROL);
		}


		ctrl_regval.value = pm_reg_read(mic_ctx, SBOX_PCU_CONTROL);
		ctrl_regval.bits.prevent_auto_c3_exit = 0;
		pm_reg_write(ctrl_regval.value, mic_ctx, SBOX_PCU_CONTROL);
	}

	pmctrl_reg.value = pm_reg_read(mic_ctx, GBOX_PM_CTRL);
	pmctrl_reg.bits.in_pckgc6 = 0;
	pm_reg_write(pmctrl_reg.value, mic_ctx, GBOX_PM_CTRL);

	ctrl_regval.value = pm_reg_read(mic_ctx, SBOX_PCU_CONTROL);
	ctrl_regval.bits.grpB_pwrgood_mask = 0;
	pm_reg_write(ctrl_regval.value, mic_ctx, SBOX_PCU_CONTROL);

	core_freq_reg.value = pm_reg_read(mic_ctx, SBOX_COREFREQ);
	core_freq_reg.bits.booted = 1;
	pm_reg_write(core_freq_reg.value, mic_ctx, SBOX_COREFREQ);
}

void program_mclk_shutdown(mic_ctx_t *mic_ctx, bool set)
{
	sbox_uos_pcu_ctrl_t uos_ctrl_regval;
	sbox_pcu_ctrl_t ctrl_regval;

	if(KNC_A_STEP == mic_ctx->bi_stepping) {
		ctrl_regval.value = pm_reg_read(mic_ctx,SBOX_PCU_CONTROL);
		ctrl_regval.bits.enable_mclk_pl_shutdown =  (set ? 1: 0);
		pm_reg_write(ctrl_regval.value, mic_ctx, SBOX_PCU_CONTROL);
	} else {
		uos_ctrl_regval.value = pm_reg_read(mic_ctx,
				SBOX_UOS_PCUCONTROL);
		uos_ctrl_regval.bits.enable_mclk_pll_shutdown = (set ? 1: 0);
		pm_reg_write(uos_ctrl_regval.value,
				mic_ctx, SBOX_UOS_PCUCONTROL);
	}
}

void program_prevent_C3Exit(mic_ctx_t *mic_ctx, bool set)
{
	sbox_pcu_ctrl_t ctrl_regval;
	ctrl_regval.value = pm_reg_read(mic_ctx,SBOX_PCU_CONTROL);
	ctrl_regval.bits.prevent_auto_c3_exit = (set ? 1: 0);
	pm_reg_write(ctrl_regval.value, mic_ctx, SBOX_PCU_CONTROL);

}

int pm_pc3_to_pc6_entry(mic_ctx_t *mic_ctx)
{
	int err;
	sbox_pcu_ctrl_t ctrl_regval;
	gbox_pm_control pmctrl_reg;
	sbox_core_freq_t core_freq_reg;

	if ((get_card_state(mic_ctx)) != PM_IDLE_STATE_PC3) {
		PM_DEBUG("Card not ready to go to PC6. \n");
		err = -EAGAIN;
		goto exit;
	}

	if (atomic_cmpxchg(&mic_ctx->gate_interrupt, 0, 1) == 1) {
		PM_DEBUG("Cannot gate interrupt handler while it is in use\n");
		err = -EFAULT;
		goto exit;
	}

	program_prevent_C3Exit(mic_ctx, true);
	program_mclk_shutdown(mic_ctx, true);

	/* Wait for uos to become idle. */
	if (!hw_idle(mic_ctx)) {
		program_mclk_shutdown(mic_ctx, false);
		if (!hw_idle(mic_ctx)) {
			program_prevent_C3Exit(mic_ctx, false);
			PM_DEBUG("Card not ready to go to PC6. \n");
			err = -EAGAIN;
			goto intr_ungate;
		} else {
			program_mclk_shutdown(mic_ctx, true);
		}
	}

	pmctrl_reg.value = pm_reg_read(mic_ctx, GBOX_PM_CTRL);
	pmctrl_reg.bits.in_pckgc6 = 1;
	pm_reg_write(pmctrl_reg.value, mic_ctx, GBOX_PM_CTRL);

	core_freq_reg.value = pm_reg_read(mic_ctx, SBOX_COREFREQ);
	core_freq_reg.bits.booted = 0;
	pm_reg_write(core_freq_reg.value, mic_ctx, SBOX_COREFREQ);

	udelay(500);

	ctrl_regval.value = pm_reg_read(mic_ctx, SBOX_PCU_CONTROL);
	ctrl_regval.bits.grpB_pwrgood_mask = 1;
	pm_reg_write(ctrl_regval.value, mic_ctx, SBOX_PCU_CONTROL);

	err = set_vid_knc(mic_ctx, 0);
	if (err != 0) {
		PM_DEBUG("Aborting PC6 entry...Failed to set VID\n");
		restore_pc6_registers(mic_ctx, true);
		goto intr_ungate;
	}

	mic_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_PC6;
	set_host_state(mic_ctx, PM_IDLE_STATE_PC6);

	dma_prep_suspend(mic_ctx->dma_handle);

	PM_PRINT("Node %d entered PC6\n",
		mic_get_scifnode_id(mic_ctx));

	return err;

intr_ungate:
	atomic_set(&mic_ctx->gate_interrupt, 0);
	tasklet_schedule(&mic_ctx->bi_dpc);
exit:
	return err;
}

/*
 * pm_pc6_exit:
 *
 * Execute pc6 exit for a node.
 * mic_ctx: The driver context of the node.
 */
int pm_pc6_exit(mic_ctx_t *mic_ctx)
{

	int err = 0;

	sbox_host_pm_state_t hpmstate_regval;
	sbox_pcu_ctrl_t ctrl_regval;
	uint8_t tdp_vid = 0;
	uint8_t is_pll_locked;
	uint32_t wait_cnt;
	int i;


	if (!check_host_state(mic_ctx, PM_IDLE_STATE_PC6)) {
		PM_DEBUG("Wrong Host PM state. State = %d\n",
				 get_host_state(mic_ctx));
		err = -EINVAL;
		goto restore_registers;
	}

	hpmstate_regval.value = pm_reg_read(mic_ctx, SBOX_HOST_PMSTATE);
	tdp_vid = hpmstate_regval.bits.tdp_vid;
	PM_DEBUG("TDP_VID value obtained from Host PM Register = %d",tdp_vid);

	PM_DEBUG("Setting voltage to %dV using SVID Control\n",tdp_vid);
	err = set_vid_knc(mic_ctx, tdp_vid);
	if (err != 0) {
		printk(KERN_ERR "%s Failed PC6 entry...error in setting VID\n", 
				__func__);
		goto restore_registers;
	}

	ctrl_regval.value = pm_reg_read(mic_ctx, SBOX_PCU_CONTROL);

	program_mclk_shutdown(mic_ctx, false);
	program_prevent_C3Exit(mic_ctx, false);

	for(wait_cnt = 0; wait_cnt < 200; wait_cnt++) {
		ctrl_regval.value = pm_reg_read(mic_ctx,SBOX_PCU_CONTROL);
		is_pll_locked = ctrl_regval.bits.mclk_pll_lock;
		if(likely(is_pll_locked))
				break;
		msleep(10);
	}

	if(wait_cnt >= 200) {
		PM_DEBUG("mclk_pll_locked bit is not set.\n");
		err = -EAGAIN;
		goto restore_registers;
	}

	ctrl_regval.bits.grpB_pwrgood_mask = 0;
	pm_reg_write(ctrl_regval.value, mic_ctx, SBOX_PCU_CONTROL);

	if (!hw_active(mic_ctx)) {
		PM_DEBUG("Timing out waiting for hw to become active");
		goto restore_registers;
	}

	for(wait_cnt = 0; wait_cnt < 200; wait_cnt++) {
		if ((get_card_state(mic_ctx)) == PM_IDLE_STATE_PC0)
			break;
		msleep(10);
	}

	if(wait_cnt >= 200) {
		PM_DEBUG("PC6 Exit not complete.\n");
		err = -EFAULT;
		goto restore_registers;
	}

	mic_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_PC0;

	for (i = 0; i <= mic_data.dd_numdevs; i++) {
		if (micscif_get_nodedep(mic_get_scifnode_id(mic_ctx), i) ==
				DEP_STATE_DISCONNECTED) {
			micscif_set_nodedep(mic_get_scifnode_id(mic_ctx), i,
					DEP_STATE_DEPENDENT);
		}
	}

	PM_PRINT("Node %d exited PC6\n",
			mic_get_scifnode_id(mic_ctx));
	goto exit;

restore_registers:
	restore_pc6_registers(mic_ctx, false);
exit:
	atomic_set(&mic_ctx->gate_interrupt, 0);
	tasklet_schedule(&mic_ctx->bi_dpc);
	return err;
}

/*
 * setup_pm_dependency:
 *
 * Function sets up the dependency matrix by populating
 * the matrix with node depency information.
 *
 * Returns 0 on success. Appropriate error on failure.
 */
int setup_pm_dependency(void){
	int err = 0;
	uint16_t i;
	uint16_t j;
	mic_ctx_t *mic_ctx;

	for (i = 0; i < mic_data.dd_numdevs; i++) {
		mic_ctx = get_per_dev_ctx(i);
		if (!mic_ctx) {
			PM_DEBUG("Failed to retrieve driver context\n");
			return -EFAULT;
		}
		if (mic_ctx->micpm_ctx.idle_state ==
			PM_IDLE_STATE_PC3_READY) {
			for (j = 0; j < mic_data.dd_numdevs; j++) {
				if (micscif_get_nodedep(mic_get_scifnode_id(mic_ctx),j+1) ==
						DEP_STATE_DEPENDENT) {
					micscif_set_nodedep(mic_get_scifnode_id(mic_ctx),j+1,
							DEP_STATE_DISCONNECT_READY);
				}
			}
		}
	}
	return err;
}

/*
 * teardown_pm_dependency
 *
 * Function resets dependency matrix by removing all depenendy info
 * from it.
 *
 * Returns 0 on success. Appropriate error on failure.
 */
int teardown_pm_dependency(void) {
	int err = 0;
	int i;
	int j;

	for (i = 0; i < mic_data.dd_numdevs; i++) {
		for (j = 0; j < mic_data.dd_numdevs; j++) {

			if (micscif_get_nodedep(i+1,j+1) == DEP_STATE_DISCONNECT_READY) {
				micscif_set_nodedep(i+1,j+1, DEP_STATE_DEPENDENT);
			}
		}
	}
	return err;
}

/*
 * revert_idle_entry_trasaction:
 *
 * @node_disc_bitmask: Bitmask of nodes which were involved in the
 *  transaction
 *
 *  Function Reverts idle state changes made to nodes when an idle
 *  state trasaction fails.
 */
int revert_idle_entry_trasaction(uint8_t *node_disc_bitmask) {
	int err = 0;
	mic_ctx_t *node_ctx;
	uint32_t node_id = 0;

	for(node_id = 0; node_id <= ms_info.mi_maxid;  node_id++) {
		if (node_id == SCIF_HOST_NODE)
			continue;

		if (!get_nodemask_bit(node_disc_bitmask, node_id))
			continue;

		node_ctx = get_per_dev_ctx(node_id - 1);
		if (!node_ctx) {
			PM_DEBUG("Failed to retrieve node context.");
			err = -EINVAL;
			goto exit;
		}

		if (node_ctx->micpm_ctx.idle_state == PM_IDLE_STATE_PC3) {
			err = pm_pc3_exit(node_ctx);
			if (err) {
				PM_DEBUG("Wakeup of Node %d failed. Node is lost"
					" and is to be disconnected",node_id);
				node_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_LOST;
				/* Since node is lost, ref_cnt increment(decement) through the
				* pm_get(put)_reference interface is prevented by idle_state.
				* We still need to ensure the ref_cnt iself is reset
				* back to 0 so that pm_get(put)_reference will work after the
				* lost node interface recovers the node. */
				atomic_set(&node_ctx->micpm_ctx.pm_ref_cnt, 0);
			}
		}
	}
exit:
	return err;
}

/* pm_node_disconnect
 *
 * Called during idlestate entry.
 *
 * Function checks the pm_ref_cnt and returns ACK
 * or NACK depending on the pm_ref_cnt value.
 */
int pm_node_disconnect(uint8_t *nodemask) {

	uint32_t node_id;
	mic_ctx_t *mic_ctx;
	int ret = 0;
	int err = 0;

	for (node_id = 0; node_id <= ms_info.mi_maxid; node_id++) {
		if (node_id == SCIF_HOST_NODE)
			continue;

		if (!get_nodemask_bit(nodemask, node_id))
			continue;

		mic_ctx = get_per_dev_ctx(node_id - 1);
		if (!mic_ctx) {
			set_nodemask_bit(nodemask, node_id, 0);
			return -EAGAIN;
		}

		if (mic_ctx->state != MIC_ONLINE) {
			set_nodemask_bit(nodemask, node_id, 0);
			return -EAGAIN;
		}

		ret = atomic_cmpxchg(&mic_ctx->micpm_ctx.pm_ref_cnt, 
			0, PM_NODE_IDLE);
		if (((ret != 0) && (ret != PM_NODE_IDLE))
			|| atomic_read(&mic_data.dd_pm.wakeup_in_progress)) {
			set_nodemask_bit(nodemask, node_id, 0);
			return -EAGAIN;
		}
	}

	return err;
}

/*
 * pm_pc3_entry:
 *
 * Execute pc3 entry for a node.
 * mic_ctx: The driver context of the node.
 */
int pm_pc3_entry(mic_ctx_t *mic_ctx)
{
	int err = 0;
	if (mic_ctx == NULL) {
		err = -EINVAL;
		goto exit;
	}

	if (((!check_host_state(mic_ctx, PM_IDLE_STATE_PC0))) ||
		(mic_ctx->micpm_ctx.idle_state != PM_IDLE_STATE_PC0)) {
		PM_DEBUG("Wrong host state. register state = %d"
			" idle state = %d\n", get_host_state(mic_ctx),
			mic_ctx->micpm_ctx.idle_state);
		goto send_wakeup;
	}

	/* cancel pc6 entry work that may be scheduled. We need to
 	* do this either here or after a pervious pc3 exit */
	cancel_delayed_work_sync(&mic_ctx->micpm_ctx.pc6_entry_work);

	if ((mic_ctx->micpm_ctx.con_state != PM_CONNECTED) ||
		(!mic_ctx->micpm_ctx.pc3_enabled))
		goto send_wakeup;

	mic_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_PC3_READY;
	err = do_idlestate_entry(mic_ctx);
	if (err)
		goto exit;
	if ((mic_ctx->micpm_ctx.pc6_enabled) &&
		(KNC_C_STEP <= mic_ctx->bi_stepping) &&
		(KNC_B1_STEP != mic_ctx->bi_stepping)) {
			queue_delayed_work(mic_ctx->micpm_ctx.pc6_entry_wq, 
			&mic_ctx->micpm_ctx.pc6_entry_work, 
			mic_ctx->micpm_ctx.pc6_timeout*HZ);
	}

	goto exit;

send_wakeup:
	mutex_lock(&mic_data.dd_pm.pm_idle_mutex);
	pm_pc3_exit(mic_ctx);
	mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
exit:
	return err;
}

/*
 * pm_pc3_exit: 
 * Calling function needs to grab idle_state mutex.
 *
 * Execute pc3 exit for a node.
 * mic_ctx: The driver context of the node.
 */
int pm_pc3_exit(mic_ctx_t *mic_ctx)
{
	int err;
	int wait_cnt;

	WARN_ON(!mutex_is_locked(&mic_data.dd_pm.pm_idle_mutex));
	mic_send_pm_intr(mic_ctx);
	for (wait_cnt = 0; wait_cnt < PC3_EXIT_WAIT_COUNT; wait_cnt++) {
		if (check_card_state(mic_ctx, PM_IDLE_STATE_PC0))
			break;
		msleep(1);
	}


	if(wait_cnt >= PC3_EXIT_WAIT_COUNT) {
		PM_DEBUG("Syncronization with card failed."
			" Node is lost\n");
		err = -EFAULT;
		goto exit;
	}

	set_host_state(mic_ctx, PM_IDLE_STATE_PC0);
	mic_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_PC0;
	PM_DEBUG("Node %d exited PC3\n", mic_get_scifnode_id(mic_ctx));

	return 0;
exit:
	return err;
}

/*
 * do_idlestate_entry:
 *
 * Function to start the idle state entry transaction for a node. Puts a node
 * and all the nodes that are dependent on this node to idle state if
 * it is possible.
 *
 * mic_ctx: The device context of node that needs to be put in idle state
 * Returs 0 in success. Appropriate error code on failure
 */
int do_idlestate_entry(mic_ctx_t *mic_ctx)
{
	int err = 0;
	uint32_t node_id = 0;
	mic_ctx_t *node_ctx;
	uint8_t *nodemask_buf;

	if(!mic_ctx)
		return -EINVAL;

	mutex_lock(&mic_data.dd_pm.pm_idle_mutex);

	if ((err = setup_pm_dependency())) {
		PM_DEBUG("Failed to set up PM specific dependencies");
		goto unlock;
	}

	nodemask_buf = (uint8_t *)
		kzalloc(mic_ctx->micpm_ctx.nodemask.len, GFP_KERNEL);
	if(!nodemask_buf) {
		PM_DEBUG("Error allocating nodemask buffer\n");
		err = ENOMEM;
		goto dep_teardown;
	}

	err = micscif_get_deactiveset(mic_get_scifnode_id(mic_ctx),
		nodemask_buf, 1);
	if (err) {
		PM_DEBUG("Node disconnection failed "
			"during deactivation set calculation");
		goto free_buf;
	}

	print_nodemaskbuf(nodemask_buf);

	if ((err = micscif_disconnect_node(mic_get_scifnode_id(mic_ctx),
			nodemask_buf, DISCONN_TYPE_POWER_MGMT))) {
		PM_DEBUG("SCIF Node disconnect failed. err: %d", err);
		goto free_buf;
	}

	if ((err = pm_node_disconnect(nodemask_buf))) {
		PM_DEBUG("PM Node disconnect failed. err = %d\n", err);
		goto free_buf;
	}

	if ((err = micvcons_pm_disconnect_node(nodemask_buf,
		DISCONN_TYPE_POWER_MGMT))) {
		PM_DEBUG("VCONS Node disconnect failed. err = %d\n", err);
		goto free_buf;
	}

	for(node_id = 0; node_id <= ms_info.mi_maxid;  node_id++) {
		if (node_id == SCIF_HOST_NODE)
			continue;
		if (!get_nodemask_bit(nodemask_buf, node_id))
			continue;
		node_ctx = get_per_dev_ctx(node_id - 1);
		if (!node_ctx) {
			PM_DEBUG("Failed to retrieve node context.");
			err = -EINVAL;
			goto revert;
		}

		if (node_ctx->micpm_ctx.idle_state ==
			PM_IDLE_STATE_PC3_READY) {
			set_host_state(node_ctx, PM_IDLE_STATE_PC3);
			node_ctx->micpm_ctx.idle_state =
				PM_IDLE_STATE_PC3;
			PM_DEBUG("Node %d entered PC3\n",
				mic_get_scifnode_id(node_ctx));
		} else {
			PM_DEBUG("Invalid idle state \n");
			err = -EINVAL;
			goto revert;
		}
	}

revert:
	if (err)
		revert_idle_entry_trasaction(nodemask_buf);
free_buf:
	kfree(nodemask_buf);
dep_teardown:
	teardown_pm_dependency();
unlock:
	if (err && (mic_ctx->micpm_ctx.idle_state != PM_IDLE_STATE_PC0))
		pm_pc3_exit(mic_ctx);

	mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
	return err;
}

/**
 * is_idlestate_exit_needed:
 *
 * @node_id[in]: node to wakeup.
 *
 * Method responsible for checking if idle state exit is required
 * In some situation we would like to know whether node is idle or not before
 * making decision to bring the node out of idle state.
 * For example - Lost node detection.
 * returns false if the node is not in IDLE state, returns true otherwise
 */
int
is_idlestate_exit_needed(mic_ctx_t *mic_ctx)
{
	int ret = 0;
	mutex_lock(&mic_data.dd_pm.pm_idle_mutex);

	switch (mic_ctx->micpm_ctx.idle_state)
	{
		case PM_IDLE_STATE_PC0:
		case PM_IDLE_STATE_LOST:
			break;
		case PM_IDLE_STATE_PC3:
		case PM_IDLE_STATE_PC3_READY:
		case PM_IDLE_STATE_PC6:
		{
			ret = 1;
			break;
		}
		default:
			ret = 1;
	}

	mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
	return ret;
}

/* do_idlestate_exit:
 *
 * Initiate idle state exits for nodes specified
 * by the bitmask.
 *
 * mic_ctx: The device context.
 * get_ref: Set to true if the entity that wants to wake
 * a node up also wantes to get a reference to the node.
 *
 * Returs 0 on success. Appropriate error on failure.
 *
 */
int do_idlestate_exit(mic_ctx_t *mic_ctx, bool get_ref) {
	int err = 0;
	uint32_t node_id = 0;
	mic_ctx_t *node_ctx;
	uint8_t *nodemask_buf;

	if(!mic_ctx)
		return -EINVAL;

	might_sleep();
	/* If the idle_state_mutex is already obtained by another thread
	 * try to wakeup the thread which MAY be waiting for REMOVE_NODE
	 * responses. This way, we give priority to idle state exits than
	 * idle state entries.
	 */
	if (!mutex_trylock(&mic_data.dd_pm.pm_idle_mutex)) {
		atomic_inc(&mic_data.dd_pm.wakeup_in_progress);
		wake_up(&ms_info.mi_disconn_wq);
		mutex_lock(&mic_data.dd_pm.pm_idle_mutex);
		atomic_dec(&mic_data.dd_pm.wakeup_in_progress);
	}

	nodemask_buf = (uint8_t *)kzalloc(mic_ctx->micpm_ctx.nodemask.len, GFP_KERNEL);
	if(!nodemask_buf) {
		PM_DEBUG("Error allocating nodemask buffer\n");
		mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
		err = ENOMEM;
		goto abort_node_wake;
	}

	if ((err = micscif_get_activeset(mic_get_scifnode_id(mic_ctx), nodemask_buf))) {
		PM_DEBUG("Node connect failed during Activation set calculation for node\n");
		mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
		err = -EINVAL;
		goto free_buf;
	}

	print_nodemaskbuf(nodemask_buf);

	for(node_id = 0; node_id <= ms_info.mi_maxid;  node_id++) {
		if (node_id == SCIF_HOST_NODE)
			continue;

		if (!get_nodemask_bit(nodemask_buf, node_id))
			continue;

		node_ctx = get_per_dev_ctx(node_id - 1);
		if (!node_ctx) {
			mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
			goto free_buf;
		}

		switch (node_ctx->micpm_ctx.idle_state) {
		case PM_IDLE_STATE_PC3:
		case PM_IDLE_STATE_PC3_READY:
			if ((err = pm_pc3_exit(node_ctx))) {
				PM_DEBUG("Wakeup of Node %d failed."
					"Node to be disconnected",node_id);
				set_nodemask_bit(nodemask_buf, node_id, 0);
				node_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_LOST;
				/* Since node is lost, ref_cnt increment(decement) through the
				* pm_get(put)_reference interface is prevented by idle_state.
				* We still need to ensure the ref_cnt iself is reset
				* back to 0 so that pm_get(put)_reference will work after the
				* lost node interface recovers the node. */
				atomic_set(&node_ctx->micpm_ctx.pm_ref_cnt, 0);
			} else {
				if ((mic_ctx == node_ctx) && get_ref)
					if (atomic_cmpxchg(&mic_ctx->micpm_ctx.pm_ref_cnt, PM_NODE_IDLE, 1) !=
							PM_NODE_IDLE)
						atomic_inc(&mic_ctx->micpm_ctx.pm_ref_cnt);
			}
			break;
		case PM_IDLE_STATE_PC6:
			if ((err = pm_pc6_exit(node_ctx))) {
				PM_DEBUG("Wakeup of Node %d failed."
					"Node to be disconnected",node_id);
				set_nodemask_bit(nodemask_buf, node_id, 0);
				node_ctx->micpm_ctx.idle_state = PM_IDLE_STATE_LOST;
				/* Since node is lost, ref_cnt increment(decement) through the
				* pm_get(put)_reference interface is prevented by idle_state.
				* We still need to ensure the ref_cnt iself is reset
				* back to 0 so that pm_get(put)_reference will work after the
				* lost node interface recovers the node. */
				atomic_set(&node_ctx->micpm_ctx.pm_ref_cnt, 0);
			} else {
				if ((mic_ctx == node_ctx) && get_ref)
					if (atomic_cmpxchg(&mic_ctx->micpm_ctx.pm_ref_cnt, PM_NODE_IDLE, 1) !=
							PM_NODE_IDLE)
						atomic_inc(&mic_ctx->micpm_ctx.pm_ref_cnt);
			}
			break;
		case PM_IDLE_STATE_PC0:
			PM_DEBUG("Node %d is in state %d "
					"and already out of package state.\n",node_id,
					node_ctx->micpm_ctx.idle_state);
			if ((mic_ctx == node_ctx) && get_ref)
					if (atomic_cmpxchg(&mic_ctx->micpm_ctx.pm_ref_cnt, PM_NODE_IDLE, 1) !=
							PM_NODE_IDLE)
						atomic_inc(&mic_ctx->micpm_ctx.pm_ref_cnt);
			break;
		default:
			PM_DEBUG("Invalid idle state of node %d."
					" State = %d \n", node_id,
					node_ctx->micpm_ctx.idle_state);
			mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
			err = -ENODEV;
			goto free_buf;
		}
	}

	/* Idle state exit of nodes are complete.
	 * Set the register state now for those nodes
	 * that are successfully up.
	 */
	for(node_id = 0; node_id <= ms_info.mi_maxid;  node_id++) {
		if (node_id == SCIF_HOST_NODE)
			continue;

		if (!get_nodemask_bit(nodemask_buf, node_id))
			continue;

		node_ctx = get_per_dev_ctx(node_id - 1);
		if (!node_ctx) {
			PM_DEBUG("Failed to retrieve node context.");
			continue;
		}


		if (node_ctx->micpm_ctx.idle_state == PM_IDLE_STATE_PC0)
			set_host_state(node_ctx, PM_IDLE_STATE_PC0);
	}

	mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
free_buf:
	kfree(nodemask_buf);
abort_node_wake:
	return err;
}

int pc6_entry_start(mic_ctx_t *mic_ctx) {

	int err = 0;

	if (mic_ctx->micpm_ctx.idle_state == PM_IDLE_STATE_PC0) {
		PM_DEBUG("Node not in PC3\n");
		err = -EFAULT;
		goto exit;
	}

	mutex_lock(&mic_data.dd_pm.pm_idle_mutex);

	if (mic_ctx->micpm_ctx.idle_state != PM_IDLE_STATE_PC3) {
		PM_DEBUG("PC6 transition failed. Node not in PC3\n");
		mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
		err = -EINVAL;
		goto exit;
	}

	if ((err = pm_pc3_to_pc6_entry(mic_ctx))) {
		PM_DEBUG("PC6 transition from PC3 failed for node %d\n",
				mic_get_scifnode_id(mic_ctx));
		mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
		goto exit;
	}
	mutex_unlock(&mic_data.dd_pm.pm_idle_mutex);
exit:
	return err;

}

/*
 * mic_get_scifnode_id:
 *
 * Function to retrieve node id of a scif node.
 *
 * mic_ctx: The driver context of the specified node.
 * Returns the scif node_id of the specified node.
 */
uint32_t mic_get_scifnode_id(mic_ctx_t *mic_ctx) {
	/* NOTE: scif node_id cannot assumed to be a simple increment
	 * of the bi_id of the driver context. This function is really
	 * a placeholder for the board_id to node_id conversion that
	 * we need to do in the host driver.
	 */
	return (uint32_t)mic_ctx->bi_id + 1;
}
