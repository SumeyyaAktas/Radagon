<div align="center">

<img src="assets/Radagon-image.jpg" width="100" height="100"/>
  
# Radagon

<p>
  
  ### AHCI Storage Driver
[![AHCI](https://img.shields.io/badge/SATA-AHCI-ffd000?style=flat-square)]()
[![License](https://img.shields.io/badge/License-Apache_2.0-ffd000?style=flat-square)]()

</p>

</div>

Radagon is a Serial ATA Advanced Host Controller Interface (AHCI) driver written in C for x86-64 operating systems. This project communicates with modern SATA storage devices through Memory-Mapped I/O (MMIO) and Direct Memory Access (DMA).

## Requirements
- x86_64-elf Cross-Compiler
- GNU Make
- NASM 
- QEMU

## Building
```bash
make all    # Build boot sector and disk image
make run    # Run in QEMU
make clean  # Clean build artifacts
```
> The ```make run``` command attaches a ICH-9 AHCI controller to QEMU to test the driver logic.

## Resources
- [Intel Serial ATA AHCI 1.3.1 Specification](<https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf>)
- [AHCI - OSDev Wiki](<https://wiki.osdev.org/AHCI>)

## License
This project is licensed under the Apache License 2.0. See the [LICENSE](./LICENSE) file for details.
