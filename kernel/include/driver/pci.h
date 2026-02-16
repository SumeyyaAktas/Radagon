#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stdbool.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02
#define PCI_COMMAND 0x04
#define PCI_REVISION_ID 0x08
#define PCI_PROG_IF 0x09
#define PCI_SUBCLASS 0x0A
#define PCI_CLASS_CODE 0x0B
#define PCI_HEADER_TYPE 0x0E
#define PCI_BAR0 0x10
#define PCI_INTERRUPT_LINE 0x3C
#define PCI_INTERRUPT_PIN 0x3D

#define PCI_COMMAND_IO (1 << 0)
#define PCI_COMMAND_MEMORY (1 << 1)
#define PCI_COMMAND_BUS_MASTER (1 << 2)

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA 0x06
#define PCI_PROG_IF_AHCI 0x01

#define PCI_BAR_TYPE_IO 0x01
#define PCI_BAR_MMIO_MASK 0xFFFFFFF0
#define PCI_BAR_IO_MASK 0xFFFFFFFC

typedef struct 
{
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
    uint8_t header_type;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint32_t bar[6];
} pci_device;

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function);

void pci_read_device_info(uint8_t bus, uint8_t device, uint8_t function, pci_device *dev);

uint32_t pci_get_bar_size(pci_device *dev, uint8_t bar_num);

void pci_enable_bus_mastering(pci_device *dev);
void pci_enable_memory_space(pci_device *dev);
void pci_enable_io_space(pci_device *dev);

pci_device* pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device* pci_find_device_by_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if);

void pci_enumerate(void);
void pci_init(void);

#endif 