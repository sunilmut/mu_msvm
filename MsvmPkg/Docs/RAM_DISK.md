# Platform-provided RAM disk

The typed configuration blob can optionally contain one `UefiConfigRamDisk`
entry (`0x29`), introduced in firmware interface version 2.1. This capability is
generic and independent of the producer or image-delivery mechanism. The direct
IGVM parameter parser is unchanged.

## Wire format

All integers use the existing configuration ABI's little-endian encoding.
The entry is 32 bytes, aligned to eight bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `Header.Type`, `0x29` |
| 4 | 4 | `Header.Length`, `32` (including the header) |
| 8 | 8 | `BaseAddress`, guest physical byte address |
| 16 | 8 | `ImageSize`, raw disk image size in bytes |
| 24 | 8 | `Flags`, must be zero |

The producer must supply a nonzero, page-aligned address and size. The image is
used in place; firmware does not copy, initialize, accept, or hash its pages.
The image must be accessible in guest-private memory before firmware starts.
The producer is responsible for integrity: preserve measured IGVM image pages,
or verify the final private copy of a separately delivered image against a
trusted expected digest before handoff.

## Memory ownership

The producer must describe the entire image inside one page-aligned
`VM_MEMORY_RANGE_V5` entry with only
`VM_MEMORY_RANGE_FLAG_PLATFORM_RESERVED` set and `Reserved` zero. No other map
entry may overlap the image. Legacy memory maps are not supported for this
capability. The image must lie above the firmware/configuration reservation and
within the physical address space, without overlapping either configured MMIO
gap. Older firmware may ignore the unknown entry; producers must use a firmware
image supporting this capability rather than interpreting that as successful
RAM-disk registration.

PEI validates the description before memory initialization. The existing
platform-reserved resource path excludes the backing from allocations and page
acceptance. DXE checks that the resulting EFI memory map still marks the entire
image `EfiReservedMemoryType` before registration. It remains reserved after
`ExitBootServices`; it is not reclaimed by this driver.

## Registration and dependency

`MsvmRamDiskDxe` depends on `EFI_RAM_DISK_PROTOCOL`. It registers the image with
`gMuBootRamDiskGuid`, a firmware-boot RAM-disk type that explicitly suppresses
ACPI NFIT publication. Existing partition and filesystem drivers discover the
raw disk through the RAM-disk driver's normal controller connection.

This requires a separate MU_BASECORE change in MdeModulePkg adding the GUID and
excluding that type in `RamDiskPublishNfit`. That exclusion covers both immediate
registration and delayed ACPI notification. Other RAM-disk types retain their
existing publication behavior. A private type GUID without the dependency
change does not suppress NFIT.

Submit the dependency change to its upstream repository first, then update the
platform's dependency revision to a published commit. A local dependency commit
must not be presented as an available upstream revision.

## Boot behavior and validation limits

An absent entry preserves normal behavior. Malformed, duplicate, non-reserved,
or conflicting entries fail configuration validation. Registration errors are
reported as DXE driver errors. This capability does not restrict other boot
devices, change BootOrder, or disable host SCSI enumeration. A separate policy
change is required for RAM-disk-only, fail-closed boot.

NFIT suppression does not guarantee that an OS cannot otherwise discover the
device. Windows Boot Manager compatibility with this device-path type and actual
boot from a GPT/FAT ESP require integration validation. Firmware builds and code
review are not substitutes for measured-launch, attestation, or guest boot tests.
