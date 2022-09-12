/* f303_ic_control.c
 *
 * Raydium TouchScreen driver.
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.

 * Copyright (c) 2021  Raydium tech Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Google Inc. or Linaro Ltd. nor the names of
 *    its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written
 *    permission.
 * * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __KERNEL__
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#else
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#endif

#include "drv_interface.h"
#include "ic_drv_global.h"
#include "ic_drv_interface.h"
#include "f303_ic_control.h"
#include "f303_ic_reg.h"

unsigned char check_dev_id_3x(unsigned short u16_dev_id)
{
	unsigned int u32_read = 0;

	if (handle_ic_read(REG_FLASHCTL_DEVID_ADDR, 4, (unsigned char *)(&u32_read), g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		return ERROR;
	}
	if (((u32_read & 0xFFFF0000) >> 16) == u16_dev_id) {
		g_u16_dev_id = DEVICE_ID_3X;
		return SUCCESS;
	} else {
		DEBUGOUT("Device ID NG! 0x%x:0x%x\n", ((u32_read & 0xFFFF0000) >> 16), u16_dev_id);
	}
	return ERROR;
}

unsigned char check_dev_sub_version_3x(unsigned char u8_version)
{
	unsigned int u32_read = 0;

	if (handle_ic_read(REG_FLASHCTL_DEVID_ADDR, 4, (unsigned char *)(&u32_read), g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		return ERROR;
	}
	DEBUGOUT("Device Sub Version 0x%x\n", u32_read);
	if ((u32_read & 0x000000FF) == u8_version) {
		return SUCCESS;
	} else {
		DEBUGOUT("Device Sub Version NG! 0x%x:0x%x\n", (u32_read & 0x000000FF), u8_version);
	}
	return ERROR;
}

unsigned char enable_ic_block_3x(void)
{
	unsigned int u32_read = 0;

	if (handle_ic_read(REG_SYSCON_BLKEN_ADDR, 4, (unsigned char *)(&u32_read), g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		return ERROR;
	}

	u32_read |= 0xc8000000;
	/*u32_read |= (BLKEN_FIC_RB_EN | BLKEN_GPIO_RB_EN | BLKEN_SYS_RB_EN);*/
	if (handle_ic_write(REG_SYSCON_BLKEN_ADDR, 4, (unsigned char *)&u32_read, g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		return ERROR;
	}

	if (handle_ic_read(REG_SYSCON_MISCIER_ADDR, 4, (unsigned char *)(&u32_read), g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		return ERROR;
	}

	u32_read |= 0x00000404;
	/*u32_read |= (MISCIER_RB_MCU_INTO | MISCIER_RB_MCU_INT_EN);*/
	if (handle_ic_write(REG_SYSCON_MISCIER_ADDR, 4, (unsigned char *)&u32_read, g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		return ERROR;
	}
	return SUCCESS;
}

unsigned char disable_i2c_deglitch_3x(void)
{
	unsigned int u32_buf = 0;
	unsigned char u8_retry = 3, u8_comfirm_time = 3;
	unsigned char u8_check = 0, u8_i = 0;
	unsigned int u32_i2c_deglitch = I2CENG_AUTO_I2C_DGF_MODE2 | I2CENG_DGF_OSC_AUTOSEL | I2CENG_AUTO_I2C_DGF_MODE | I2CENG_I2CS_DGFEN_DLY_NUM(6);

	/*check I2C mode*/
	while (u8_retry--) {
		u32_buf = 0;
		handle_ic_read(REG_FLASHCTL_DEVID_ADDR, 4, (unsigned char *)(&u32_buf), g_u8_drv_interface, I2C_WORD_MODE);
		if ((u32_buf & 0xFFFF0000) == 0xF3030000) {
			u8_check++;
		} else
			break;
	}
	if (u8_check == 3) {
		if (!g_u8_mute_i2c_err_log)
			DEBUGOUT("PDA2 OK\n");
		return SUCCESS;
	}

	u8_retry = 100;
	while (u8_retry--) {
		if (handle_ic_write(REG_I2CENG_ADDR, 4, (unsigned char *)(&u32_i2c_deglitch), I2C_PDA_MODE, I2C_WORD_MODE) == ERROR) {
			/*DEBUGOUT("[disable_i2c_deglitch_3x] handle_ic_write I2C NG!\n");*/
			if (!g_u8_mute_i2c_err_log)
				DEBUGOUT("[DI2CDG]-W");
			continue;
		}

		u8_check = 0;
		for (u8_i = 0; u8_i < u8_comfirm_time; u8_i++) {
			/*check I2C*/
			u32_buf = 0;
			if (handle_ic_read(REG_I2CENG_ADDR, 4, (unsigned char *)(&u32_buf), I2C_PDA_MODE, I2C_WORD_MODE) == ERROR) {
				/*DEBUGOUT("[disable_i2c_deglitch_3x] 2.handle_ic_read I2C NG!\n");*/
				if (!g_u8_mute_i2c_err_log)
					DEBUGOUT("[DI2CDG]-R");
				break;
			}

			if (u32_buf == u32_i2c_deglitch) {
				u8_check++;
			} else
				break;
		}

		if (u8_check == u8_comfirm_time)
			break;
	}
	if (!g_u8_mute_i2c_err_log)
		DEBUGOUT("\n");

	if (u8_retry == 0)
		return ERROR;

	u32_buf = GPIO_DEGLITCH_EN(3);
	if (handle_ic_write(REG_GPIO_DEGLITCH_ENABLE, 4, (unsigned char *)(&u32_buf), I2C_PDA_MODE, I2C_WORD_MODE) == ERROR) {
		DEBUGOUT("[DI2CDG_3x] 3.handle_ic_write I2C NG!\n");
		return ERROR;
	}

	/*Enable PDA2*/
	u32_buf = PDA2CTL_PDA2_EN | PDA2CTL_SIE2;
	if (handle_ic_write(REG_PDA2CTL_ADDR, 4, (unsigned char *)(&u32_buf),  I2C_PDA_MODE, I2C_WORD_MODE) == ERROR) {
		DEBUGOUT("[DI2CDG_3x] 4.i2c_write_pda I2C NG!\n");
		return ERROR;
	}

	/*Disable PDA*/
#if SELFTEST
#ifdef __KERNEL__
	raydium_i2c_pda_set_address(RAYDIUM_PDA_I2CREG, DISABLE);
#else
	sysfs_mode_control(ENABLE_PDA2);
#endif
#else
	i2c_set_pda_address(I2C_EID, 0x500006, I2C_PDA2_WORD_MODE);
#endif
	return SUCCESS;
}

unsigned char stop_mcu_3x(unsigned char u8_is_tp_reset)
{
	unsigned short u16_time_out = 100;
	unsigned int u32_read_data = 0;
	unsigned int u32_write = 0;

	g_u8_mute_i2c_err_log = TRUE;
	if (u8_is_tp_reset) {
		gpio_touch_hw_reset();
		delay_ms(35);
		if (disable_i2c_deglitch_3x() == ERROR) {
			DEBUGOUT("[stop_mcu_3x] 2.DI2CDG NG!\n");
			goto EXIT_ERROR;
		}
	}

	/*DEBUGOUT("[stop_mcu_3x] 1\n");*/

	/*Stop MCU*/
	/*memset(wData, 0, sizeof(wData));*/
	u32_write = (MCU_HOLD | SKIP_LOAD);
	if (handle_ic_write(REG_FLASHCTL_FLASH_STATE_REG_ADDR, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		goto EXIT_ERROR;
	}

	u32_write = BLKRST_SW_RST;
	if (handle_ic_write(REG_SYSCON_BLKRST_ADDR, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		goto EXIT_ERROR;
	}
	delay_ms(20);

	if (disable_i2c_deglitch_3x() == ERROR) {
		/*DEBUGOUT("[stop_mcu_3x] 3.disable_i2c_deglitch_3x NG!\n");*/
		DEBUGOUT("[stop_mcu_3x] 3.DI2CDG NG!\n");
		goto EXIT_ERROR;
	}
	/*DEBUGOUT("[stop_mcu_3x] 2\n");*/

	if (handle_ic_read(REG_FLASHCTL_FLASH_STATE_REG_ADDR, 4, (unsigned char *)(&u32_read_data), g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
		DEBUGOUT("[stop_mcu_3x] 4.Flash State NG!\n");
		goto EXIT_ERROR;
	}

	while ((u32_read_data & MCU_HOLD_STATUS) == 0 && u16_time_out-- > 0) {
		delay_ms(10);
		if (handle_ic_read(REG_FLASHCTL_FLASH_STATE_REG_ADDR, 4, (unsigned char *)(&u32_read_data), g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
			goto EXIT_ERROR;
		}

#if ENABLE_WDT
		Chip_WWDT_Feed(LPC_WWDT);/* reload the Watchdog timer*/
#endif
	}

	DEBUGOUT("Stop MCU=0x%X(0x%x)(%d)!!\n", u32_read_data, (u32_read_data & MCU_HOLD_STATUS), u16_time_out);

	if ((u32_read_data & MCU_HOLD_STATUS) == 0) {
		DEBUGOUT("[stop_mcu_3x] 4.STOP MCU NG!\n");
		goto EXIT_ERROR;
	}

	g_u8_mute_i2c_err_log = FALSE;
	return SUCCESS;

EXIT_ERROR:
	g_u8_mute_i2c_err_log = FALSE;
	return ERROR;
}

unsigned char hardware_reset_3x(unsigned char u8_enable_ic_block)
{
	unsigned char u8_time_out = 200;
	DEBUGOUT("HW Reseting...\n");

	gpio_touch_hw_reset();
	delay_ms(100);

	g_u8_mute_i2c_err_log = TRUE;
	if (disable_i2c_deglitch_3x() == ERROR) {
		/*DEBUGOUT("[hardware_reset_3x] disable_i2c_deglitch_3x NG!\n");*/
		DEBUGOUT("[hardware_reset_3x] DI2CDG NG!\n");
		g_u8_mute_i2c_err_log = FALSE;
		return ERROR;
	}
	g_u8_mute_i2c_err_log = FALSE;

	if (u8_enable_ic_block) {
		if (enable_ic_block_3x() == ERROR) {
			DEBUGOUT("HW Reset NG!!\n");
			return ERROR;
		}
	}
	while (u8_time_out--) {
		if (gpio_touch_int_pin_state_access()) {
			break;
		}
		if (u8_time_out == 0) {
			return ERROR;
		}
		delay_ms(1);
	}
	return SUCCESS;
}

unsigned char set_fw_system_cmd_3x(unsigned int u32_sysm_cmd)
{
	unsigned char u8_time_out = 100, u8_value;

	if (handle_ic_write(FW_SYS_CMD_ADDR, 4, (unsigned char *)&u32_sysm_cmd, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
		return ERROR;

	/* Wait Test Command ready*/
	while (--u8_time_out) {
		delay_ms(1);
		if (handle_ic_read(FW_SYS_CMD_ADDR, 1, &u8_value, g_u8_drv_interface, I2C_BYTE_MODE) == ERROR) {
			return ERROR;
		} else if (u8_value == 0) {
			break;
		}
	}

	if (u8_time_out == 0)
		return ERROR;

	return SUCCESS;
}

unsigned char wait_fw_state_3x(unsigned int u32_addr, unsigned int u32_state, unsigned short u16_delay, unsigned short u16_retry)
{
	unsigned int u32_read_data = 0;
	do {
		if (handle_ic_read(u32_addr, 4, (unsigned char *)(&u32_read_data), g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
			return ERROR;
		}

		delay_ms(u16_delay);
		u16_retry--;

#if ENABLE_WDT
		/* reload the Watchdog timer*/
		Chip_WWDT_Feed(LPC_WWDT);
#endif
	} while ((u32_read_data != u32_state) && (u16_retry != 0));

	if (u32_read_data != u32_state) {
		return ERROR;
	}
	return SUCCESS;
}

unsigned char wait_T2D_done_state_3x(unsigned int u32_addr, unsigned short u16_delay, unsigned short u16_retry)
{
	unsigned int u32_read_data = 0;
	do {
		if (handle_ic_read(u32_addr, 4, (unsigned char *)(&u32_read_data), g_u8_drv_interface, I2C_WORD_MODE) == ERROR) {
			return ERROR;
		}
		/*DEBUGOUT("[T2DW] ready 0x%x\n", u32_read_data);*/

		delay_ms(u16_delay);
		u16_retry--;
	} while (((u32_read_data & 0x01) == 0) && (u16_retry != 0));

	if ((u32_read_data & 0x01) == 0) {
		return ERROR;
	}
	return SUCCESS;
}

unsigned char WriteToDriver_3x(unsigned char *p_u8_data, unsigned char u8_data_length)
{
	unsigned int u32_write = 0;


	if (p_u8_data != NULL) {

		if (p_u8_data[0] == 0xFE) {
			g_u8_PAGE_ADDR = p_u8_data[1];
			return SUCCESS;
		}

		if (p_u8_data[0] == 0x35) {
			u32_write = ((0x00 << 24) | (0x00 << 16) | (p_u8_data[0] << 8) | g_u8_PAGE_ADDR);
			if (handle_ic_write(REG_T2D_CONFIG_2, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;
			u32_write  = 0x0000015A;
			if (handle_ic_write(REG_T2D_CONFIG_3, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;
		}

		if (u8_data_length == 1)  /*cmd mode*/
			u32_write = ((0x00 << 24) | (p_u8_data[1] << 16) | (p_u8_data[0] << 8) | g_u8_PAGE_ADDR);
		else if (u8_data_length == 2)
			u32_write = ((0x01 << 24) | (p_u8_data[1] << 16) | (p_u8_data[0] << 8) | g_u8_PAGE_ADDR);
		else if (u8_data_length == 5) {
			/* 2A 2B*/

			u32_write = ((0x01 << 24) | (p_u8_data[1] << 16) | (p_u8_data[0] << 8) | 0x00);
			if (handle_ic_write(REG_T2D_CONFIG_2, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;
			u32_write  = 0x0000015A;
			if (handle_ic_write(REG_T2D_CONFIG_3, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;

			u32_write = ((0x01 << 24) | (p_u8_data[2] << 16) | (p_u8_data[0] << 8) | 0x01);
			if (handle_ic_write(REG_T2D_CONFIG_2, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;
			u32_write  = 0x0000015A;
			if (handle_ic_write(REG_T2D_CONFIG_3, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;

			u32_write = ((0x01 << 24) | (p_u8_data[3] << 16) | (p_u8_data[0] << 8) | 0x02);
			if (handle_ic_write(REG_T2D_CONFIG_2, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;
			u32_write  = 0x0000015A;
			if (handle_ic_write(REG_T2D_CONFIG_3, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;

			u32_write = ((0x01 << 24) | (p_u8_data[4] << 16) | (p_u8_data[0] << 8) | 0x03);
			if (handle_ic_write(REG_T2D_CONFIG_2, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;
			u32_write  = 0x0000015A;
			if (handle_ic_write(REG_T2D_CONFIG_3, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
				return ERROR;

			return SUCCESS;
		}

		if (handle_ic_write(REG_T2D_CONFIG_2, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
			return ERROR;
		u32_write  = 0x0000015A;
		if (handle_ic_write(REG_T2D_CONFIG_3, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
			return ERROR;
	}
	return SUCCESS;
}

unsigned char ReadFromDriver_3x(unsigned char *p_u8_addr, unsigned char u8_read_len, unsigned char *p_u8_output_buf)
{
	unsigned int u32_write = 0;

	if ((p_u8_addr[0] == 0xFE) || (p_u8_addr[0] == 0xFF) || (u8_read_len > 1)) {
		DEBUGOUT("[ReadFromDriver_3x] no use\n");
		return FALSE;
	}

	u32_write = ((p_u8_addr[0] << 24) | (g_u8_PAGE_ADDR << 16) | (0x01 << 8) | 0x82);
	DEBUGOUT("read address 0x%x\n", u32_write);
	if (handle_ic_write(REG_T2D_R_CONFIG_1, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
		return ERROR;
	if (handle_ic_read(REG_T2D_R_CONFIG_2, 4, p_u8_output_buf, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
		return ERROR;

	DEBUGOUT("read data 0x%x\n", p_u8_output_buf[0]);
	return TRUE;

}

unsigned char WriteDriverByTouchMode(const unsigned char *p_u8_data, uint16_t u16DataLength)
{
	unsigned short TotalDataLen = (p_u8_data[3] << 8) + p_u8_data[4] - 7;/*Valid data not include 7 byte header*/
	unsigned short u16_i = 0/*, u8_j = 0*/;
	unsigned char  u8_CurCmdLen = 0;
	unsigned int u32_write = 0;
	/*	unsigned char u8_read_buf[4];
		signed char i8_retry_cnt = 3;
		unsigned char u8_error_flag = 0;*/
	for (u16_i = 0; u16_i < (u16DataLength - 7) && u16_i < sizeof(g_u8_data_buf); u16_i++) {
		g_u8_data_buf[u16_i] = p_u8_data[u16_i + 7];
	}
	if (p_u8_data[0] == 0xF3 && p_u8_data[1] == 0x01) {
		/*Check Command type*/
		DEBUGOUT("Write Key\n");
		u32_write  = 0x015AFABC;
		if (handle_ic_write(REG_T2D_CONFIG_2, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
			return ERROR;
		u32_write  = 0x0000015A;
		if (handle_ic_write(REG_T2D_CONFIG_3, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
			return ERROR;

		if (p_u8_data[2] == 0x01 ||
		    p_u8_data[2] == 0x10) {			/*SPI short multiple command*/
			for (u16_i = 0; u16_i < TotalDataLen; u16_i += (u8_CurCmdLen + 1)) {
				u8_CurCmdLen = g_u8_data_buf[u16_i];

				/*DEBUGOUT(" i = %d\n", u16_i);
				if (u8_CurCmdLen == 2)
					DEBUGOUT("-Before Parse Data[%d] = 0x%02X, 0x%02X, 0x%02X, %d\n", u16_i, g_u8_data_buf[u16_i], g_u8_data_buf[u16_i + 1], g_u8_data_buf[u16_i + 2], u8_CurCmdLen);
				else if (u8_CurCmdLen == 3)
					DEBUGOUT("-Before Parse Data[%d] = 0x%02X, 0x%02X, 0x%02X, 0x%02X, %d\n", u16_i, g_u8_data_buf[u16_i], g_u8_data_buf[u16_i + 1], g_u8_data_buf[u16_i + 2], g_u8_data_buf[u16_i + 3], u8_CurCmdLen);
				else
					DEBUGOUT("-Before Parse Data[%d] = 0x%02X, 0x%02X, %d\n", u16_i, g_u8_data_buf[u16_i], g_u8_data_buf[u16_i + 1], u8_CurCmdLen);
				*/

				/*Check is Delay command*/
				if (g_u8_data_buf[u16_i + 1] != 0xFF) { /*Check Command ID*/

					if (wait_T2D_done_state_3x(0x50001220, 1, 100) == ERROR) {
						DEBUGOUT("[T2DW] Check T2D idle Fail\n");
						return ERROR;
					}

					WriteToDriver_3x((unsigned char *)&g_u8_data_buf[u16_i + 1], u8_CurCmdLen);
					/*delay_ms(1);*/
					/*
					i8_retry_cnt = 3;
					while (i8_retry_cnt) {
						u8_error_flag = 0;
						if (ReadFromDriver_3x((unsigned char *)&g_u8_data_buf[u16_i + 1], u8_CurCmdLen - 1 , u8_read_buf)) {
							for (u8_j = 0 ; u8_j < u8_CurCmdLen - 1 ; u8_j++) {
								if (g_u8_data_buf[u16_i + 2 + u8_j] != u8_read_buf[u8_j]) {
									u8_error_flag = 1;
									DEBUGOUT("write address:%x\n", g_u8_data_buf[u16_i + 1]);
									DEBUGOUT("Data:%d\n", g_u8_data_buf[u16_i + 2 + u8_j]);
									DEBUGOUT("g_u8_spi_cmd_read_buf[%d]:%x\n", u8_j, u8_read_buf[u8_j]);
								}
							}
							if (u8_error_flag == 0)
								break;
							if (u8_error_flag == 1)
								i8_retry_cnt--;
							if (i8_retry_cnt == 0) {
								DEBUGOUT("i8_retry_cnt error!\n");
								return ERROR;
							}
						} else
							break;
					}*/
				} else {
					DEBUGOUT("Delay\n");
					delay_ms(g_u8_data_buf[u16_i + 2]);
				}
			}
		} else {
			DEBUGOUT("[WriteDriverByTouchMode] command type not support\n");
			return ERROR;
		}
		u32_write  = 0x0100FABC;
		if (handle_ic_write(REG_T2D_CONFIG_2, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
			return ERROR;
		u32_write  = 0x0000015A;
		if (handle_ic_write(REG_T2D_CONFIG_3, 4, (unsigned char *)&u32_write, g_u8_drv_interface, I2C_WORD_MODE) == ERROR)
			return ERROR;
	}
	return SUCCESS;
}

