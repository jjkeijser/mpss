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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "../include/scif.h"
#include "trace_capture.h"

#define BARRIER(epd, string) { \
        printf("%s\n", string); 						\
	if ((err = scif_send(epd, &control_msg, sizeof(control_msg), 1)) <= 0) { \
		printf("scif_send failed with err %d\n", errno); \
		fflush(stdout); \
		goto close; \
	} \
	if ((err = scif_recv(epd, &control_msg, sizeof(control_msg), 1)) <= 0) { \
		printf("scif_recv failed with err %d\n", errno); \
		fflush(stdout); \
		goto close; \
	} \
}

#if 0
// These are common to the Host App
// and the MIC driver Trace Capture Feature
// COMMON DEFINES START HERE
enum TRACE_COMMAND {
	TRACE_NOP = 100,
	TRACE_DATA,
	TRACE_HOST_READY,
	TRACE_DONE,
	TRACE_ERROR,
	TRACE_PRINT,
	TRACE_GET_FILE,
	TRACE_PAGE_READY,
	TRACE_REG_COMPLETE,
	TRACE_MEM_COMPLETE,
	TRACE_COMPLETE
};

#define TRACE_STATUS_OFFSET   8
#define TRACE_SIZE_OFFSET     12

// Enable/Disable Memory Test.
// This MUST be enabled simultaneously on Host App as well.
#define MIC_TRACE_CAPTURE_MEMORY_TEST 0

#if MIC_TRACE_CAPTURE_MEMORY_TEST
#define TRACE_CHECKSUM_OFFSET 16
#endif

#define TRACE_TRIGGER_OFFSET  20
#define TRACE_DATA_OFFSET     4096

// Types of Triggers - Refer to uOS Trace Capture Wiki for Usage
// Generic counter
#define TRACE_HOST_GENERIC_COUNTER 0x1
// Async Flip counter
#define TRACE_HOST_FRAME_COUNTER   0x2
// COMMON DEFINES END HERE
#endif

// End points for SCIF
//static scif_epd_t mictc_epd_cmd;
static scif_epd_t mictc_epd_data;

// SCIF ports - temp hack; move to scif.h
#define MICTC_SCIF_PORT_DATA 300

static volatile uint64_t *g_traceBufferStatusOffset = NULL;
static volatile uint64_t *g_traceBufferSizeOffset = NULL;
static volatile uint32_t *g_traceBufferDataOffset = NULL;
static volatile uint32_t *g_traceBufferTriggerOffset = NULL;

// This is an array of trigger numbers.  The value TRACE_EOL is ignored.
static uint32_t g_traceTriggers[TRACE_TRIGGER_MAX];

static struct scif_portID portID_data;
static scif_epd_t mictc_newepd;

static void *g_mictc_buffer_base;
static void *g_mictc_buffer_offset_xml;
static off_t g_mictc_buffer_offset_mem;

FILE *fp;

static
int open_scif_channels(void)
{
	int err;
	struct pollfd spollfd;
	int control_msg = 0;
	long scif_offset_dst;
	int timeout = 0;
	int page_count = 0;
	int i;

	if ((err = posix_memalign(&g_mictc_buffer_base, 0x1000, MICTC_MEM_BUFFER_SIZE))) {
		fprintf(stderr, "posix_memalign failed failed with %d\n", err);
		return 0;
	}
	// Data channel
	if ((mictc_epd_data = scif_open()) == SCIF_OPEN_FAILED) {
		fprintf(stderr, "scif_open failed with ENOMEM\n", errno);
		return 0;
	}

	if (scif_bind(mictc_epd_data, MICTC_SCIF_PORT_DATA) == -1) {
		fprintf(stderr, "scif_bind failed with error %d\n", errno);
		return 0;
	}

	portID_data.node = 1;
	portID_data.port = MICTC_SCIF_PORT_DATA;

	if (scif_listen(mictc_epd_data, 1) == -1) {
		fprintf(stderr, "scif_listen failed with error %d\n", errno);
		return 0;
	}

	while (1) {
		printf("scif_accept in poll mode until a connect request is found\n");
		err = 1;
		while (err) {
			spollfd.fd = scif_get_fd(mictc_epd_data);
			spollfd.events = POLLIN;
			spollfd.revents = 0;
			if ((err = poll(&spollfd, 1, -1)) < 0) {
				printf("poll failed with err %d\n", errno);
			}
			if (((err = scif_accept(mictc_epd_data, &portID_data, &mictc_newepd, 0)) < 0) && (errno != EAGAIN)) {
				printf("scif_accept failed with err %d\n", errno);
				return 0;
			}
		}

		printf("scif_accept from port %d complete\n", portID_data.port);

		if ((g_mictc_buffer_offset_mem = scif_register(mictc_newepd, g_mictc_buffer_base, MICTC_MEM_BUFFER_SIZE, 0,	// suggested_offset,
													   SCIF_PROT_READ | SCIF_PROT_WRITE, 0)) < 0) {
			fprintf(stderr, "scif_register failed with err %d\n", errno);
			return 0;
		}

		printf("After scif_register, g_mictc_buffer_offset_mem = %llx\n",
			   (unsigned long long)g_mictc_buffer_offset_mem);
		fflush(stdout);

		//  printf("Before scif_send\n");
		//  fflush(stdout);

		BARRIER(mictc_newepd, "before barrier");

		if ((err =
			 scif_send(mictc_newepd, &g_mictc_buffer_offset_mem, sizeof(g_mictc_buffer_offset_mem),
					   SCIF_SEND_BLOCK)) <= 0) {
			printf("scif_send failed with err %d\n", errno);
			fflush(stdout);
			goto close;
		}
		//    BARRIER(mictc_newepd, "scif_send");

		//  printf("scif_offset = %lx\n", scif_offset);
		//  fflush(stdout);

		printf("Before scif_recv\n");
		fflush(stdout);

		if ((err = scif_recv(mictc_newepd, &scif_offset_dst, sizeof(scif_offset_dst), SCIF_RECV_BLOCK)) <= 0) {
			printf("scif_recv failed with err %d\n", errno);
			fflush(stdout);
			goto close;
		}
		printf("scif_offset_dst = %lx\n", scif_offset_dst);

		printf("Before scif_mmap\n");

		if ((g_mictc_buffer_offset_xml = scif_mmap(0,	// physical address
												   MICTC_XML_BUFFER_SIZE,	// length
												   SCIF_PROT_READ | SCIF_PROT_WRITE,	// protection
												   0,	// flags
												   mictc_newepd,	// endpoint
												   scif_offset_dst)	// offset
			) == (void *)-1) {
			fprintf(stderr, "scif_mmap failed with err %d\n", errno);
			return 0;
		}

		g_traceBufferStatusOffset = (uint64_t *) (g_mictc_buffer_offset_xml + TRACE_STATUS_OFFSET);
		g_traceBufferSizeOffset = (uint64_t *) (g_mictc_buffer_offset_xml + TRACE_SIZE_OFFSET);
		g_traceBufferDataOffset = (uint32_t *) (g_mictc_buffer_offset_xml + TRACE_DATA_OFFSET);
		g_traceBufferTriggerOffset = (uint32_t *) (g_mictc_buffer_offset_xml + TRACE_TRIGGER_OFFSET);

		for (i = 0; i < TRACE_TRIGGER_MAX; i++) {
		  *g_traceBufferTriggerOffset = g_traceTriggers[i];
		  g_traceBufferTriggerOffset++;
		}

		*g_traceBufferStatusOffset = TRACE_HOST_READY;

		printf("Before fopen\n");

		if ((fp = fopen("cpu.xml", "w")) == NULL) {
			fprintf(stderr, "Cannot open file cpu.xml.\n");
		}

		printf("Waiting for TRACE_REG_COMPLETE or TRACE_ABORTED");
		fflush(stdout);

		while (*g_traceBufferStatusOffset != TRACE_REG_COMPLETE) {
			printf(".");
			fflush(stdout);
			sleep(1);
			if (timeout++ >= 200) {
				// Hmmm, something is hung up.  Save everything in the buffer ignoring length.
				printf("Punt!\n");
				fprintf(fp, "%s\n", (char *)g_traceBufferDataOffset);
				*g_traceBufferStatusOffset = TRACE_GET_FILE;
				fclose(fp);
				sleep(5);
				goto close;		// and quit
			}
			// If this happens the current trigger was not one we want -- reset and wait.
			if (*g_traceBufferStatusOffset == TRACE_ABORTED) {
				printf("\nAborted trace\n");
				fflush(stdout);
				goto close2;
			}
		}
		printf("\n");

		{
			int j;

			asm volatile ("lfence" ::: "memory");
			j = *g_traceBufferSizeOffset;
			fprintf(fp, "%*s\n", j, (char *)g_traceBufferDataOffset);
		}
		*g_traceBufferStatusOffset = TRACE_GET_FILE;
		fclose(fp);
		sleep(5);

		// Memory dump

		if ((fp = fopen("mem.dat", "w")) == NULL) {
			fprintf(stderr, "Cannot open file mem.dat.\n");
		}

		printf("Waiting for memory pages\n");
		fflush(stdout);

		timeout = 0;

		{
			long i = 0;

			while (*g_traceBufferStatusOffset != TRACE_MEM_COMPLETE) {
			  	//printf("status %d\n", *g_traceBufferStatusOffset); 

				if (*g_traceBufferStatusOffset == TRACE_PAGE_READY) {
					printf(" %ld", i++);
					fflush(stdout);
					asm volatile ("lfence" ::: "memory");

					if (fwrite(g_mictc_buffer_base, *g_traceBufferSizeOffset, 1, fp) != 1) {
						fprintf(stderr, "\nCannot write file mem.dat.  error = %d\n", ferror(fp));
						return 0;
					}
					*g_traceBufferStatusOffset = TRACE_HOST_READY;	// Get next page
					timeout = 0;
				} else {
					//  printf(".");
					//  fflush(stdout);
					usleep(10000);

					if (timeout++ >= 2000) {
						// Hmmm, something is hung up.  Just close and quit.
						printf("Punt!\n");
						fclose(fp);
						sleep(5);
						goto close;	// and quit
					}
				}
			}
		}
 close1:
		printf("\nClosing memory dump file.\n");
		fflush(stdout);
		fclose(fp);
		*g_traceBufferStatusOffset = TRACE_COMPLETE;	// File is closed; tell driver we are done.
		printf("Done.\n");
		fflush(stdout);
 close2:
		sleep(2);
		scif_munmap(g_mictc_buffer_offset_xml, MICTC_XML_BUFFER_SIZE);
		scif_unregister(mictc_newepd, (off_t) g_mictc_buffer_base, MICTC_MEM_BUFFER_SIZE);
		scif_close(mictc_newepd);
	} // while (1)
 close:
	scif_munmap(g_mictc_buffer_offset_xml, MICTC_XML_BUFFER_SIZE);
	scif_close(mictc_newepd);
	scif_close(mictc_epd_data);
	free(g_mictc_buffer_base);
	return 1;
}

int main(int argc, char *argv[])
{
	int i;

	for (i = 0; i < TRACE_TRIGGER_MAX; i++) {
		g_traceTriggers[i] = TRACE_EOL;
	}

	if (argc >= 2) {
		for (i = 1; i < argc; i++) {
			if (i > TRACE_TRIGGER_MAX) break;

			g_traceTriggers[i - 1] = atoi(argv[i]);
			printf("Trigger %d\n", g_traceTriggers[i - 1]);
		}
	} else {
		printf("No triggers -- accept everything\n");
	}

	if (!open_scif_channels())
		exit(1);

	exit(0);
}
