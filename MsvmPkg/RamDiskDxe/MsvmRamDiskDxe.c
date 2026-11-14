/** @file
  Registers a platform-provided RAM disk without publishing ACPI NFIT.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/BootRamDisk.h>
#include <Protocol/RamDisk.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC
EFI_STATUS
CheckReservedBacking (
  IN UINT64  Base,
  IN UINT64  Size
  )
{
  EFI_STATUS             status;
  EFI_MEMORY_DESCRIPTOR  *map = NULL;
  EFI_MEMORY_DESCRIPTOR  *entry;
  UINTN                  mapSize = 0;
  UINTN                  mapKey;
  UINTN                  descriptorSize;
  UINT32                 descriptorVersion;
  UINTN                  offset;

  status = gBS->GetMemoryMap (&mapSize, map, &mapKey, &descriptorSize, &descriptorVersion);
  while (status == EFI_BUFFER_TOO_SMALL) {
    mapSize += 2 * descriptorSize;
    map      = AllocatePool (mapSize);
    if (map == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    status = gBS->GetMemoryMap (&mapSize, map, &mapKey, &descriptorSize, &descriptorVersion);
    if (EFI_ERROR (status)) {
      FreePool (map);
      map = NULL;
    }
  }

  if (EFI_ERROR (status)) {
    return status;
  }

  status = EFI_NOT_FOUND;
  for (offset = 0; offset < mapSize; offset += descriptorSize) {
    entry = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)map + offset);
    if ((entry->Type == EfiReservedMemoryType) &&
        (entry->PhysicalStart <= Base) &&
        (entry->NumberOfPages <= MAX_UINT64 / EFI_PAGE_SIZE) &&
        (Base - entry->PhysicalStart <= EFI_PAGES_TO_SIZE (entry->NumberOfPages)) &&
        (Size <= EFI_PAGES_TO_SIZE (entry->NumberOfPages) - (Base - entry->PhysicalStart)))
    {
      status = EFI_SUCCESS;
      break;
    }
  }

  FreePool (map);
  return status;
}

EFI_STATUS
EFIAPI
MsvmRamDiskDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT64                    base = PcdGet64 (PcdRamDiskBaseAddress);
  UINT64                    size = PcdGet64 (PcdRamDiskImageSize);
  EFI_STATUS                status;
  EFI_RAM_DISK_PROTOCOL     *ramDisk;
  EFI_DEVICE_PATH_PROTOCOL  *devicePath;

  if (size == 0) {
    return EFI_SUCCESS;
  }

  status = CheckReservedBacking (base, size);
  if (EFI_ERROR (status)) {
    DEBUG ((DEBUG_ERROR, "RAM-disk reserved backing unavailable: %r\n", status));
    return status;
  }

  status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID **)&ramDisk);
  if (!EFI_ERROR (status)) {
    status = ramDisk->Register (base, size, &gMuBootRamDiskGuid, NULL, &devicePath);
  }

  if (EFI_ERROR (status)) {
    DEBUG ((DEBUG_ERROR, "RAM-disk registration failed: %r\n", status));
    return status;
  }

  DEBUG ((DEBUG_INFO, "Registered reserved RAM disk at %lx, size %lx\n", base, size));
  return EFI_SUCCESS;
}
