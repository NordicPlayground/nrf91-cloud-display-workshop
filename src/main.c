/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/logging/log.h> 

#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/kernel.h>

/** Add includes needed for LTE connection **/
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#include <modem/modem_key_mgmt.h>
#include <net/nrf_provisioning.h>
#include <zephyr/sys/reboot.h>
#include <cJSON.h>

#include "nrf_provisioning_at.h"

#include "cloud.h"

LOG_MODULE_REGISTER(workshop_code,LOG_LEVEL_DBG);
/* Semaphore used to block the main thread until modem has established
 * an LTE connection.
 */
K_SEM_DEFINE(lte_connected, 0, 1);
K_SEM_DEFINE(provisioning_complete, 0, 1);

static struct nrf_provisioning_dm_change dmode;
static struct nrf_provisioning_mm_change mmode;

const struct device *dev;
uint16_t x_res;
uint16_t y_res;
uint16_t rows;
uint8_t ppt;
uint8_t font_width;
uint8_t font_height;

static void lte_handler(const struct lte_lc_evt *const evt)
{
        switch (evt->type) {
        case LTE_LC_EVT_NW_REG_STATUS:
                if (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME &&
                    evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING) {
                        break;
                }

                LOG_INF("Connected to: %s network",
                       evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "home" : "roaming");

                k_sem_give(&lte_connected);
                break;

        case LTE_LC_EVT_PSM_UPDATE:
        case LTE_LC_EVT_EDRX_UPDATE:
        case LTE_LC_EVT_RRC_UPDATE:
        	LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? 
			"Connected" : "Idle");
			if (evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED) {
			cfb_print(dev, "              ", 20, 40);
			cfb_print(dev, "RRC Connected", 20, 40);
			cfb_framebuffer_finalize(dev);
			
			} else if (evt->rrc_mode == LTE_LC_RRC_MODE_IDLE) {
			cfb_print(dev, "              ", 20, 40);
			cfb_print(dev, "RRC Idle", 20, 40);
			cfb_framebuffer_finalize(dev);
			} else {
			cfb_print(dev, "              ", 20, 40);
			cfb_framebuffer_finalize(dev);
			}

	        break;	
        case LTE_LC_EVT_CELL_UPDATE:
        case LTE_LC_EVT_LTE_MODE_UPDATE:
        case LTE_LC_EVT_TAU_PRE_WARNING:
        case LTE_LC_EVT_NEIGHBOR_CELL_MEAS:
        case LTE_LC_EVT_MODEM_SLEEP_EXIT_PRE_WARNING:
        case LTE_LC_EVT_MODEM_SLEEP_EXIT:
        case LTE_LC_EVT_MODEM_SLEEP_ENTER:
        case LTE_LC_EVT_MODEM_EVENT:
                /* Handle LTE events */
                break;

        default:
                break;
        }
}

static void reboot_device(void)
{
	/* Disconnect from network gracefully */
	int ret = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_OFFLINE);

	if (ret != 0) {
		LOG_ERR("Unable to set modem offline, error %d", ret);
	}

	sys_reboot(SYS_REBOOT_WARM);
}


static int modem_mode_cb(enum lte_lc_func_mode new_mode, void *user_data)
{
	enum lte_lc_func_mode fmode;
	char time_buf[64];
	int ret;

	ARG_UNUSED(user_data);

	if (lte_lc_func_mode_get(&fmode)) {
		LOG_ERR("Failed to read modem functional mode");
		ret = -EFAULT;
		return ret;
	}

	if (fmode == new_mode) {
		ret = fmode;
	} else if (new_mode == LTE_LC_FUNC_MODE_NORMAL) {
		/* Use the blocking call, because in next step
		 * the service will create a socket and call connect()
		 */
		ret = lte_lc_connect();

		if (ret) {
			LOG_ERR("lte_lc_connect() failed %d", ret);
		}
		LOG_INF("Modem connection restored");

		LOG_INF("Waiting for modem to acquire network time...");

		do {
			k_sleep(K_SECONDS(3));
			ret = nrf_provisioning_at_time_get(time_buf, sizeof(time_buf));
		} while (ret != 0);

		LOG_WRN("Network time obtained");
		ret = fmode;
	} else {
		ret = lte_lc_func_mode_set(new_mode);
		if (ret == 0) {
			LOG_ERR("Modem set to requested state %d", new_mode);
			ret = fmode;
		}
	}

	return ret;
}

static void device_mode_cb(enum nrf_provisioning_event event, void *user_data)
{
	ARG_UNUSED(user_data);

	switch (event) {
	case NRF_PROVISIONING_EVENT_START:
		LOG_INF("Provisioning started");
		break;
	case NRF_PROVISIONING_EVENT_STOP:
		LOG_WRN("Provisioning stopped");
		k_sem_give(&provisioning_complete);
		break;
	case NRF_PROVISIONING_EVENT_DONE:
		LOG_INF("Provisioning done, rebooting...");
		reboot_device();
		break;
	default:
		LOG_ERR("Unknown event");
		break;
	}
}



static int modem_configure_and_connect(void)
{
	int err;

	LOG_INF("Initializing modem library");
	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("Failed to initialize the modem library, error: %d", err);
		return err;
	}

	LOG_INF("Connecting to LTE network");
	err = lte_lc_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
		return err;
	}
	return 0;
}

int display_init(void)
{

    /* Get devicetree identifier and make sure it is ready to use */
	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(dev)) {
		LOG_ERR("Device %s not ready", dev->name);
		return 0;
	}

	if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO10) != 0) {
		if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO01) != 0) {
			LOG_ERR("Failed to set required pixel format");
			return 0;
		}
	}

	LOG_INF("Initialized %s", dev->name);

	if (cfb_framebuffer_init(dev)) {
		LOG_ERR("Framebuffer initialization failed!");
		return 0;
	}
	cfb_framebuffer_clear(dev, true);

	display_blanking_off(dev);
	x_res = cfb_get_display_parameter(dev, CFB_DISPLAY_WIDTH);
	y_res = cfb_get_display_parameter(dev, CFB_DISPLAY_HEIGH);
	rows = cfb_get_display_parameter(dev, CFB_DISPLAY_ROWS);
	ppt = cfb_get_display_parameter(dev, CFB_DISPLAY_PPT);
	LOG_INF("x_res %d, y_res %d, ppt %d, rows %d, cols %d",
	       x_res,
	       y_res,
	       ppt,
	       rows,
	       cfb_get_display_parameter(dev, CFB_DISPLAY_COLS));

	cfb_framebuffer_invert(dev);

	cfb_set_kerning(dev, 1);
	cfb_framebuffer_set_font(dev,0);

	/* List available fonts */
	for (int idx = 0; idx < 3; idx++) {
		if (cfb_get_font_size(dev, idx, &font_width, &font_height)) {
			break;
		}
		cfb_framebuffer_set_font(dev, idx);
		LOG_WRN("font width %d, font height %d",
		       font_width, font_height);
	}

	return 0;
}

int main(void)
{
	int err;

	mmode.cb = modem_mode_cb;
	mmode.user_data = NULL;
	dmode.cb = device_mode_cb;
	dmode.user_data = NULL;

	LOG_INF("Hello World! %s\n", CONFIG_BOARD);

	display_init();

	err = modem_configure_and_connect();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}

	k_sem_take(&lte_connected, K_FOREVER);

	cfb_framebuffer_clear(dev, false);
	cfb_framebuffer_set_font(dev,0);
	cfb_print(dev, "Nordic", 40, 1);
	cfb_print(dev, "Semiconductor", 20, 10);
	cfb_print(dev, "EMEA FAE Workshop",12, 20);
	cfb_framebuffer_finalize(dev);

	err = nrf_provisioning_init(&mmode, &dmode);
		if (err) {
		LOG_ERR("Failed to initialize provisioning client");
		}
	k_sem_take(&provisioning_complete, K_FOREVER);
	LOG_INF("Provisioning complete");

	cloud_thread();

	return 0;
}
