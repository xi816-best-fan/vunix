#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "init_kernel.h"
#include "vfs.h"
#include "rescue.h"

void init_kernel(int size, char** args) {
  printf("Vunix version 0.2(root@vunix)\n");
  printf("Command line: ");
  for(int i = 0; i < size; i++) {
    printf("%s ", args[i]);
  }
  printf("\n");
  printf("e820: BIOS-provided physical RAM map:\n");
  printf("e820: [mem 0x0000000000000000-0x0000000000FACE0]\n");
  printf("smpboot: Allowing 1 CPUs, 0 hotplug CPUs\n");
  printf("VunixPCI: MMCONFIG for domain 0000[bus 00-3f] at [mem 0xf8000000-0xfbffffff] (base 0xf8000000)\n");
  printf("VunixACPI: EC: EC Started");
  usleep(364039);
  printf("ahci 0000:00:1f.2: version 0.1\n");
  printf("ahci 0000:00:1f.2: AHCI 0001.0300 32 slots 6 ports 6 Gbps 0x5 impl SATA mode\n");
  printf("ata1: SATA max UDMA/133 abar m2048@0xf7d10000 port 0xf7d10100 irq 28\n");
  if(strcmp(args[1], "rescue")) {
    sys_mount(args[1]);
  } else {
    printf("Booting in rescue mode...");
    rescue();
  }
  printf("VUNIX-fs (sda1): mounted filesystem with ordered data mode. Opts: (null)\n");
  printf("e1000e: VunixCore Risc_gc16 PRO Driver - 0.1\n");
  printf("e1000e: 0000:00:19.0 eth: (VunixPCI Express:2.5GT/s:Width x1) 00:11:22:33:44:55\n");
  printf("Running /init as init process...\n");
  usleep(1000000);
}

