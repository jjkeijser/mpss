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

/*
 * -----------------------------------------
 * SCIF IOCTL interface information
 * -----------------------------------------
 */
#if defined(_WIN32) && !defined(_WIN64)
#define ptr64_t __ptr64
#else
#define ptr64_t
#endif

/**
 * The purpose of SCIF_VERSION is to check for compatibility between host and
 * card SCIF modules and also between SCIF driver and libscif. This version
 * should be incremented whenever a change is made to SCIF that affects the
 * interface between SCIF driver and libscif or between the card and host SCIF
 * driver components.
 */
#define SCIF_VERSION		1

/**
 * struct scifioctl_connect:
 *
 * \param self			used to read back the assigned portID
 * \param peer			destination node and port to connect to
 *
 * This structure is used for CONNECT IOCTL.
 */
struct scifioctl_connect {
	struct scif_portID	self;
	struct scif_portID	peer;
};


/**
 * struct scifioctl_accept:
 *
 * \param flags			flags
 * \param peer			global id of peer endpoint
 * \param newepd		new connected endpoint descriptor
 *
 * This structure is used for SCIF_ACCEPTREQ IOCTL.
 */
struct scifioctl_accept {
	int			flags;
	struct scif_portID	peer;
	void			* ptr64_t endpt;
};

/**
 * struct scifioctl_msg:
 *
 * \param msg			message buffer address
 * \param len			message length
 * \param flags			flags
 * \param out_len		Number of bytes sent/received.
 *
 * This structure is used for SCIF_SEND/SCIF_RECV IOCTL.
 */
struct scifioctl_msg {
	void		* ptr64_t msg;
	int		len;
	int		flags;
	int		out_len;
};

/**
 * struct scifioctl_reg:
 *
 * \param addr starting virtual address
 * \param len			length of range
 * \param offset		offset of window
 * \param prot			read/write protection
 * \param flags			flags
 * \param out_len		offset returned.
 *
 * This structure is used for SCIF_REG IOCTL.
 */
struct scifioctl_reg {
	void		* ptr64_t addr;
	uint64_t	len;
	off_t		offset;
	int		prot;
	int		flags;
	off_t		out_offset;
};

/**
 * struct scifioctl_unreg:
 *
 * \param offset		start of range to unregister
 * \param len			length of range to unregister
 *
 * This structure is used for SCIF_UNREG IOCTL.
 */
struct scifioctl_unreg {
	off_t		offset;
	uint64_t	len;
};

/**
 * struct scifioctl_copy:
 *
 * \param loffset	offset in local registered address space to/from
which to copy
 * \param len		length of range to copy
 * \param roffset	offset in remote registered address space to/from
which to copy
 * \param addr		user virtual address to/from which to copy
 * \param flags		flags
 *
 * This structure is used for SCIF_READFROM, SCIF_WRITETO, SCIF_VREADFROM
and
 * SCIF_VREADFROM IOCTL's.
 */
struct scifioctl_copy {
	off_t		loffset;
	uint64_t	len;
	off_t		roffset;
	uint8_t		* ptr64_t addr;
	int		flags;
};

/**
 * struct scifioctl_fence_mark:
 *
 * \param flags		flags
 * \param mark		Fence handle returned by reference.
 *
 * This structure is used from SCIF_FENCE_MARK IOCTL.
 */
struct scifioctl_fence_mark {
	int             flags;
	int             *mark;
};

/**
 * struct scifioctl_fence_signal:
 *
 * \param loff		local offset
 * \param lval		local value to write to loffset
 * \param roff		remote offset
 * \param rval		remote value to write to roffset
 * \param flags		flags
 *
 * This structure is used for SCIF_FENCE_SIGNAL IOCTL.
 */
struct scifioctl_fence_signal {
	off_t loff;
	uint64_t lval;
	off_t roff;
	uint64_t rval;
	int flags;
};

/**
 * struct scifioctl_nodeIDs:
 *
 * \param nodes		pointer to an array of nodeIDs
 * \param len		length of array
 * \param self		ID of the current node
 *
 * This structure is used for the SCIF_GET_NODEIDS ioctl
 */
struct scifioctl_nodeIDs {
	uint16_t * ptr64_t nodes;
	int	 len;
	uint16_t * ptr64_t self;
};


#define SCIF_BIND		_IOWR('s', 1, int *)
#define SCIF_LISTEN		_IOW('s', 2, int)
#define SCIF_CONNECT		_IOWR('s', 3, struct scifioctl_connect *)
#define SCIF_ACCEPTREQ		_IOWR('s', 4, struct scifioctl_accept *)
#define SCIF_ACCEPTREG		_IOWR('s', 5, void *)
#define SCIF_SEND		_IOWR('s', 6, struct scifioctl_msg *)
#define SCIF_RECV		_IOWR('s', 7, struct scifioctl_msg *)
#define SCIF_REG		_IOWR('s', 8, struct scifioctl_reg *)
#define SCIF_UNREG		_IOWR('s', 9, struct scifioctl_unreg *)
#define SCIF_READFROM		_IOWR('s', 10, struct scifioctl_copy *)
#define SCIF_WRITETO		_IOWR('s', 11, struct scifioctl_copy *)
#define SCIF_VREADFROM		_IOWR('s', 12, struct scifioctl_copy *)
#define SCIF_VWRITETO		_IOWR('s', 13, struct scifioctl_copy *)
#define SCIF_GET_NODEIDS	_IOWR('s', 14, struct scifioctl_nodeIDs *)
#define SCIF_FENCE_MARK		_IOWR('s', 15, struct scifioctl_fence_mark *)
#define SCIF_FENCE_WAIT		_IOWR('s', 16, int)
#define SCIF_FENCE_SIGNAL	_IOWR('s', 17, struct scifioctl_fence_signal *)

#define SCIF_GET_VERSION	_IO('s', 23)
