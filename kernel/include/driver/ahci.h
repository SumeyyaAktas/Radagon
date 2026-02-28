#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include "pci.h"

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA 0x06
#define PCI_PROG_IF_AHCI 0x01

#define GHC_AE (1 << 31)
#define GHC_HR (1 << 0)
#define HOST_CAP_64 (1 << 31)

#define PxCMD_ST (1 << 0)
#define PxCMD_FR (1 << 14)
#define PxCMD_FRE (1 << 4)
#define PxCMD_CR (1 << 15)
#define PxSTSS (2 << 0)
#define PxIS_TFES (1 << 30) 

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SATAPI 4
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define HBA_PORT_DET_PRESENT 3
#define HBA_PORT_DET_IDLE 4
#define HBA_PORT_IPM_ACTIVE 1
#define AHCI_BASE 0x400000 

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08     
#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY 0xEC

#define SATA_SIG_ATA 0x00000101  
#define SATA_SIG_ATAPI 0xEB140101  
#define SATA_SIG_SEMB 0xC33C0101  
#define SATA_SIG_PM 0x96690101

typedef enum
{
	FIS_TYPE_REG_H2D = 0x27,	
	FIS_TYPE_REG_D2H = 0x34,	
	FIS_TYPE_DMA_ACT = 0x39,	
	FIS_TYPE_DMA_SETUP = 0x41,	
	FIS_TYPE_DATA = 0x46,	
	FIS_TYPE_BIST = 0x58,	
	FIS_TYPE_PIO_SETUP = 0x5F,	
	FIS_TYPE_DEV_BITS = 0xA1,	
} FIS_TYPE;

typedef struct tagFIS_REG_H2D
{
	uint8_t fis_type;	

	uint8_t pmport:4;	
	uint8_t rsv0:3;		
	uint8_t c:1;		

	uint8_t command;	
	uint8_t featurel;	

	uint8_t lba0;		
	uint8_t lba1;		
	uint8_t lba2;		
	uint8_t device;		

	uint8_t lba3;		
	uint8_t lba4;		
	uint8_t lba5;		
	uint8_t featureh;	

	uint8_t countl;		
	uint8_t counth;		
	uint8_t icc;		
	uint8_t control;	

	uint8_t  rsv1[4];	
} FIS_REG_H2D __attribute__(());

typedef volatile struct tagHBA_PORT 
{
	uint32_t clb;		
	uint32_t clbu;		
	uint32_t fb;		
	uint32_t fbu;		
	uint32_t is;		
	uint32_t ie;		
	uint32_t cmd;		
	uint32_t rsv0;		
	uint32_t tfd;		
	uint32_t sig;		
	uint32_t ssts;		
	uint32_t sctl;		
	uint32_t serr;		
	uint32_t sact;		
	uint32_t ci;		
	uint32_t sntf;		
	uint32_t fbs;		
	uint32_t rsv1[11];	
	uint32_t vendor[4];	
} HBA_PORT __attribute__(());

typedef volatile struct tagHBA_MEM
{
	uint32_t cap;		
	uint32_t ghc;		
	uint32_t is;		
	uint32_t pi;		
	uint32_t vs;		
	uint32_t ccc_ctl;	
	uint32_t ccc_pts;	
	uint32_t em_loc;		
	uint32_t em_ctl;		
	uint32_t cap2;		
	uint32_t bohc;		

	uint8_t rsv[0xA0-0x2C];

	uint8_t vendor[0x100-0xA0];

	HBA_PORT ports[32];	
} HBA_MEM __attribute__(());

typedef struct tagHBA_CMD_HEADER 
{
	uint8_t cfl:5;		
	uint8_t a:1;		
	uint8_t w:1;		
	uint8_t p:1;		

	uint8_t r:1;		
	uint8_t b:1;		
	uint8_t c:1;		
	uint8_t rsv0:1;		
	uint8_t pmp:4;		

	uint16_t prdtl;		

	volatile uint32_t prdbc;		

	uint32_t ctba;		
	uint32_t ctbau;		

	uint32_t rsv1[4];
} HBA_CMD_HEADER __attribute__(());

typedef struct tagHBA_PRDT_ENTRY
{
	uint32_t dba;		
	uint32_t dbau;		
	uint32_t rsv0;		

	uint32_t dbc:22;		
	uint32_t rsv1:9;		
	uint32_t i:1;		
} HBA_PRDT_ENTRY __attribute__(());

typedef struct tagHBA_CMD_TBL
{
	uint8_t cfis[64];	
	uint8_t acmd[16];	
	uint8_t rsv[48];	

	HBA_PRDT_ENTRY prdt_entry[1];	
} HBA_CMD_TBL __attribute__(());

void ahci_init(pci_device *ahci_dev);
void ahci_probe_port(HBA_MEM *hba_mem, int port_no);
void ahci_rebase_port(HBA_PORT *port, int port_no);
int find_cmdslot(HBA_PORT *port);
int ahci_read(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint64_t buf);
int ahci_write(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint64_t buf);
int ahci_identify(HBA_PORT *port, uint16_t *buf);
void ata_extract_string(char *dst, uint16_t *src, int start, int length);
void ata_print_identify(uint16_t *identify_buf);

#endif