/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2022 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include <bdk.h>

#include "fe_info.h"
#include "../config.h"
#include "../hos/hos.h"
#include "../hos/pkg1.h"
#include <libs/fatfs/ff.h>

extern hekate_config h_cfg;
extern void emmcsn_path_impl(char *path, char *sub_dir, char *filename, sdmmc_storage_t *storage);

#pragma GCC push_options
#pragma GCC optimize ("Os")

void print_fuseinfo()
{
	u32 fuse_size = h_cfg.t210b01 ? 0x368 : 0x300;
	u32 fuse_address = h_cfg.t210b01 ? 0x7000F898 : 0x7000F900;

	gfx_clear_partial_grey(0x1B, 0, 1256);
	gfx_con_setpos(0, 0);

	gfx_printf("\nSKU:         %X - ", FUSE(FUSE_SKU_INFO));
	switch (fuse_read_hw_state())
	{
	case FUSE_NX_HW_STATE_PROD:
		gfx_printf("Retail\n");
		break;
	case FUSE_NX_HW_STATE_DEV:
		gfx_printf("Dev\n");
		break;
	}
	gfx_printf("Marca Sdram: %d\n", fuse_read_dramid(true));
	gfx_printf("F. quemados: %d / 64\n", bit_count(fuse_read_odm(7)));
	gfx_printf("Clave segura:%08X%08X%08X%08X\n\n\n",
		byte_swap_32(FUSE(FUSE_PRIVATE_KEY0)), byte_swap_32(FUSE(FUSE_PRIVATE_KEY1)),
		byte_swap_32(FUSE(FUSE_PRIVATE_KEY2)), byte_swap_32(FUSE(FUSE_PRIVATE_KEY3)));

	gfx_printf("%kCache Fusible:\n\n%k", TXT_CLR_CYAN_L, TXT_CLR_DEFAULT);
	gfx_hexdump(fuse_address, (u8 *)fuse_address, fuse_size);

	btn_wait();
}

void print_mmc_info()
{
	gfx_clear_partial_grey(0x1B, 0, 1256);
	gfx_con_setpos(0, 0);

	static const u32 SECTORS_TO_MIB_COEFF = 11;

	if (!emmc_initialize(false))
	{
		EPRINTF("Error al iniciar eMMC.");
		goto out;
	}
	else
	{
		u16 card_type;
		u32 speed = 0;

		gfx_printf("%kCID:%k\n", TXT_CLR_CYAN_L, TXT_CLR_DEFAULT);
		switch (emmc_storage.csd.mmca_vsn)
		{
		case 2: /* MMC v2.0 - v2.2 */
		case 3: /* MMC v3.1 - v3.3 */
		case 4: /* MMC v4 */
			gfx_printf(
				" Marca:      %X\n"
				" ID de OEM:  %02X\n"
				" Model:      %c%c%c%c%c%c\n"
				" Rev Prod:   %X\n"
				" S/N:        %04X\n"
				" Mes/Anio:   %02d/%04d\n\n",
				emmc_storage.cid.manfid, emmc_storage.cid.oemid,
				emmc_storage.cid.prod_name[0], emmc_storage.cid.prod_name[1], emmc_storage.cid.prod_name[2],
				emmc_storage.cid.prod_name[3], emmc_storage.cid.prod_name[4],	emmc_storage.cid.prod_name[5],
				emmc_storage.cid.prv, emmc_storage.cid.serial, emmc_storage.cid.month, emmc_storage.cid.year);
			break;
		default:
			break;
		}

		if (emmc_storage.csd.structure == 0)
			EPRINTF("Estructura CSD desconocida.");
		else
		{
			gfx_printf("%kCSD Extendida V1.%d:%k\n",
				TXT_CLR_CYAN_L, emmc_storage.ext_csd.ext_struct, TXT_CLR_DEFAULT);
			card_type = emmc_storage.ext_csd.card_type;
			char card_type_support[96];
			card_type_support[0] = 0;
			if (card_type & EXT_CSD_CARD_TYPE_HS_26)
			{
				strcat(card_type_support, "HS26");
				speed = (26 << 16) | 26;
			}
			if (card_type & EXT_CSD_CARD_TYPE_HS_52)
			{
				strcat(card_type_support, ", HS52");
				speed = (52 << 16) | 52;
			}
			if (card_type & EXT_CSD_CARD_TYPE_DDR_1_8V)
			{
				strcat(card_type_support, ", DDR52_1.8V");
				speed = (52 << 16) | 104;
			}
			if (card_type & EXT_CSD_CARD_TYPE_HS200_1_8V)
			{
				strcat(card_type_support, ", HS200_1.8V");
				speed = (200 << 16) | 200;
			}
			if (card_type & EXT_CSD_CARD_TYPE_HS400_1_8V)
			{
				strcat(card_type_support, ", HS400_1.8V");
				speed = (200 << 16) | 400;
			}

			gfx_printf(
				" Version Esp.:  %02X\n"
				" Rev Extendida: 1.%d\n"
				" Version Dev:   %d\n"
				" Clases de Cmd: %02X\n"
				" Capacidad:     %s\n"
				" Tasa Maxima:   %d MB/s (%d MHz)\n"
				" Tasa actual:   %d MB/s\n"
				" Tipo soporte:  ",
				emmc_storage.csd.mmca_vsn, emmc_storage.ext_csd.rev, emmc_storage.ext_csd.dev_version, emmc_storage.csd.cmdclass,
				emmc_storage.csd.capacity == (4096 * 512) ? "Alta" : "Baja", speed & 0xFFFF, (speed >> 16) & 0xFFFF,
				emmc_storage.csd.busspeed);
			gfx_con.fntsz = 8;
			gfx_printf("%s", card_type_support);
			gfx_con.fntsz = 16;
			gfx_printf("\n\n", card_type_support);

			u32 boot_size = emmc_storage.ext_csd.boot_mult << 17;
			u32 rpmb_size = emmc_storage.ext_csd.rpmb_mult << 17;
			gfx_printf("%kParticiones de eMMC:%k\n", TXT_CLR_CYAN_L, TXT_CLR_DEFAULT);															  
			gfx_printf(" 1: %kBOOT0      %k\n    Tam: %5d KiB (Sectores LBA: 0x%07X)\n", TXT_CLR_GREENISH, TXT_CLR_DEFAULT,
				boot_size / 1024, boot_size / 512);
			gfx_put_small_sep();
			gfx_printf(" 2: %kBOOT1      %k\n    Tam: %5d KiB (Sectores LBA: 0x%07X)\n", TXT_CLR_GREENISH, TXT_CLR_DEFAULT,
				boot_size / 1024, boot_size / 512);
			gfx_put_small_sep();
			gfx_printf(" 3: %kRPMB       %k\n    Tam: %5d KiB (Sectores LBA: 0x%07X)\n", TXT_CLR_GREENISH, TXT_CLR_DEFAULT,
				rpmb_size / 1024, rpmb_size / 512);
			gfx_put_small_sep();
			gfx_printf(" 0: %kGPP (USER) %k\n    Tam: %5d MiB (Sectores LBA: 0x%07X)\n\n", TXT_CLR_GREENISH, TXT_CLR_DEFAULT,
				emmc_storage.sec_cnt >> SECTORS_TO_MIB_COEFF, emmc_storage.sec_cnt);
			gfx_put_small_sep();
			gfx_printf("%kGPP (eMMC USER) Tabla de particion:%k\n", TXT_CLR_CYAN_L, TXT_CLR_DEFAULT);

			emmc_set_partition(EMMC_GPP);
			LIST_INIT(gpt);
			emmc_gpt_parse(&gpt);
			int gpp_idx = 0;
			LIST_FOREACH_ENTRY(emmc_part_t, part, &gpt, link)
			{
				gfx_printf(" %02d: %k%s%k\n     Tam: % 5d MiB (Sectores LBA 0x%07X)\n    Rango LBA: %08X-%08X\n",
					gpp_idx++, TXT_CLR_GREENISH, part->name, TXT_CLR_DEFAULT, (part->lba_end - part->lba_start + 1) >> SECTORS_TO_MIB_COEFF,
					part->lba_end - part->lba_start + 1, part->lba_start, part->lba_end);
				gfx_put_small_sep();
			}
			emmc_gpt_free(&gpt);
		}
	}

out:
	emmc_end();

	btn_wait();
}

void print_sdcard_info()
{
	static const u32 SECTORS_TO_MIB_COEFF = 11;

	gfx_clear_partial_grey(0x1B, 0, 1256);
	gfx_con_setpos(0, 0);

	if (sd_initialize(false))
	{
		gfx_printf("%kIDentificacion de SD:%k\n", TXT_CLR_CYAN_L, TXT_CLR_DEFAULT);
		gfx_printf(
			" Marca:      %02x\n"
			" ID de OEM:  %c%c\n"
			" Modelo:     %c%c%c%c%c\n"
			" Rev de HW:  %X\n"
			" Rev de FW:  %X\n"
			" S/N:        %08x\n"
			" Mes/Anio:   %02d/%04d\n\n",
			sd_storage.cid.manfid, (sd_storage.cid.oemid >> 8) & 0xFF, sd_storage.cid.oemid & 0xFF,
			sd_storage.cid.prod_name[0], sd_storage.cid.prod_name[1], sd_storage.cid.prod_name[2],
			sd_storage.cid.prod_name[3], sd_storage.cid.prod_name[4],
			sd_storage.cid.hwrev, sd_storage.cid.fwrev, sd_storage.cid.serial,
			sd_storage.cid.month, sd_storage.cid.year);

		u16 *sd_errors = sd_get_error_count();
		gfx_printf("%kDatos Especificos SD V%d.0:%k\n", TXT_CLR_CYAN_L, sd_storage.csd.structure + 1, TXT_CLR_DEFAULT);
		gfx_printf(
			" Clases de Cmd:  %02X\n"
			" Capacidad:      %d MiB\n"
			" Ancho de Bus:   %d\n"
			" Tasa Actual:    %d MB/s (%d MHz)\n"
			" Clase:          %d\n"
			" Grado UHS:      U%d\n"
			" Clase de Video: V%d\n"
			" Rendim. en App: A%d\n"
			" Prot. cont. Esc:%d\n"
			" Errores SDMMC:  %d %d %d\n\n",
			sd_storage.csd.cmdclass, sd_storage.sec_cnt >> 11,
			sd_storage.ssr.bus_width, sd_storage.csd.busspeed, sd_storage.csd.busspeed * 2,
			sd_storage.ssr.speed_class, sd_storage.ssr.uhs_grade, sd_storage.ssr.video_class,
			sd_storage.ssr.app_class, sd_storage.csd.write_protect,
			sd_errors[0], sd_errors[1], sd_errors[2]); // SD_ERROR_INIT_FAIL, SD_ERROR_RW_FAIL, SD_ERROR_RW_RETRY.

		int res = f_mount(&sd_fs, "", 1);
		if (!res)
		{
			gfx_puts("Obteniendo info de volumen FAT...\n\n");
			f_getfree("", &sd_fs.free_clst, NULL);
			gfx_printf("%kEncontrado %s volumen:%k\n Libre:    %d MiB\n Cluster: %d KiB\n",
					TXT_CLR_CYAN_L, sd_fs.fs_type == FS_EXFAT ? "exFAT" : "FAT32", TXT_CLR_DEFAULT,
					sd_fs.free_clst * sd_fs.csize >> SECTORS_TO_MIB_COEFF, (sd_fs.csize > 1) ? (sd_fs.csize >> 1) : 512);
			f_mount(NULL, "", 1);
		}
		else
		{
			EPRINTFARGS("Error al montar SD (Error FatFS %d).\n"
				"Asegurate si existe una particion FAT..", res);
		}

		sd_end();
	}
	else
	{
		EPRINTF("Error al iniciar SD.");
		if (!sdmmc_get_sd_inserted())
			EPRINTF("Asegurate de que este insertada.");
		else
			EPRINTF("Lector de SD no colocado correctamente!");
		sd_end();
	}

	btn_wait();
}

void print_fuel_gauge_info()
{
	int value = 0;

	gfx_printf("%kInfo de bateria:\n%k", TXT_CLR_CYAN_L, TXT_CLR_DEFAULT);

	max17050_get_property(MAX17050_RepSOC, &value);
	gfx_printf("Capacidad ahora:        %3d%\n", value >> 8);

	max17050_get_property(MAX17050_RepCap, &value);
	gfx_printf("Capacidad ahora:        %4d mAh\n", value);

	max17050_get_property(MAX17050_FullCAP, &value);
	gfx_printf("Capacidad llena:        %4d mAh\n", value);

	max17050_get_property(MAX17050_DesignCap, &value);
	gfx_printf("Capacidad (proyecto):   %4d mAh\n", value);

	max17050_get_property(MAX17050_Current, &value);
	gfx_printf("Actual ahora:           %d mA\n", value / 1000);

	max17050_get_property(MAX17050_AvgCurrent, &value);
	gfx_printf("Promedio actual:        %d mA\n", value / 1000);

	max17050_get_property(MAX17050_VCELL, &value);
	gfx_printf("Voltaje ahora:          %4d mV\n", value);

	max17050_get_property(MAX17050_OCVInternal, &value);
	gfx_printf("Voltaje circuito-abiert:%4d mV\n", value);

	max17050_get_property(MAX17050_MinVolt, &value);
	gfx_printf("Voltaje minimo:         %4d mV\n", value);

	max17050_get_property(MAX17050_MaxVolt, &value);
	gfx_printf("Voltaje maximo:         %4d mV\n", value);

	max17050_get_property(MAX17050_V_empty, &value);
	gfx_printf("Voltaje vacio (proyect):%4d mV\n", value);

	max17050_get_property(MAX17050_TEMP, &value);
	gfx_printf("Temperatura de bateria: %d.%d oC\n", value / 10,
			   (value >= 0 ? value : (~value)) % 10);
}

void print_battery_charger_info()
{
	int value = 0;

	gfx_printf("%k\n\nInfo Carga de Bateria:\n%k", TXT_CLR_CYAN_L, TXT_CLR_DEFAULT);

	bq24193_get_property(BQ24193_InputVoltageLimit, &value);
	gfx_printf("Limite voltaje de entrada: %4d mV\n", value);

	bq24193_get_property(BQ24193_InputCurrentLimit, &value);
	gfx_printf("Limite de entrada actual   %4d mA\n", value);

	bq24193_get_property(BQ24193_SystemMinimumVoltage, &value);
	gfx_printf("Limite de voltaje minimo:  %4d mV\n", value);

	bq24193_get_property(BQ24193_FastChargeCurrentLimit, &value);
	gfx_printf("Limite corriente Carga Rap:%4d mA\n", value);

	bq24193_get_property(BQ24193_ChargeVoltageLimit, &value);
	gfx_printf("Limite de voltaje de carga:%4d mV\n", value);

	bq24193_get_property(BQ24193_ChargeStatus, &value);
	gfx_printf("Estado de carga:           ");
	switch (value)
	{
	case 0:
		gfx_printf("No cargando\n");
		break;
	case 1:
		gfx_printf("Pre-cargando\n");
		break;
	case 2:
		gfx_printf("Carga rapida\n");
		break;
	case 3:
		gfx_printf("Carga terminada\n");
		break;
	default:
		gfx_printf("Desconocido (%d)\n", value);
		break;
	}
	bq24193_get_property(BQ24193_TempStatus, &value);
	gfx_printf("Estado de temperatura:     ");
	switch (value)
	{
	case 0:
		gfx_printf("Normal\n");
		break;
	case 2:
		gfx_printf("Calido\n");
		break;
	case 3:
		gfx_printf("Fresco\n");
		break;
	case 5:
		gfx_printf("Frio\n");
		break;
	case 6:
		gfx_printf("Caliente\n");
		break;
	default:
		gfx_printf("Desconocido (%d)\n", value);
		break;
	}
}

void print_battery_info()
{
	gfx_clear_partial_grey(0x1B, 0, 1256);
	gfx_con_setpos(0, 0);

	print_fuel_gauge_info();

	print_battery_charger_info();

	u8 *buf = (u8 *)malloc(0x100 * 2);

	gfx_printf("%k\n\nRegistros de carga de Bateria:\n%k", TXT_CLR_CYAN_L, TXT_CLR_DEFAULT);

	for (int i = 0; i < 0x200; i += 2)
	{
		i2c_recv_buf_small(buf + i, 2, I2C_1, MAXIM17050_I2C_ADDR, i >> 1);
		usleep(2500);
	}

	gfx_hexdump(0, (u8 *)buf, 0x200);

	btn_wait();
}

#pragma GCC pop_options
