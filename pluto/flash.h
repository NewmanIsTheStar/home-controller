/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef FLASH_H
#define FLASH_H

#include "config.h"

/*Explanation
  The configuration was originally stored in the last sector of flash.
  The Pi Pico2 had a hardware bug that needed a work around.
  That workaround overwrites the last sector of flash when dragging 
  and dropping the UF2 file onto the Pi Pico2 -- thus nuking the cfg->
  Therefore the config has been moved to the penultimate flash sector.
  See "RP2350-E10 errata for the Raspberry Pi Pico 2" for more info.
*/
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - (2*FLASH_SECTOR_SIZE))
#define FLASH_LEGACY_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

/* Configuration Search Sequence
   1. If a configuration file is present and valid then it will be used.
   2. If no file is found then the peunultimate sector of flash is checked for a valid configuration.
   3. Finally, the last sector of flash is checked for a valid configuration.
   4. If no configuration was found then a new one will be created.
*/


int flash_read_non_volatile_variables(CONFIG_TYPE_T config_type);
int flash_write_non_volatile_variables(CONFIG_TYPE_T config_type);
int flash_dump(void);
void flash_get_program_size(void);
void flash_get_config_size(void);
void *flash_get_config_location(CONFIG_TYPE_T config_type);
int flash_dump_config(CONFIG_TYPE_T config_type);

#endif