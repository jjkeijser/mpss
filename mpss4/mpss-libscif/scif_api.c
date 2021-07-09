/*
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 */

#ifndef WIN32
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif

#include "stdio.h"

#include "scif.h"
#include <linux/scif_ioctl.h>

#ifndef WIN32

#define DEVICE_NODE "/dev/scif"

#define __symver_tag SCIF

#define __symver(S, at, M, N) \
	__asm__ (".symver " __str(V(S, M, N)) "," __V(S, at, M, N));
#define __V(symbol, at, major, minor) \
	__str(symbol) at __str(__symver_tag) "_" __str(major) "." __str(minor)
#define __str(s) ___str(s)
#define ___str(s) #s

#ifndef GENMAP_PARSING_PASS
#define V(symbol, major, minor) \
	__ ## symbol ## _ ## major ## _ ## minor
#define compatible_version(symbol, major, minor) \
	__symver(symbol, "@", major, minor)
#define default_version(symbol, major, minor) \
	__symver(symbol, "@@", major, minor)
#define only_version(symbol, major, minor)
#endif

#else

#define DEVICE_NODE "\\\\.\\scif"

#define V(symbol, major, minor) symbol
#define compatible_version(symbol, major, minor)
#define default_version(symbol, major, minor)
#define only_version(symbol, major, minor)

#endif

#ifdef WIN32
static scif_epd_t scif_default_epd;
static void * ptr64_t scif_process_info;

BOOL WINAPI DllMain(
	__in HINSTANCE hinstDll,
	__in DWORD fdwReason,
	__in LPVOID lpvReserved)
{
	struct scifioctl_init_proc init;
	struct scifioctl_uninit_proc uninit;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		if ((scif_default_epd = scif_open()) < 0)
			return FALSE;
		if (ioctl(scif_default_epd, SCIF_IOCTL_INIT_PROC, &init) < 0) {
			scif_close(scif_default_epd);
			return FALSE;
		}
		scif_process_info = init.proc;
		break;

	case DLL_PROCESS_DETACH:
		uninit.proc = scif_process_info;
		ioctl(scif_default_epd, SCIF_IOCTL_UNINIT_PROC, &uninit);
		scif_close(scif_default_epd);
		break;
	}
	return TRUE;
}

MICACCESSAPI int * _dll_errno(void)
{
	// errno has to be called from within dll
	// as the errno is different in dll from the app
	return _errno();
}

MICACCESSAPI void _dll_set_errno(void)
{
	DWORD err = GetLastError();
	int _errno;

	switch (err) {
	case ERROR_ACCESS_DENIED:
		_errno = EPERM;
		break;
	case ERROR_FILE_NOT_FOUND:
		_errno = ENOENT;
		break;
	case ERROR_NOACCESS:
		_errno = EFAULT;
		break;
	case ERROR_NOT_ENOUGH_MEMORY:
		_errno = ENOMEM;
		break;
	case ERROR_INVALID_PARAMETER:
		_errno = EINVAL;
		break;
	case ERROR_INVALID_HANDLE:
		_errno = EBADF;
		break;
	case ERROR_DEV_NOT_EXIST:
		_errno = ENODEV;
		break;
	case ERROR_BUSY:
		_errno = EBUSY;
		break;
	case ERROR_OPERATION_ABORTED:
		_errno = EINTR;
		break;
	case ERROR_RETRY:
	case ERROR_REQUEST_ABORTED:
		_errno = EAGAIN;
		break;
	case ERROR_DISK_FULL:
		_errno = ENOSPC;
		break;
	case ERROR_CONNECTION_ACTIVE:
		_errno = EISCONN;
		break;
	case ERROR_CONNECTION_INVALID:
		_errno = ENOTCONN;
		break;
	case ERROR_CONNECTION_REFUSED:
		_errno = ECONNREFUSED;
		break;
	case ERROR_NETNAME_DELETED:
		_errno = ECONNRESET;
		break;
	case ERROR_TOO_MANY_NAMES:
		_errno = EADDRINUSE;
		break;
	case ERROR_DEVICE_NOT_CONNECTED:
		_errno = ENXIO;
		break;
	case ERROR_NOT_SUPPORTED:
		_errno = EOPNOTSUPP;
		break;
	default:
		if (err == ERROR_SUCCESS)
			_errno = 0;
		else
			_errno = EINVAL;
	}
	_set_errno(_errno);
}
#endif

MICACCESSAPI scif_epd_t
scif_open(void)
{
#ifndef WIN32
	scif_epd_t fd;

	if ((fd = open(DEVICE_NODE, O_RDWR)) < 0)
		return (scif_epd_t)-1;

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
		close(fd);
		return (scif_epd_t)-1;
	}
	return fd;
#else
	return open(DEVICE_NODE, O_RDWR);
#endif
}
only_version(scif_open, 0, 0)

MICACCESSAPI int
scif_close(scif_epd_t epd)
{
#ifdef WIN32
	int ret;
	__try {
		ret = close(epd);
	}
	__except(EXCEPTION_INVALID_HANDLE) {
		//
	}
	if (ret!=0) {
#ifdef MICACCESSAPI_EXPORTS
		_dll_set_errno();
#endif
	}
	return ret;
#else
	if (close(epd))
		return -1;
	return 0;
#endif
}
only_version(scif_close, 0, 0)

MICACCESSAPI int
scif_bind(scif_epd_t epd, uint16_t pn)
{
	if (ioctl(epd, SCIF_BIND, &pn) < 0)
		return -1;

	return pn;
}
only_version(scif_bind, 0, 0)

MICACCESSAPI int
scif_listen(scif_epd_t epd, int backlog)
{
#ifndef WINDOWS
	if (ioctl(epd, SCIF_LISTEN, backlog) < 0)
#else
	if (ioctl(epd, SCIF_LISTEN, (void *)backlog) < 0)
#endif
		return -1;

	return 0;
}
only_version(scif_listen, 0, 0)

MICACCESSAPI int
scif_connect(scif_epd_t epd, struct scif_portID *dst)
{
	struct scifioctl_connect req;

	if (!dst) {
		errno = EINVAL;
		return -1;
	}

	req.peer.node = dst->node;
	req.peer.port = dst->port;

	if (ioctl(epd, SCIF_CONNECT, &req) < 0)
		return -1;

	return req.self.port;
}
only_version(scif_connect, 0, 0)

MICACCESSAPI int
scif_accept(scif_epd_t epd, struct scif_portID *peer, scif_epd_t *newepd, int flags)
{
	struct scifioctl_accept req;
	scif_epd_t newfd;
	int errno_save;

	if (!peer || !newepd) {
		errno = EINVAL;
		return -1;
	}

	// Create a new file descriptor link it to the new connection
#ifndef _WIN32
	if ((newfd = open(DEVICE_NODE, O_RDWR)) < 0)
		return -1;
#else
	if ((newfd = scif_open()) == SCIF_OPEN_FAILED)
		return -1;
#endif

	req.flags = flags;

	// First get the accept connection completed
	if (ioctl(epd, SCIF_ACCEPTREQ, &req) < 0) {
		errno_save = errno;
		close(newfd);
		errno = errno_save;
		return -1;
	}

	if (ioctl(newfd, SCIF_ACCEPTREG, &req.endpt) < 0) {
		errno_save = errno;
		close(newfd);
		errno = errno_save;
		return -1;
	}

	*newepd = newfd;
	peer->node = req.peer.node;
	peer->port = req.peer.port;
	return 0;
}
only_version(scif_accept, 0, 0)

MICACCESSAPI int
scif_send(scif_epd_t epd, void *msg, int len, int flags)
{
	struct scifioctl_msg send_msg =
#ifndef _WIN32
		{ .msg = (__u64)(uintptr_t)msg, .len = len, .flags = flags };
#else
		{msg, len, flags };
#endif
	if (ioctl(epd, SCIF_SEND, &send_msg) < 0)
		return -1;
	return send_msg.out_len;
}
only_version(scif_send, 0, 0)

MICACCESSAPI int
scif_recv(scif_epd_t epd, void *msg, int len, int flags)
{
	struct scifioctl_msg recv_msg =
#ifndef _WIN32
		{ .msg = (__u64)(uintptr_t)msg, .len = len, .flags = flags };
#else
		{msg, len, flags };
#endif

	if (ioctl(epd, SCIF_RECV, &recv_msg) < 0)
		return -1;
	return recv_msg.out_len;
}
only_version(scif_recv, 0, 0)

MICACCESSAPI off_t
scif_register(scif_epd_t epd, void *addr, size_t len, off_t offset,
				int prot, int flags)
{
	struct scifioctl_reg reg =
#ifndef _WIN32
	{ .addr = (__u64)(uintptr_t)addr, .len = len, .offset = offset,
			.prot = prot, .flags = flags };
#else
	{ addr, len, offset,
			prot, flags };
#endif

	if (ioctl(epd, SCIF_REG, &reg) < 0)
		return -1;
	return reg.out_offset;
}
only_version(scif_register, 0, 0)

MICACCESSAPI int
scif_unregister(scif_epd_t epd, off_t offset, size_t len)
{
	struct scifioctl_unreg unreg =
#ifndef _WIN32
		{ .len = len, .offset = offset};
#else
		{ offset, len };
#endif

	if (ioctl(epd, SCIF_UNREG, &unreg) < 0)
		return -1;
	return 0;
}
only_version(scif_unregister, 0, 0)

MICACCESSAPI void*
scif_mmap(void *addr, size_t len, int prot, int flags, scif_epd_t epd, off_t offset)
{
#ifndef _WIN32
	return mmap(addr, len, prot, (flags | MAP_SHARED), (int)epd, offset);
#else
	struct scifioctl_mmap mmap =
		{addr, len, prot, flags, epd, offset, scif_process_info};
	if (ioctl(epd, SCIF_IOCTL_MMAP, &mmap) < 0)
		return SCIF_MMAP_FAILED;
	return mmap.addr;
#endif
}
only_version(scif_mmap, 0, 0)

MICACCESSAPI int
scif_munmap(void *addr, size_t len)
{
#ifndef _WIN32
	return munmap(addr, len);
#else
	struct scifioctl_munmap munmap =
		{addr, len, scif_process_info};
	if (ioctl(scif_default_epd, SCIF_IOCTL_MUNMAP, &munmap) < 0)
		return -1;
	return 0;
#endif
}
only_version(scif_munmap, 0, 0)

MICACCESSAPI int
scif_readfrom(scif_epd_t epd, off_t loffset, size_t len, off_t roffset, int flags)
{
	struct scifioctl_copy copy =
#ifndef _WIN32
	{.loffset = loffset, .len = len, .roffset = roffset, .flags = flags };
#else
	{loffset, len, roffset, 0,flags };
#endif

	if (ioctl(epd, SCIF_READFROM, &copy) < 0)
		return -1;
	return 0;
}
only_version(scif_readfrom, 0, 0)

MICACCESSAPI int
scif_writeto(scif_epd_t epd, off_t loffset, size_t len, off_t roffset, int flags)
{
	struct scifioctl_copy copy =
#ifndef _WIN32
	{.loffset = loffset, .len = len, .roffset = roffset, .flags = flags };
#else
	{loffset, len, roffset, 0, flags };
#endif

	if (ioctl(epd, SCIF_WRITETO, &copy) < 0)
		return -1;
	return 0;
}
only_version(scif_writeto, 0, 0)

MICACCESSAPI int
scif_vreadfrom(scif_epd_t epd, void *addr, size_t len, off_t offset, int flags)
{
	struct scifioctl_copy copy =
#ifndef _WIN32
	{.addr = (__u64)(uintptr_t)addr, .len = len,
			.roffset = offset, .flags = flags };
#else
	{0, len, offset, addr, flags };
#endif

	if (ioctl(epd, SCIF_VREADFROM, &copy) < 0)
		return -1;
	return 0;
}
only_version(scif_vreadfrom, 0, 0)

MICACCESSAPI int
scif_vwriteto(scif_epd_t epd, void *addr, size_t len, off_t offset, int flags)
{
	struct scifioctl_copy copy =
#ifndef _WIN32
	{.addr = (__u64)(uintptr_t)addr, .len = len, .roffset = offset,
			.flags = flags };
#else
	{0, len, offset, addr, flags };
#endif

	if (ioctl(epd, SCIF_VWRITETO, &copy) < 0)
		return -1;
	return 0;
}
only_version(scif_vwriteto, 0, 0)

MICACCESSAPI int
scif_fence_mark(scif_epd_t epd, int flags, int *mark)
{
	struct scifioctl_fence_mark fence_mark =
#ifndef _WIN32
	{.flags = flags, .mark = (__u64)(uintptr_t)mark};
#else
	{flags, mark};
#endif

	if (ioctl(epd, SCIF_FENCE_MARK, &fence_mark) < 0)
		return -1;
	return 0;
}
only_version(scif_fence_mark, 0, 0)

MICACCESSAPI int
scif_fence_wait(scif_epd_t epd, int mark)
{
#ifndef WINDOWS
	if (ioctl(epd, SCIF_FENCE_WAIT, mark) < 0)
		return -1;
#else
	if (ioctl(epd, SCIF_FENCE_WAIT, (void*)mark) < 0)
		return -1;
#endif
	return 0;
}
only_version(scif_fence_wait, 0, 0)

MICACCESSAPI int
scif_fence_signal(scif_epd_t epd, off_t loff, uint64_t lval,
	off_t roff, uint64_t rval, int flags)
{
	struct scifioctl_fence_signal signal =
#ifndef _WIN32
	{.loff = loff, .lval = lval, .roff = roff,
		.rval = rval, .flags = flags};
#else
	{loff, lval, roff, rval, flags};
#endif

	if (ioctl(epd, SCIF_FENCE_SIGNAL, &signal) < 0)
		return -1;

	return 0;
}
only_version(scif_fence_signal, 0, 0)

MICACCESSAPI int
scif_get_nodeIDs(uint16_t *nodes, int len, uint16_t *self)
{
	scif_epd_t fd;
#ifndef _WIN32
	struct scifioctl_node_ids nodeIDs = {.nodes = (__u64)(uintptr_t)nodes,
				.len = len, .self = (__u64)(uintptr_t)self};
#else
	struct scifioctl_nodeIDs nodeIDs = {nodes, len, self};
#endif

#ifndef _WIN32
	if ((fd = open(DEVICE_NODE, O_RDWR)) < 0)
		return -1;
#else
	if ((fd = scif_open()) == SCIF_OPEN_FAILED)
		return -1;
#endif

	if (ioctl(fd, SCIF_GET_NODEIDS, &nodeIDs) < 0) {
		close(fd);
		return -1;
	}

	close(fd);

	return nodeIDs.len;
}
only_version(scif_get_nodeIDs, 0, 0)

#ifndef _WIN32
MICACCESSAPI int
scif_get_fd(scif_epd_t epd)
{
	return (int) epd;
}
only_version(scif_get_fd, 0, 0)
#endif

MICACCESSAPI int
scif_poll(struct scif_pollepd *ufds, unsigned int nfds, long timeout_msecs)
{
#ifndef _WIN32
	return poll((struct pollfd*)ufds, nfds, timeout_msecs);
#else
	struct scifioctl_poll scif_poll;
	unsigned int loop;
	int err = 0;
	int ioctlepd =-1;

	if (!ufds)
	{
		errno = EFAULT;
		return -1;
	}

	scif_poll.ufds = malloc(sizeof(struct scif_pollepd_internal)*nfds);
	if (!scif_poll.ufds) {
		errno = ENOMEM;
		return -1;
	}
	for (loop=0; loop<nfds; loop++ ){
		scif_poll.ufds[loop].epd = (int64_t)ufds[loop].epd;
		scif_poll.ufds[loop].events = ufds[loop].events;
		scif_poll.ufds[loop].revents = ufds[loop].revents;
	}
	scif_poll.timeout_msecs = timeout_msecs;
	scif_poll.nfds = nfds;
	scif_poll.ret = 0;


	for (loop=0; loop<nfds; loop++ ){
		if ( (int) ufds[loop].epd !=-1)
		{
			ioctlepd = (int) ufds[loop].epd;
			break;
		}
	}
	if (loop == nfds ) 
	{
		free(scif_poll.ufds);
		return -1;
	}

	//ioctl(ufds->epd, SCIF_POLL, &scif_poll);
	ioctl((HANDLE)ioctlepd, SCIF_POLL, &scif_poll);
	err = scif_poll.ret;
	for (loop=0; loop<nfds; loop++ ){
		ufds[loop].revents = scif_poll.ufds[loop].revents;
	}
	free(scif_poll.ufds);
	/* positive value indicates the total number of endpoint descriptors that have
     * been selected (that is, endpoint descriptors for which the revents member is
     * non-zero. A value of 0 indicates that the call timed out and no endpoint
     * descriptors have been selected, in user mode -1 is returned and
     * errno is set to indicate the error */
	if ( err <0 )
		return -1;
	return err;
#endif
}
only_version(scif_poll, 0, 0)
