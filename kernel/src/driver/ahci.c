#include <stdint.h>
#include "memory.h"
#include "driver/ahci.h"
#include "driver/pci.h"
#include "driver/pit_timer.h"
#include "driver/serial.h"
#include "driver/vga.h"

static uint8_t ahci_port_memory[32 * 0x2000] __attribute__((aligned(1024)));
static uint16_t identify_buf[256] __attribute__((aligned(512)));

HBA_MEM *abar;

static void ahci_start_port(HBA_PORT *port)
{
    int timeout = 1000;
    while ((port->cmd & (PxCMD_CR | PxCMD_FR)) && timeout--)
    {
        pit_wait(1);
    }

    port->cmd |= PxCMD_FRE;
    port->cmd |= PxCMD_ST;
}

static void ahci_stop_port(HBA_PORT *port)
{
    port->cmd &= ~PxCMD_ST;

    int timeout = 1000;
    while ((port->cmd & PxCMD_CR) && timeout--)
    {
        pit_wait(1);
    }

    port->cmd &= ~PxCMD_FRE;

    timeout = 1000;
    while ((port->cmd & PxCMD_FR) && timeout--)
    {
        pit_wait(1);
    }
}

void ahci_probe_port(HBA_MEM *hba_mem, int port_no)
{
    HBA_PORT *port = &hba_mem->ports[port_no];
    
    uint32_t ssts = port->ssts;
    uint8_t det = ssts & 0x0F;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    
    serial_print("Port ");
    serial_print_hex8(port_no);
    serial_print(" SSTS: 0x");
    serial_print_hex(ssts);
    serial_print(" DET=");
    serial_print_hex8(det);
    serial_print(" IPM=");
    serial_print_hex8(ipm);
    serial_print("\n");
    
    if (det == 0) 
    {
        serial_print("No device detected\n");
        return;
    }
    
    if (det != 0x3) 
    {
        serial_print("Device present but PHY communication not established (DET=");
        serial_print_hex8(det);
        serial_print(")\n");
        return;
    }
    
    if (ipm != 0x1) 
    {
        serial_print("Device not in active power state (IPM=");
        serial_print_hex8(ipm);
        serial_print(")\n");
        return;
    }

    port->serr = 0xFFFFFFFF;

    serial_print("Starting port...\n");
    ahci_start_port(port);

    serial_print("Waiting for device signature...\n");
    int timeout = 100;
    uint32_t sig;
    
    while (timeout > 0) 
    {
        sig = port->sig;

        if (sig != 0xFFFFFFFF && sig != 0x00000000) 
        {
            break;
        }
        
        pit_wait(10);  
        timeout--;
    }
    
    if (timeout == 0) 
    {
        serial_print("Timeout waiting for device signature\n");
        serial_print("Final signature: 0x");
        serial_print_hex(port->sig);
        serial_print("\n");
        return;
    }
    
    serial_print("Device signature: 0x");
    serial_print_hex(sig);
    serial_print("\n");

    switch (sig)
    {
    case SATA_SIG_ATA:
        serial_print("SATA drive found on port ");
        serial_print_hex8(port_no);
        serial_print("\n");
        ahci_rebase_port(port, port_no);
        if (ahci_identify(port, identify_buf))
        {
            ata_print_identify(identify_buf);
        }
        else
        {
            serial_print("Failed to identify device\n");
        }
        break;

    case SATA_SIG_ATAPI:
        serial_print("SATAPI device found on port ");
        serial_print_hex8(port_no);
        serial_print("\n");
        break;

    case SATA_SIG_SEMB:
        serial_print("SEMB device found on port ");
        serial_print_hex8(port_no);
        serial_print("\n");
        break;

    case SATA_SIG_PM:
        serial_print("Port multiplier found on port ");
        serial_print_hex8(port_no);
        serial_print("\n");
        break;

    default:
        serial_print("Unknown device signature on port ");
        serial_print_hex8(port_no);
        serial_print("\n");
        break;
    }
}

void ahci_init(pci_device *ahci_dev)
{
    uint64_t abar_phys = ahci_dev->bar[5] & 0xFFFFFFF0;
    map_mmio_region(abar_phys, 0x2000);
    abar = (HBA_MEM *)abar_phys;
    
    serial_print("AHCI BAR5 address: 0x");
    serial_print_hex((uint32_t)(uintptr_t)abar);
    serial_print("\n");
    
    HBA_MEM *hba_mem = (HBA_MEM *)abar;

    serial_print("Setting AHCI enable bit...\n");
    hba_mem->ghc |= GHC_AE;

    serial_print("Initiating HBA reset...\n");
    hba_mem->ghc |= 1;

    int timeout = 100;

    while (hba_mem->ghc & 0x01)
    {
        pit_wait(1);
        if (--timeout == 0) 
        {
            serial_print("Error: Failed to reset controller\n");
            return;
        }
    }

    hba_mem->ghc |= GHC_AE;
    serial_print("Controller reset complete and AHCI re-enabled\n");

    timeout = 100;

    if (hba_mem->cap2 & 0x01) 
    { 
        serial_print("BIOS/OS Handoff supported, requesting ownership...");
        hba_mem->bohc |= 0x02; 
   
        while (hba_mem->bohc & 0x01) 
        {
            pit_wait(1);
            if (--timeout == 0) 
            {
                serial_print("Error: BIOS Busy bit could not be cleared\n");
                return;
            } 
        }

        serial_print("BIOS Busy bit successfully cleared\n");
    }
    else
    {
        serial_print("BIOS/OS Handoff not supported\n");
    }

    int num_ports = (hba_mem->cap & 0x1F) + 1; 
    serial_print("AHCI ports supported: ");
    serial_print_hex8(num_ports);
    serial_print("\n");

    serial_print("Ports implemented bitmask: 0x");
    serial_print_hex(hba_mem->pi);

    serial_print("\nProbing ports...\n");
    for (int i = 0; i < num_ports; i++) 
    {
        if (hba_mem->pi & (1 << i)) 
        {
            ahci_probe_port(hba_mem, i); 
        }
    }
    
    serial_print("\nAHCI initialization complete");
}

void ahci_rebase_port(HBA_PORT *port, int port_no)
{
    serial_print("Rebasing port ");
    serial_print_hex8(port_no);
    serial_print("...\n");

    ahci_stop_port(port);
    
    serial_print("Port stopped successfully\n");

    uint64_t port_base = (uint64_t)&ahci_port_memory[port_no * 0x2000];
    
    serial_print("Port base address: 0x");
    serial_print_hex(port_base);
    serial_print("\n");

    port->clb = (uint32_t)(port_base & 0xFFFFFFFF);
    port->clbu = (uint32_t)(port_base >> 32);

    uint64_t fis_base = port_base + 0x400;
    port->fb = (uint32_t)(fis_base & 0xFFFFFFFF);
    port->fbu = (uint32_t)(fis_base >> 32);

    memset((void*)port_base, 0, 1024);
    memset((void*)fis_base, 0, 256);

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER*)port_base;

    for (int i = 0; i < 32; i++)
    {
        cmd_header[i].prdtl = 8;  

        uint64_t ctba = port_base + 0x800 + (i * 256);

        cmd_header[i].ctba  = (uint32_t)(ctba & 0xFFFFFFFF);
        cmd_header[i].ctbau = (uint32_t)(ctba >> 32);

        memset((void*)ctba, 0, sizeof(HBA_CMD_TBL));
    }

    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    ahci_start_port(port);

    serial_print("Port rebased successfully\n\n");
}

int find_cmdslot(HBA_PORT *port)
{
    uint32_t slots = (port->sact | port->ci);
    int num_of_slots = (abar->cap & 0x0f00) >> 8;
    int i;

    for (i = 0; i < num_of_slots; i++)
    {
        if ((slots & 1) == 0)
        {
            return i;
        }
		
        slots >>= 1;
    }

    serial_print("Cannot find free command list entry\n");
	return -1;
}

int ahci_read(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint64_t buf)
{
    port->is = 0xFFFFFFFF;
    
    int spin = 0; 

    int slot = find_cmdslot(port);
    if (slot == -1)
    {
        return 0;
    }

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER*)((uint64_t)port->clb | ((uint64_t)port->clbu << 32));
    cmd_header += slot;
	cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);	
	cmd_header->w = 0;		
	cmd_header->prdtl = (uint16_t)((count - 1) >> 4) + 1;	

    uint64_t ctba = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
    HBA_CMD_TBL *cmd_tbl = (HBA_CMD_TBL*)ctba;
    memset((void*)cmd_tbl, 0, sizeof(HBA_CMD_TBL) + (cmd_header->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    int i;
    uint32_t sector_count = count;
    for (i = 0; i < cmd_header->prdtl - 1; i++)
    {
        cmd_tbl->prdt_entry[i].dba  = (uint32_t)(buf & 0xFFFFFFFF);
        cmd_tbl->prdt_entry[i].dbau = (uint32_t)(buf >> 32);
		cmd_tbl->prdt_entry[i].dbc = 8 * 1024 - 1;	
		cmd_tbl->prdt_entry[i].i = 1;
		buf += 8 * 1024;
		count -= 16;	
    }

    cmd_tbl->prdt_entry[i].dba = (uint32_t)buf;
    cmd_tbl->prdt_entry[i].dbau = (uint32_t)(buf >> 32);
	cmd_tbl->prdt_entry[i].dbc = (count << 9) - 1;	
	cmd_tbl->prdt_entry[i].i = 1;

    FIS_REG_H2D *cmd_fis = (FIS_REG_H2D*)(&cmd_tbl->cfis);

    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
	cmd_fis->c = 1;	
	cmd_fis->command = ATA_CMD_READ_DMA_EX;

    cmd_fis->lba0 = (uint8_t)startl;
	cmd_fis->lba1 = (uint8_t)(startl >> 8);
	cmd_fis->lba2 = (uint8_t)(startl >> 16);
	cmd_fis->device = 1 << 6;

    cmd_fis->lba3 = (uint8_t)(startl >> 24);
	cmd_fis->lba4 = (uint8_t)starth;
	cmd_fis->lba5 = (uint8_t)(starth >> 8);

	cmd_fis->countl = sector_count & 0xFF;
    cmd_fis->counth = (sector_count >> 8) & 0xFF;

    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
	{
		spin++;
	}
	
    if (spin == 1000000)
	{
		serial_print("Port is hung\n");
		return 0;
	}

    port->ci = 1 << slot;	

	while (1)
	{
		if ((port->ci & (1 << slot)) == 0) 
        {
            break;
        }

		if (port->is & PxIS_TFES)	
		{
			serial_print("Read disk error\n");
			return 0;
		}
	}

    if (port->is & PxIS_TFES)
	{
		serial_print("Read disk error\n");
		return 0;
	}

    return 1;
}

int ahci_write(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint64_t buf)
{
    port->is = 0xFFFFFFFF;

    int spin = 0; 

    int slot = find_cmdslot(port);
    if (slot == -1)
    {
        return 0;
    }

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER*)((uint64_t)port->clb | ((uint64_t)port->clbu << 32));
    cmd_header += slot;
    cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);     
    cmd_header->w = 1;                          
    cmd_header->prdtl = (uint16_t)((count - 1) >> 4) + 1;

    uint64_t ctba = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
    HBA_CMD_TBL *cmd_tbl = (HBA_CMD_TBL*)ctba;
    memset((void*)cmd_tbl, 0, sizeof(HBA_CMD_TBL) + (cmd_header->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    int i;
    uint32_t sector_count = count;
    for (i = 0; i < cmd_header->prdtl - 1; i++)
    {
        cmd_tbl->prdt_entry[i].dba  = (uint32_t)(buf & 0xFFFFFFFF);
        cmd_tbl->prdt_entry[i].dbau = (uint32_t)(buf >> 32);
		cmd_tbl->prdt_entry[i].dbc = 8 * 1024 - 1;	
		cmd_tbl->prdt_entry[i].i = 0;
		buf += 8 * 1024;
		count -= 16;
    }

    cmd_tbl->prdt_entry[i].dba = (uint32_t)buf;
    cmd_tbl->prdt_entry[i].dbau = (uint32_t)(buf >> 32);
	cmd_tbl->prdt_entry[i].dbc = (count << 9) - 1;	
	cmd_tbl->prdt_entry[i].i = 0;

    FIS_REG_H2D *cmd_fis = (FIS_REG_H2D*)(&cmd_tbl->cfis);

    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
	cmd_fis->c = 1;	
	cmd_fis->command = ATA_CMD_WRITE_DMA_EX;

    cmd_fis->lba0 = (uint8_t)startl;
	cmd_fis->lba1 = (uint8_t)(startl >> 8);
	cmd_fis->lba2 = (uint8_t)(startl >> 16);
	cmd_fis->device = 1 << 6;

    cmd_fis->lba3 = (uint8_t)(startl >> 24);
	cmd_fis->lba4 = (uint8_t)starth;
	cmd_fis->lba5 = (uint8_t)(starth >> 8);

	cmd_fis->countl = sector_count & 0xFF;
    cmd_fis->counth = (sector_count >> 8) & 0xFF;

    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
	{
		spin++;
	}
	
    if (spin == 1000000)
	{
		serial_print("Port is hung\n");
		return 0;
	}

    port->ci = 1 << slot;	

	while (1)
	{
		if ((port->ci & (1 << slot)) == 0) 
        {
            break;
        }

		if (port->is & PxIS_TFES)	
		{
			serial_print("Write disk error\n");
			return 0;
		}
	}

    if (port->is & PxIS_TFES)
	{
		serial_print("Write disk error\n");
		return 0;
	}

    return 1;
}

int ahci_identify(HBA_PORT *port, uint16_t *buf)
{
    port->is = 0xFFFFFFFF;

    int slot = find_cmdslot(port);
    if (slot == -1)
    {
        return 0;
    }

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER*)((uint64_t)port->clb | ((uint64_t)port->clbu << 32));
    cmd_header += slot;
    cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd_header->w = 0;
    cmd_header->prdtl = 1;

    uint64_t ctba = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
    HBA_CMD_TBL *cmd_tbl = (HBA_CMD_TBL*)ctba;
    memset((void*)cmd_tbl, 0, sizeof(HBA_CMD_TBL));

    cmd_tbl->prdt_entry[0].dba = (uint32_t)((uint64_t)buf & 0xFFFFFFFF);
    cmd_tbl->prdt_entry[0].dbau = (uint32_t)(((uint64_t)buf) >> 32);
    cmd_tbl->prdt_entry[0].dbc = 512 - 1;
    cmd_tbl->prdt_entry[0].i = 1;

    FIS_REG_H2D *fis = (FIS_REG_H2D*)cmd_tbl->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = ATA_CMD_IDENTIFY;
    fis->device = 0;

    int spin = 100000;
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin--);

    if (!spin) 
    {
        serial_print("Device busy, cannot send IDENTIFY command\n");
        return 0;
    }

    port->ci = 1 << slot;

    while (port->ci & (1 << slot))
    {
        if (port->is & PxIS_TFES)
        {
            serial_print("IDENTIFY command failed\n");
            return 0;
        }
    }

    return 1;
}

void ata_extract_string(char *dst, uint16_t *src, int start, int length)
{
    for (int i = 0; i < length / 2; i++)
    {
        dst[i * 2] = src[start + i] >> 8;
        dst[i * 2 + 1] = src[start + i] & 0xFF;
    }
    dst[length] = 0;

    for (int i = length - 1; i >= 0 && dst[i] == ' '; i--)
    {
        dst[i] = '\0';
    }
}

void ata_print_identify(uint16_t *identify_buf)
{
    char model[41];
    char serial[21];

    ata_extract_string(model, identify_buf, 27, 40);
    ata_extract_string(serial, identify_buf, 10, 20);

    int lba48 = (identify_buf[83] & (1 << 10)) != 0;

    uint64_t sectors = 0;
    if (lba48)
    {
        sectors =
            ((uint64_t)identify_buf[103] << 48) |
            ((uint64_t)identify_buf[102] << 32) |
            ((uint64_t)identify_buf[101] << 16) |
             (uint64_t)identify_buf[100];
    }

    vga_print_color("MODEL: ", YELLOW, BLACK);
    vga_print(model);
    vga_print("\n");

    vga_print_color("SERIAL NUMBER: ", YELLOW, BLACK);
    vga_print(serial);
    vga_print("\n");

    vga_print_color("LBA48: ", YELLOW, BLACK);
    vga_print(lba48 ? "SUPPORTED" : "NOT SUPPORTED");
    vga_print("\n");

    if (lba48)
    {
        vga_print_color("TOTAL SECTORS: ", YELLOW, BLACK);
        vga_print_hex(sectors);
        vga_print("\n\n");
    }
}