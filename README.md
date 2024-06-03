nRF91-series workshop
---------------------
This repository contains a modified version of the Zephyr "Hello World" sample found in the nRF Connect SDK. This sample serves as a starting point to demonstrate how relatively simple examples can be extended with additional functionality to implement more advanced use cases.

The goal of this workshop is to connect the nRF9151DK to a cloud service, sending and receiving messages to and from the cloud interface, which will be shown on the display. Starting from the simple "Hello World" example, the application will be extended incrementally:

1. Advanced Logging: Enhance the basic example by adding more advanced logging capabilities.
2. OLED Display: Integrate an OLED display to show information.
3. LTE Connection: Add LTE connectivity to enable the device to connect to a network and extend the application to display the connection status on the OLED.
4. Cloud Connectivity via CoAP: Implement cloud connectivity using the CoAP protocol, allowing the device to provision over-the-air to the nRF Cloud.
5. Cloud-to-Device Messaging: Send a message from nRF Cloud to the device and display it on the OLED.

## HW Requirements
- 1 x nRF9151DK
- 1 x USB C cable
- 1 x OLED display (SSD1306) 
- 4 x jumper wires (GND, VDD, SDA, SCL)

## SW Requirements
- Register an account on https://nrfcloud.com/
- nRF Connect for Desktop
    - Quick Start
    - Board configurator
    - Cellular Monitor
    - Serial Terminal
- Visual Studio Code (from here referred to as VS Code)
    - nRF Connect for VS Code extension
    - nRF Connect SDK v2.6.1 installed through VS Code extension

## Workshop steps


### Step 1 Setting up the starting point

In VSCode, in the Welcome page of the nRF Connect interface, click on 'Create a new application'. From the dropdown menu select 'Copy a sample'.
 Hint: A faster way to do this is to bring up the command palette in VSCode (by pressing F1), typing 'new application from sample', and pressing Enter. 


![Create a new application from a sample](/pics/step1_copy-sample.png)

Search for Hello World and click Enter. Then choose the folder you wish to store the sample e.g. `C:\temp\projects\workshop`. Note: The last part of the path is the project name in which the project files are stored.

![Choose sample to Copy](/pics/step1_copy-sample_hello_world.png)

![Choose folder](/pics/step1_copy-sample_hw_folder.png)

VS Code will open the folder in built-in file explorer. Click on main.c under src to open the code itself.

![Hello World](/pics/step1_hello_world.png)

From the nRF Connect for VS Code extension find the hello_world project under applications. 
![Hello World](/pics/step1_hello_world-build.png)

Click on Add build configuration. Select nRF9151dk_nrf9151_ns from the boards dropdown list and click Build configuration.

![Hello World - Build configuration](/pics/step1_hello_world-build2.png)

Make sure your device is conncted and discovered in the Connected devices list. Then once the build is completed, click on Flash. 
Open a terminal from the dropdown menu of the connected devices menu. A new dropdown menu will appear from the top menu of VS Code. Select the device and '115200 8n1' when prompted to select configuration.

![Hello World - Flash device](/pics/step1_hello_world-flash_Terminal.png)

Verify that the Hello World sample worked by pressing reset on the DK.
![Hello World - Open the terminal](/pics/step1_hello_world-flash_Terminal3.png)

Next we are going to add more advanced logging, utilizing the Zephyr OS Logging module. The logger module is the recommended method for sending messages to a console, unlike the printk() function, which will not return until all bytes of the message are sent.
Please see the [Developer Academy Lesson 4 - Printing messages to console and logging](https://academy.nordicsemi.com/courses/nrf-connect-sdk-fundamentals/lessons/lesson-3-printing-messages-to-console-and-logging/topic/logger-module/) for more information
In prj.conf enable the following configuration
```C
CONFIG_LOG=y
```
![Hello world - Add logging](/pics/step1_hello_world-Logging.png)
Add the following include to the top of your main.c file:

```C
#include <zephyr/logging/log.h> 
```
![Hello world - Add logging](/pics/step1_hello_world-Logging2.png)

Add the following line beneath the added line above and over main(), around line 10 of main.c
```C
LOG_MODULE_REGISTER(workshop_step1,LOG_LEVEL_DBG);
```

Now we can replace printf with LOG_INF. 
In the main() function replace printf with LOG_INF

```C
LOG_INF("Hello World! %s\n", CONFIG_BOARD);
```
![Hello world - Add logging](/pics/step1_hello_world-Logging3.png)

Rebuild the project and flash the sample. The output in the terminal will now show the new log message with timestamp and show which module the log message belongs to.
![Hello world - Add logging](/pics/step1_hello_world-Logging4.png)

### Step 2 Adding an OLED display

In the following step we are going to add an OLED display to the nRF9151DK. The display is enabled through the KConfig interface and the devicetree. 
_The following code is based on the Character Frame Buffer samples in the Zephyr repository._

In order for the display to be discovered we need to add a devicetree overlay telling our I2C bus what address to access, and additional configurations.  We need to create an overlay with the file name nrf9151dk_nrf9151_ns.overlay and add it to the root project folder.

Right click in the project folder in file explorer and click new file. Type in `nrf9151dk_nrf9151_ns.overlay` as name.
Add the following to the .overlay file
```C
#include <zephyr/dt-bindings/input/input-event-codes.h>

/ {
	chosen {
		zephyr,display = &ssd1306_ssd1306_128x64;
	};
};

&i2c2 {
	status = "okay";
	zephyr,flash-buf-max-size = <64>;
	zephyr,concat-buf-size = <2048>;

	ssd1306_ssd1306_128x64: ssd1306@3c {
		compatible = "solomon,ssd1306fb";
		reg = <0x3c>;
		width = <128>;
		height = <64>;
		segment-offset = <0>;
		page-offset = <0>;
		display-offset = <0>;
		multiplex-ratio = <63>;
		segment-remap;
		com-invdir;
		prechargep = <0x22>;
	};
};
```

![Hello world - Add devicetree overlay](/pics/step2_add_display-overlay.png)

In the project configuration, prj.conf, copy and paste the following KConfig configurations to enable:
```C
# Logging
CONFIG_LOG=y
CONFIG_CFB_LOG_LEVEL_DBG=y

# Display
CONFIG_DISPLAY=y
CONFIG_CHARACTER_FRAMEBUFFER=y
CONFIG_SHELL=y
CONFIG_CHARACTER_FRAMEBUFFER_SHELL=y

# System
CONFIG_HEAP_MEM_POOL_SIZE=16384
```

Add the following includes to main.c
```C
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/kernel.h>
```

Add the following variable just below LOG_MODULE_REGISTER in main.c
```C
const struct device *dev;
uint16_t x_res;
uint16_t y_res;
uint16_t rows;
uint8_t ppt;
uint8_t font_width;
uint8_t font_height;
```

Add the following code below the variables in main.c, above the main() function
```C
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
	cfb_framebuffer_set_font(dev,3);

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
```
Add the following code to the main() function right before the *LOG_INF("Hello World! %s\n", CONFIG_BOARD);* line
```C
display_init();
``` 

Add the following code at the end of the main() function, right above *return 0;*
```C
	cfb_framebuffer_clear(dev, false);
	cfb_framebuffer_set_font(dev,0);
	cfb_print(dev, "Nordic", 40, 1);
	cfb_print(dev, "Semiconductor", 20, 10);
	cfb_print(dev, "EMEA FAE Workshop"12, 20);
	cfb_framebuffer_finalize(dev);
```

As we have added the .overlay file we need to do a pristine build of the project. 
Click on the rebuild icon next to the build button in the Actions pane in the VS Code extension
![Add display - pristine build](/pics/step2_add_display-overlay-pristine.png)

Before we flash the device, we need to connect the display to the development kit. Connect the four jumper wires to the pin header, GND, VDD, SDA and SCL. GND and VDD are connected to GND and VDD on the DK. While SDA and SCL are connected to the configured GPIO on the DK. Please see the overview on the bottom of the board for which pins to connect. 
Open the Board configurator from the nRF Connect for Desktop and click on 3.3V under VDD (nPM VOUT1) and then click Write config to set output to 3.3V.
![Configure board VOUT1](/pics/step2_add_display-vOUT_board.conf.png)


Once connected, click Flash. Congratulations you now have a display to show information on.


### Step 3
Next we are going to add LTE connectivity to enable the device to connect to a network and extend the application to display the connection status in the OLED display. 
We need to add the nRF Modem library to interface the modem and the LTE Link Control library to control the LTE link in the nRF91.

Add these lines to your prj.conf to enable both libraries
```C
# Modem
CONFIG_NRF_MODEM_LIB=y
CONFIG_LTE_LINK_CONTROL=y
CONFIG_LTE_NETWORK_MODE_LTE_M=y
```

Add these includes list of includes in main.c, at line 
```C
/** Add includes needed for LTE connection **/
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
```
Add the following line below the LOG_MODULE_REGISTER line. 
```C
/* Semaphore used to block the main thread until modem has established
 * an LTE connection.
 */
K_SEM_DEFINE(lte_connected, 0, 1);
```

Add the following LTE event handler before the main() function in main.c
```C
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
```
Add the function modem_configure_and_connect() below the lte_handler() to initialize the modem library and the LTE connection.
```C
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
```
In the beginning of the main() function, add the follwing code around line 173
```C
    int err;

	err = modem_configure_and_connect();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}

	k_sem_take(&lte_connected, K_FOREVER);
```


Build and flash the sample to your board. The output should now be similar to this:
![Step 3 - LTE output](/pics/step3_LTE-output.png) 

Add the following code in the LTE handler, under the LOG_INF line in LTE_LC_EVT_RRC_UPDATE case, before 'break':
```C
if (evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED) {
cfb_print(dev, "RRC Connected", 20, 40);
cfb_framebuffer_finalize(dev);

} else if (evt->rrc_mode == LTE_LC_RRC_MODE_IDLE) {
cfb_print(dev, "RRC Idle", 20, 40);
cfb_framebuffer_finalize(dev);
} else {
cfb_print(dev, "              ", 20, 40);
cfb_framebuffer_finalize(dev);
}
```
This is a very crude way to show the connection status in the display. 
![Step 3 - OLED display](/pics/step3_LTE-display_output.jpg)


### Step 4
Cloud Connectivity via CoAP: Implement cloud connectivity using the CoAP protocol, allowing the device to provision over-the-air to the nRF Cloud using provisioning service. This service allows you to securely provision your Nordic Semiconductor devices over-the-air to nRF Cloud. In this step we will combine elements from the Cellular: nRF Device provisioning sample with a simple CoAP application to enable the device to be added to nRF Cloud device list.

In prj.conf add the following Kconfig configuration options to enable provisioning and the CoAP protocol.

Under `# Logging` copy and paste the following options. This will enable more logging from the different modules used by the project.
```C
# Logging
CONFIG_NRF_MODEM_LIB_TRACE=n
CONFIG_LOG=y
CONFIG_SERIAL=y
CONFIG_THREAD_NAME=y

CONFIG_NRF_CLOUD_LOG_LEVEL_DBG=y
CONFIG_NET_LOG=y
CONFIG_COAP_LOG_LEVEL_DBG=y
CONFIG_LTE_LINK_CONTROL_LOG_LEVEL_DBG=n
CONFIG_NRF_PROVISIONING_LOG_LEVEL_DBG=y
```

Under `# Modem` copy and paste the following options.
```C
# Modem
CONFIG_NEWLIB_LIBC=y

CONFIG_NETWORKING=y
CONFIG_NET_SOCKETS=y
CONFIG_NET_SOCKETS_OFFLOAD=y
CONFIG_NET_SOCKETS_POSIX_NAMES=y
CONFIG_NET_IPV6=n
CONFIG_LTE_PSM_REQ=n
``` 


Under `# System` copy and paste the following options
```C
CONFIG_MAIN_STACK_SIZE=2048
``` 

Copy and paste the following configuration options at the end of prj.conf. 
```C
# nRF Cloud CoAP
CONFIG_NRF_CLOUD_SEC_TAG=111
CONFIG_NRF_CLOUD_CLIENT_ID_SRC_INTERNAL_UUID=y
CONFIG_MODEM_JWT=y
CONFIG_NRF_CLOUD_COAP=y
CONFIG_NEWLIB_LIBC_FLOAT_PRINTF=y

CONFIG_COAP_CLIENT_BLOCK_SIZE=1024
CONFIG_COAP_CLIENT_STACK_SIZE=6144

# nRF Provisioning
CONFIG_MODEM_KEY_MGMT=y
CONFIG_MODEM_ATTEST_TOKEN=y
CONFIG_DATE_TIME=y

CONFIG_NRF_PROVISIONING=y
CONFIG_NRF_PROVISIONING_COAP=y
CONFIG_NRF_PROVISIONING_ROOT_CA_SEC_TAG=111110
CONFIG_COAP_EXTENDED_OPTIONS_LEN=y
CONFIG_COAP_EXTENDED_OPTIONS_LEN_VALUE=64
CONFIG_COAP_CLIENT_MESSAGE_SIZE=1536
CONFIG_NRF_PROVISIONING_AUTO_INIT=n
CONFIG_NRF_PROVISIONING_CODEC=y
CONFIG_NRF_PROVISIONING_CBOR=y
CONFIG_ZCBOR=y
CONFIG_ZCBOR_CANONICAL=y
CONFIG_NRF_PROVISIONING_CBOR_RECORDS=20
CONFIG_NRF_PROVISIONING_RX_BUF_SZ=2048

CONFIG_SETTINGS=y
CONFIG_SETTINGS_FCB=y
CONFIG_FCB=y
CONFIG_FLASH_MAP=y
CONFIG_FLASH=y
CONFIG_FLASH_PAGE_LAYOUT=y

CONFIG_MODEM_INFO=y
CONFIG_MODEM_INFO_ADD_DEVICE=n
CONFIG_MODEM_INFO_ADD_NETWORK=n
CONFIG_MODEM_INFO_ADD_SIM=n
``` 


In the start of main.c, in the include list add the following includes
```C
#include <modem/modem_key_mgmt.h>
#include <net/nrf_provisioning.h>
#include <zephyr/sys/reboot.h>
#include "nrf_provisioning_at.h"
``` 

Add the following semaphore with the lte_connected semaphore
```C
K_SEM_DEFINE(provisioning_complete, 0, 1);
```

Add the following structures below the semaphores
```C
static struct nrf_provisioning_dm_change dmode;
static struct nrf_provisioning_mm_change mmode;
```

Add the following code after the LTE Link handler
```C
static void reboot_device(void)
{
	/* Disconnect from network gracefully */
	int ret = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_OFFLINE);

	if (ret != 0) {
		LOG_ERR("Unable to set modem offline, error %d", ret);
	}

	sys_reboot(SYS_REBOOT_WARM);
}
```


Add the following code after the code above
```C
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

		LOG_INF("Network time obtained");
		ret = fmode;
	} else {
		ret = lte_lc_func_mode_set(new_mode);
		if (ret == 0) {
			LOG_INF("Modem set to requested state %d", new_mode);
			ret = fmode;
		}
	}

	return ret;
}
```




Add the following code after the code above
```C
static void device_mode_cb(enum nrf_provisioning_event event, void *user_data)
{
	ARG_UNUSED(user_data);

	switch (event) {
	case NRF_PROVISIONING_EVENT_START:
		LOG_INF("Provisioning started");
		break;
	case NRF_PROVISIONING_EVENT_STOP:
		LOG_INF("Provisioning stopped");
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
```

Add the following code in the beginning of the main() function, after *int err*
```C
mmode.cb = modem_mode_cb;
mmode.user_data = NULL;
dmode.cb = device_mode_cb;
dmode.user_data = NULL;
```

At the end of the main() function, add the following code
```C
err = nrf_provisioning_init(&mmode, &dmode);
	if (err) {
		LOG_ERR("Failed to initialize provisioning client");
	}

k_sem_take(&provisioning_complete, K_FOREVER);
LOG_INF("Provisioning complete");

```

You should now be able to build the project and flash your device. The output will have a few errors, which is expected as the device is not yet claimed in nRF Cloud. 
![Step 4 - Provisioning output](/pics/step4_provisioning_output.png)
If no attestation token is printed and the error `nrf_provisioning_coap: Failed to connect UDP socket` is printed then you may need to check if `CONFIG_NET_IPV6=n` is set in prj.conf.

In the output logs your device should now print 'Attestation token for claiming device on nRFCloud'. Copy the token value and go to [nrfcloud.com](www.nrfcloud.com) and navigate to Security services and click on Claimed Devices. 
![nRF Cloud claim device](/pics/step4_nrfcloud-claim1.png)

Now click on Claim Device in the top right corner
![nRF Cloud claim device button](/pics/step4_nrfcloud-claim2.png)

In the claim device pop-up paste your attestation token printed from the output logs, enable 'Onboard to nRF Cloud'. Enter security tag 111 and choose CoAP minimal as Server certificate. Click Claim Device.
![nRF Cloud claim device pop-up](/pics/step4_nrfcloud-claim3.png)

Your device should be successfully be claimed in nRF Cloud. 
![nRF Cloud claim device button](/pics/step4_nrfcloud-claim4.png)

On you development kit press the reset button. This will start the process of provisioning your device, downloading the needed certificates from nRF Cloud and restarting the device automatically as needed. 

Once the process is done, you can verify that your device is added to the device list in [nrfcloud.com](www.nrfcloud.com) by clicking Device Management and Devices. Note down the ID from the claimed devices list in case you have many devices.
![nRF Cloud device management](/pics/step4_nrfcloud-device1.png)

We can see the device is in our list of devices
![nRF Cloud device management confirmed](/pics/step4_nrfcloud-device2.png)

### Step 5
Cloud-to-Device Messaging: Send a message from nRF Cloud to the device and display it on the OLED. In this step we will add a crude way to send a message to the device using the shadow information from nRF Cloud.

Add a new file and rename it to KConfig. In this file add the following code
```C
config CLOUD_POLL_INTERVAL
    int "How often to poll the device shadow, in seconds"
    default 60

config SHADOW_BUFFER_SIZE
    int "How big buffer to allocate for the shadow"
    default 1024

module = CLOUD_DISPLAY
module-str = Cloud display
source "${ZEPHYR_BASE}/subsys/logging/Kconfig.template.log_config"

menu "Zephyr Kernel"
source "Kconfig.zephyr"
endmenu 
```

In the folder `src` add a new file and rename to cloud.h. Copy and paste the following code to cloud.h
```C
#ifndef __CLOUD_H__
#define __CLOUD_H__

int cloud_thread(void);

#endif /* __CLOUD_H__ */
```

Add another new file to `src` and rename this file to cloud_coap.c. 
At the start of this file add the following includes
```C
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <net/nrf_cloud_coap.h>
#include <cJSON.h>

#include "cloud.h"
```

To allow logging from this file, add the following code beneath the include list
```C
LOG_MODULE_REGISTER(cloud, CONFIG_CLOUD_DISPLAY_LOG_LEVEL);
```

In prj.conf under `# Logging`, copy and paste the following options. This will enable the `CONFIG_CLOUD_DISPLAY_LOG_LEVEL` defined in KConfig and allowing the LOG Module to use this log level.
```C
# Logging
CONFIG_CLOUD_DISPLAY_LOG_LEVEL_DBG=y
```

Coming back to `/src/cloud_coap.c` we need to add some variables, structs and defines. Copy the following lines and paste below the LOG_MODULE_REGISTER
```C
extern const struct device *dev; //Enable access to display
static void shadow_poll_timer_handler(struct k_timer *id);

K_SEM_DEFINE(shadow_poll_sem, 0, 1);
K_TIMER_DEFINE(shadow_poll_timer, shadow_poll_timer_handler, NULL);

char shadow_buffer[CONFIG_SHADOW_BUFFER_SIZE];
```

Add the following code to enable `shadow_poll_timer_handler` below the defines, around line 18.
```C
static void shadow_poll_timer_handler(struct k_timer *id)
{
    k_sem_give(&shadow_poll_sem);
}
```
Add the following code beneath the `shadow_poll_timer_handler`:
```C
/* display_string will be pointing to the string value inside the cJSON object,
    and will be freed together with input */
static int parse_config(cJSON *input, char **display_string)
{
    if (input == NULL) {
        return -EINVAL;
    }

    LOG_DBG("Shadow:\n%s\n", cJSON_Print(input));

    cJSON *config = cJSON_GetObjectItem(input, "config");
    if (config == NULL) {
        LOG_ERR("Could not find config object");
        return -1;
    }
    
    cJSON *display = cJSON_GetObjectItem(config, "display");
    if (display == NULL) {
        LOG_ERR("Could not find display config");
        return -1;
    }

    *display_string = cJSON_GetStringValue(display);
    return 0;
}
```
Add the following code after the code above
```C 
/* json is an allocated string that must be freed by the caller */
static int encode_config(char *display_string, char **json)
{
    if (display_string == NULL) {
        return -EINVAL;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        LOG_ERR("Failed to create root object");
        return -1;
    }

    cJSON *config = cJSON_CreateObject();
    if (config == NULL) {
        LOG_ERR("Failed to create config object");
        return -1;
    }

    cJSON_AddItemToObject(root, "config", config);

    if (cJSON_AddStringToObject(config, "display", display_string) == NULL) {
        LOG_ERR("Failed to add display string");
        cJSON_Delete(root);
        return -1;
    }

    *json = cJSON_Print(root);
    if (json == NULL) {
        LOG_ERR("Failed to print JSON string");
        cJSON_Delete(root);
        return -1;
    }

    cJSON_Delete(root);
    return 0;
}
```
At the end of the file add the following function
```C
int cloud_thread(void)
{

}
``` 

In cloud_thread() define the following integer at the top
```C 
	int err;
```

Following the integer add the following code in cloud_thread(). Here we initialize the nRF Cloud CoAP communication and connect to nRF Cloud via CoAP.
```C
    LOG_INF("Cloud thread started");
    
    err = nrf_cloud_coap_init();
    if (err) {
        LOG_ERR("Failed to init nRF Cloud CoAP: %d", err);
        return err;
    }

    err = nrf_cloud_coap_connect(NULL);
    if (err) {
        LOG_ERR("Failed to connect to nRF Cloud");
        return err;
    }
```

Next we need to configure JSON, part of the nRF Cloud communication. Add the following code after the cloud connection above
```C
    LOG_DBG("Setting initial config");
    char *json = NULL;
    err = encode_config("", &json);
    if (err || json == NULL) {
        LOG_ERR("Failed to encode default config. err: %d, json: %p", err, json);
        return err;
    }
```

Now we can update the shadow information from nRF Cloud. Add the following code at the end of cloud_thread() function
```C
err = nrf_cloud_coap_shadow_state_update(json);
    if (err) {
        LOG_ERR("Failed to update shadow state: %d", err);
        return err;
    }
    free(json);
    json = NULL;

    k_timer_start(&shadow_poll_timer, K_NO_WAIT, K_SECONDS(CONFIG_CLOUD_POLL_INTERVAL));
```
Add a while loop after the last lines above
```C
while(true) {

}
```

In the while loop add the following code. Here we download the shadow data.
```C
        k_sem_take(&shadow_poll_sem, K_FOREVER);

        LOG_INF("Getting shadow");
        err = nrf_cloud_coap_shadow_get(shadow_buffer, CONFIG_SHADOW_BUFFER_SIZE, true);
        if (err) {
            LOG_ERR("Failed to get shadow delta: %d", err);
            continue;
        } else if (err == 0 && strlen(shadow_buffer) == 0) {
            LOG_INF("No changes to the shadow");
            continue;
        }

        LOG_DBG("Shadow delta:\n%s\n", shadow_buffer);

        struct nrf_cloud_data shadow_delta = {
            .ptr = shadow_buffer,
            .len = strlen(shadow_buffer)
        };
        struct nrf_cloud_obj shadow_config;
```

Next we will process the shadow data. Add the following code after the last code above:
```C
 		err = nrf_cloud_coap_shadow_delta_process(&shadow_delta, &shadow_config);
        if (err) {
            LOG_ERR("Failed to process shadow delta: %d", err);
            goto shadow_cleanup;
        } else if (shadow_config.type != NRF_CLOUD_OBJ_TYPE_JSON) {
            LOG_ERR("Unsupported nRF Cloud object type: %d", shadow_config.type);
            goto shadow_cleanup;
        }

        char *string = NULL;
        err = parse_config(shadow_config.json, &string);
        if (err || string == NULL) {
            LOG_ERR("Failed to parse shadow: %d", err);
            goto shadow_cleanup;
        }
        LOG_DBG("Display: %s", string);
```

Now that we have downloaded and processed the shadow data, we can print the information in the OLED display.
Here we can print the string from above. Add the following code after the code above around line 164
```C
 		//Print string to OLED display
        cfb_print(dev, "              ", 20, 50);
		cfb_print(dev, string, 20, 50);
        cfb_framebuffer_finalize(dev);
```

Add the following code to report back to nRF Cloud that the shadow information is used. 
```C
  err = encode_config(string, &json);
        if (err || json == NULL) {
            LOG_ERR("Failed to encode default config");
            continue;
        }

        err = nrf_cloud_coap_shadow_state_update(json);
        if (err) {
            LOG_ERR("Failed to update shadow state: %d", err);
            goto json_cleanup;
        }

json_cleanup:
        free(json);
        json = NULL;
        
shadow_cleanup:
        err = nrf_cloud_obj_free(&shadow_config);
        if (err) {
            LOG_ERR("Failed to free shadow object: %d", err);
            continue;
```


In CMakesLists.txt add the following code at the end
```C
target_include_directories(app PRIVATE src)
target_sources_ifdef(CONFIG_NRF_CLOUD_COAP app PRIVATE src/cloud_coap.c)
```

In main.c add the following includes to the include list
```C
#include <cJSON.h>
#include "cloud.h"

```

Add the following function from cloud_coap.c to the end of main() function in main.c
```C
cloud_thread();
```

Build and flash your device again. Navigating back to the device management list in nRF Cloud, click on your device and you should see a new button added to the "top menu" called View Config
![Step 5 - nRF Cloud View Config](/pics/Step5_nrfcloud_ViewConfig.png)

The view config menu item allows us to configure the device and send a desired configuration which we can print in the OLED display. From the parse_config() function in cloud_coap.c we can see that it looks after a `display` object.
In the device configuration we can add a desired configuration with the following code
```C
{
  "display": "Message for display"
}
```
