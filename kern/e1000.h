#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#define E1000_ICS      0x000C8
#define E1000_IMS      0x000D0
#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100e
#define E1000_STATUS   0x00008
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_TCTL     0x00400
#define E1000_TIPG     0x00410

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
#define E1000_TXD_CMD_EOP    0x01

#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RA       0x05400
#define E1000_RAH_AV  0x80000000
#define E1000_MTA      0x05200
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */


#define TDA_COUNT 32
#define RDA_COUNT 256
#define PACKET_MAX_SIZE 2048
#define E1000_LOCATE(e1000, offset) (e1000[(offset) >> 2])

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
}__attribute__((packed));

/* Receive Descriptor */
struct rx_desc {
	uint64_t buffer_addr;
	uint16_t length;    /* Data buffer length */
    uint16_t cso;        /* Checksum offset */

	uint8_t status;
	uint8_t err;
	uint16_t special; 
	
}__attribute__((packed));

struct packet
{
    char body[PACKET_MAX_SIZE];
};
int pci_e1000_attach(struct pci_func * f);
int e1000_transmit(void *addr, size_t len);
int e1000_receive(void *addr, size_t len);
#endif  // SOL >= 6
