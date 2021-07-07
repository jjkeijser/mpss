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

// Use 2MB for KNF and 4MB for KNC.
#define MICTC_XML_BUFFER_SIZE (2 * 1024 * 1024)

// Memory transfer window.  1GB
#define MICTC_MEM_BUFFER_SIZE (1 * 1024UL * 1024UL * 1024UL)

FILE *ip;
FILE *op;


int main(void)
{
  long srcPhysAddr = 0;
  uint32_t page_buf[4096/4];
  long i = 0;
  int size;
  char dest[64];

  if ((ip = fopen("mem.dat", "r")) == NULL) {
    fprintf(stderr, "Cannot open file mem.dat.\n");
  }

  if ((op = fopen("memfmt.txt", "w")) == NULL) {
    fprintf(stderr, "Cannot open file memfmt.txt.\n");
  }

  while (! feof(ip)) {
    fread(page_buf, sizeof(page_buf), 1, ip); // check for error
      
    size = sprintf(dest, "origin %lx\n", srcPhysAddr);
    fwrite(dest, size, 1, op);

    for (i = 0; i < 4096/4; i++) {
      size = sprintf(dest, "%x\n", page_buf[i]);
      fwrite(dest, size, 1, op);
    }

    srcPhysAddr += 4096;
  }
  fclose(ip);
  fclose(op);
}
