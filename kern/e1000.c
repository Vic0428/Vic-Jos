#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
// LAB 6: Your driver code here

volatile uint32_t *e1000;

uint32_t MAC[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

struct tx_desc tda[TDA_COUNT] __attribute__((aligned (PGSIZE))) = {{0, 0, 0, 0, 0, 0, 0}};
struct packet tx_buffer[TDA_COUNT] __attribute__((aligned (PGSIZE))) = {{{0}}};

struct packet rx_buffer[RDA_COUNT] __attribute__((aligned (PGSIZE))) = {{{0}}};
struct rx_desc rda[RDA_COUNT] __attribute__((aligned (PGSIZE))) = {{0, 0, 0, 0, 0, 0}};
int pci_e1000_attach(struct pci_func *f) {
    pci_func_enable(f);
    e1000 = mmio_map_region(f->reg_base[0], f->reg_size[0]);

    // Transmit initialization
    for (int i = 0; i < TDA_COUNT; ++i) {
        tda[i].addr = PADDR(&tx_buffer[i]);
        tda[i].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
        tda[i].status = E1000_TXD_STAT_DD;
    }
    E1000_LOCATE(e1000, E1000_TDBAL) = PADDR(tda);
    E1000_LOCATE(e1000, E1000_TDLEN) = sizeof(struct tx_desc) * TDA_COUNT;
    E1000_LOCATE(e1000, E1000_TDH) = 0;
    E1000_LOCATE(e1000, E1000_TDT) = 0;
    E1000_LOCATE(e1000, E1000_TCTL) = E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << 4) | (0x40 << 12);
    E1000_LOCATE(e1000, E1000_TIPG) = 10 | (8 << 10) | (12 << 20);
    

    // Receive initialization
    // Configure mac address
    for (int i = 0; i < RDA_COUNT; ++i) {
        rda[i].buffer_addr = PADDR(&rx_buffer[i]);
    }

    uint32_t low = 0;
    for (int i = 0; i < 4; i++) {
        low |= (MAC[i] << (8 * i));
    }
    uint32_t high = 0;
    for (int i = 0; i < 2; i++) {
        high |= (MAC[i + 4] << (8 * i));
    }
    E1000_LOCATE(e1000, E1000_RA) = low;
    E1000_LOCATE(e1000, E1000_RA + 4) = high | E1000_RAH_AV;

    E1000_LOCATE(e1000, E1000_ICS) = 0;
    E1000_LOCATE(e1000, E1000_IMS) = 0;

    E1000_LOCATE(e1000, E1000_RDBAL) = PADDR(rda);
    E1000_LOCATE(e1000, E1000_RDBAH) = 0;

    E1000_LOCATE(e1000, E1000_RDLEN) = RDA_COUNT * sizeof(struct rx_desc);
    E1000_LOCATE(e1000, E1000_RDH) = 0;
    E1000_LOCATE(e1000, E1000_RDT) = RDA_COUNT - 1;

    E1000_LOCATE(e1000, E1000_RCTL) = E1000_RCTL_EN  | E1000_RCTL_SECRC | E1000_RCTL_SZ_2048 | E1000_RCTL_BAM;

    return 0;
}


int e1000_transmit(void *addr, size_t len) {
    uint32_t tail = E1000_LOCATE(e1000, E1000_TDT);
    if (len > PACKET_MAX_SIZE) {
        len = PACKET_MAX_SIZE;
    }
    if (!(tda[tail].status & E1000_TXD_STAT_DD)) {
        return -1;
    } else {
        memmove(&tx_buffer[tail], addr, len);
        // Copy data into packet
        tda[tail].status &= ~E1000_TXD_STAT_DD;
        tda[tail].length = len;
        // Move the tail
        E1000_LOCATE(e1000, E1000_TDT) = (tail + 1) % TDA_COUNT;
    }
    return 0;
}


int e1000_receive(void *addr, size_t len) {
    uint32_t tail = (E1000_LOCATE(e1000, E1000_RDT) + 1) % RDA_COUNT;
    if (!(rda[tail].status & E1000_RXD_STAT_DD)) {
        return -1;
    } else {
        if (len > rda[tail].length) {
            len = rda[tail].length;
        }
        memmove(addr, &rx_buffer[tail], len);
        rda[tail].status &= ~E1000_RXD_STAT_DD;
        // Move the head
        E1000_LOCATE(e1000, E1000_RDT) = tail;
    }
    return len;
}