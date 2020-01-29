#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
// LAB 6: Your driver code here

volatile uint32_t *e1000;
struct tx_desc tda[TDA_COUNT] __attribute__((aligned (PGSIZE))) = {{0, 0, 0, 0, 0, 0, 0}};
struct packet buffer[TDA_COUNT] __attribute__((aligned (PGSIZE))) = {{{0}}};

int pci_e1000_attach(struct pci_func *f) {
    pci_func_enable(f);
    e1000 = mmio_map_region(f->reg_base[0], f->reg_size[0]);

    for (int i = 0; i < TDA_COUNT; ++i) {
        tda[i].addr = PADDR(&buffer[i]);
        tda[i].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
        tda[i].status = E1000_TXD_STAT_DD;
    }
    E1000_LOCATE(e1000, E1000_TDBAL) = PADDR(tda);
    E1000_LOCATE(e1000, E1000_TDLEN) = sizeof(struct tx_desc) * TDA_COUNT;
    E1000_LOCATE(e1000, E1000_TDH) = 0;
    E1000_LOCATE(e1000, E1000_TDT) = 0;
    E1000_LOCATE(e1000, E1000_TCTL) = E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << 4) | (0x40 << 12);
    E1000_LOCATE(e1000, E1000_TIPG) = 10 | (8 << 10) | (12 << 20);
    return 0;
}


int e1000_transmit(void *addr, size_t len) {
    uint32_t tail = E1000_LOCATE(e1000, E1000_TDT);
    if (len > PACKET_MAX_SIZE) {
        len = PACKET_MAX_SIZE;
    }
    if (!tda[tail].status & E1000_TXD_STAT_DD) {
        return -1;
    } else {
        memmove(&buffer[tail], addr, len);
        // Copy data into packet
        tda[tail].status &= ~E1000_TXD_STAT_DD;
        tda[tail].length = len;
        // Move the tail
        E1000_LOCATE(e1000, E1000_TDT) = (tail + 1) % TDA_COUNT;
    }
    return 0;
}