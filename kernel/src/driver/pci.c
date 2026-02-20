#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "ports.h"
#include "driver/pci.h"
#include "driver/serial.h"
#include "driver/vga.h"
#include "driver/ahci.h"

#define MAX_PCI_DEVICES 64

static pci_device pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

static uint32_t pci_config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    return (uint32_t)
    (
        ((uint32_t)1 << 31) |           
        ((uint32_t)bus << 16) |         
        ((uint32_t)device << 11) |      
        ((uint32_t)function << 8) |    
        ((uint32_t)offset & 0xFC)   
    );
}

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return (uint8_t)((inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF);
}

void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    uint32_t address = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value)
{
    uint32_t address = pci_config_address(bus, device, function, offset);
    uint32_t old_value = pci_config_read_dword(bus, device, function, offset & 0xFC);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFF << shift;
    uint32_t new_value = (old_value & ~mask) | ((uint32_t)value << shift);
    
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, new_value);
}

void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value)
{
    uint32_t address = pci_config_address(bus, device, function, offset);
    uint32_t old_value = pci_config_read_dword(bus, device, function, offset & 0xFC);
    uint32_t shift = (offset & 3) * 8;
    uint32_t mask = 0xFF << shift;
    uint32_t new_value = (old_value & ~mask) | ((uint32_t)value << shift);
    
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, new_value);
}

bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function)
{
    uint16_t vendor_id = pci_config_read_word(bus, device, function, PCI_VENDOR_ID);
    return (vendor_id != 0xFFFF && vendor_id != 0x0000);
}

void pci_read_device_info(uint8_t bus, uint8_t device, uint8_t function, pci_device *dev)
{
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    
    dev->vendor_id = pci_config_read_word(bus, device, function, PCI_VENDOR_ID);
    dev->device_id = pci_config_read_word(bus, device, function, PCI_DEVICE_ID);
    dev->class_code = pci_config_read_byte(bus, device, function, PCI_CLASS_CODE);
    dev->subclass = pci_config_read_byte(bus, device, function, PCI_SUBCLASS);
    dev->prog_if = pci_config_read_byte(bus, device, function, PCI_PROG_IF);
    dev->revision_id = pci_config_read_byte(bus, device, function, PCI_REVISION_ID);
    dev->header_type = pci_config_read_byte(bus, device, function, PCI_HEADER_TYPE);
    dev->interrupt_line = pci_config_read_byte(bus, device, function, PCI_INTERRUPT_LINE);
    dev->interrupt_pin = pci_config_read_byte(bus, device, function, PCI_INTERRUPT_PIN);

    for (uint8_t i = 0; i < 6; i++) 
    {
        dev->bar[i] = pci_config_read_dword(bus, device, function, PCI_BAR0 + (i * 4));
    }
}

uint32_t pci_get_bar_size(pci_device *dev, uint8_t bar_num)
{
    if (bar_num >= 6) 
    {
        return 0;
    }

    uint8_t bar_offset = PCI_BAR0 + (bar_num * 4);
    uint32_t original = pci_config_read_dword(dev->bus, dev->device, dev->function, bar_offset);

    pci_config_write_dword(dev->bus, dev->device, dev->function, bar_offset, 0xFFFFFFFF);

    uint32_t size_mask = pci_config_read_dword(dev->bus, dev->device, dev->function, bar_offset);

    pci_config_write_dword(dev->bus, dev->device, dev->function, bar_offset, original);

    if (original & PCI_BAR_TYPE_IO) 
    {
        size_mask &= PCI_BAR_IO_MASK;
    } 
    else 
    {
        size_mask &= PCI_BAR_MMIO_MASK;
    }
    
    if (size_mask == 0) 
    {
        return 0;
    }

    return ~size_mask + 1;
}

void pci_enable_bus_mastering(pci_device *dev)
{
    uint16_t command = pci_config_read_word(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_BUS_MASTER;
    pci_config_write_word(dev->bus, dev->device, dev->function, PCI_COMMAND, command);
}

void pci_enable_memory_space(pci_device *dev)
{
    uint16_t command = pci_config_read_word(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY;
    pci_config_write_word(dev->bus, dev->device, dev->function, PCI_COMMAND, command);
}

void pci_enable_io_space(pci_device *dev)
{
    uint16_t command = pci_config_read_word(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_IO;
    pci_config_write_word(dev->bus, dev->device, dev->function, PCI_COMMAND, command);
}

pci_device* pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    for (uint32_t i = 0; i < pci_device_count; i++) 
    {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) 
        {
            return &pci_devices[i];
        }
    }

    return NULL;
}

pci_device* pci_find_device_by_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if)
{
    for (uint32_t i = 0; i < pci_device_count; i++) 
    {
        if (pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass && pci_devices[i].prog_if == prog_if) 
        {
            return &pci_devices[i];
        }
    }

    return NULL;
}

static void pci_print_device(pci_device *dev)
{
    serial_print("PCI [");
    serial_print_hex8(dev->bus);
    serial_print(":");
    serial_print_hex8(dev->device);
    serial_print(":");
    serial_print_hex8(dev->function);
    serial_print("] Vendor: ");
    serial_print_hex16(dev->vendor_id);
    serial_print(" Device: ");
    serial_print_hex16(dev->device_id);
    serial_print(" Class: ");
    serial_print_hex8(dev->class_code);
    serial_print(" Subclass: ");
    serial_print_hex8(dev->subclass);
    serial_print(" ProgIF: ");
    serial_print_hex8(dev->prog_if);
    serial_print("\n");

    for (uint8_t i = 0; i < 6; i++) 
    {
        if (dev->bar[i] != 0 && dev->bar[i] != 0xFFFFFFFF) 
        {
            serial_print("BAR");
            serial_print_hex8(i);
            serial_print(": ");
            serial_print_hex(dev->bar[i]);
            
            uint32_t size = pci_get_bar_size(dev, i);
            if (size > 0) 
            {
                serial_print(" (Size: ");
                serial_print_hex(size);
                serial_print(")");
            }

            serial_print("\n");
        }
    }
}

void pci_enumerate(void)
{
    serial_print("Starting PCI enumeration...\n\n");
    
    pci_device_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) 
    {
        for (uint8_t device = 0; device < 32; device++) 
        {
            if (!pci_device_exists(bus, device, 0)) 
            {
                continue;
            }

            uint8_t header_type = pci_config_read_byte(bus, device, 0, PCI_HEADER_TYPE);
            uint8_t function_count = (header_type & 0x80) ? 8 : 1;

            for (uint8_t function = 0; function < function_count; function++) 
            {
                if (!pci_device_exists(bus, device, function)) 
                {
                    continue;
                }
                
                if (pci_device_count >= MAX_PCI_DEVICES) 
                {
                    serial_print("Warning: Maximum PCI device limit reached\n");
                    return;
                }
                
                pci_device *dev = &pci_devices[pci_device_count++];
                pci_read_device_info(bus, device, function, dev);
                pci_print_device(dev);
            }
        }
    }
    
    serial_print("\nTotal devices found: ");
    serial_print_hex8(pci_device_count);

    serial_print("\nSearching for AHCI controller...\n");
    pci_device *ahci = pci_find_device_by_class(PCI_CLASS_MASS_STORAGE, PCI_SUBCLASS_SATA, PCI_PROG_IF_AHCI);
    
    if (ahci != NULL) 
    {
        serial_print("AHCI controller found\n\n");
        serial_print("Vendor ID: ");
        serial_print_hex16(ahci->vendor_id);
        serial_print("\nDevice ID: ");
        serial_print_hex16(ahci->device_id);
        serial_print("\n\n");

        serial_print("Enabling bus mastering and memory space...\n");
        pci_enable_bus_mastering(ahci);
        pci_enable_memory_space(ahci);
        
        serial_print("AHCI controller initialized successfully");
        ahci_init(ahci);

    } 
    else 
    {
        serial_print("Error: No AHCI controller found!\n");
        vga_print("Error: No AHCI controller found!\n");
    }
}

void pci_init(void)
{
    serial_print("Initializing PCI subsystem...\n");

    outl(PCI_CONFIG_ADDRESS, 0x80000000);
    uint32_t test = inl(PCI_CONFIG_ADDRESS);
    
    if (test != 0x80000000) 
    {
        serial_print("ERROR: PCI not available!\n");
        return;
    }
    
    serial_print("PCI subsystem detected\n");
    pci_enumerate();
}