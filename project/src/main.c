/**
 * @file
 * @copyright 2016 opencxa.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @author Christopher Armenio
 */


// ******** includes ********
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "nvs_flash.h"

#include "aws_certs.h"

#include <cxa_assert.h>
#include <cxa_blueGiga_btle_client.h>
#include <cxa_console.h>
#include <cxa_delay.h>
#include <cxa_esp32_btle_client.h>
#include <cxa_esp32_gpio.h>
#include <cxa_esp32_timeBase.h>
#include <cxa_esp32_usart.h>
#include <cxa_mqtt_connectionManager.h>
#include <cxa_mqtt_rpc_node_root.h>
#include <cxa_network_wifiManager.h>
#include <cxa_ioStream_bridge.h>
#include <cxa_runLoop.h>
#include <cxa_uniqueId.h>

#include <ovr_beaconManager.h>

#define CXA_LOG_LEVEL			CXA_LOG_LEVEL_TRACE
#include <cxa_logger_implementation.h>


// ******** local macro definitions ********
#define MQTT_SERVER 		"A218ORPUJ66O9J.iot.us-east-1.amazonaws.com"
#define MQTT_PORT 			8883


// ******** local type definitions *******


// ******** local function prototypes ********
static void sysInit();
static void userTask(void *pvParameters);

static void consoleCb_noti(cxa_array_t *const argsIn, cxa_ioStream_t *const ioStreamIn, void* userVarIn);


// ******** local variable declarations ********
static cxa_esp32_gpio_t gpio_led;
static cxa_esp32_usart_t usart_debug;

static cxa_esp32_gpio_t gpio_btleReset;
static cxa_esp32_usart_t usart_btle;
static cxa_blueGiga_btle_client_t btleClient;

static ovr_beaconManager_t beaconManager;

static cxa_mqtt_rpc_node_root_t rpcNode_root;


// ******** global function implementations ********
void app_main(void)
{
    nvs_flash_init();

    sysInit();
}


// ******** local function implementations ********
static void sysInit()
{
	// setup our debug serial console
	cxa_esp32_usart_init_noHH(&usart_debug, UART_NUM_0, 115200);
	cxa_assert_setIoStream(cxa_usart_getIoStream(&usart_debug.super));

	// setup our timebase
	cxa_esp32_timeBase_init();

	// setup our console and logger
	cxa_console_init("ovrBeacon Gateway", cxa_usart_getIoStream(&usart_debug.super));
	cxa_logger_setGlobalIoStream(cxa_usart_getIoStream(&usart_debug.super));

	// setup our networking
	cxa_network_wifiManager_init();
	cxa_mqtt_connManager_init_clientCert(NULL,
										 MQTT_SERVER, MQTT_PORT,
										 tls_server_root_cert, sizeof(tls_server_root_cert),
										 tls_client_cert, sizeof(tls_client_cert),
										 tls_client_key_private, sizeof(tls_client_key_private));
	cxa_mqtt_rpc_node_root_init(&rpcNode_root, cxa_mqtt_connManager_getMqttClient(), true, cxa_uniqueId_getHexString());

	// setup our application-specific peripherals
	cxa_esp32_gpio_init_output(&gpio_btleReset, GPIO_NUM_4, CXA_GPIO_POLARITY_INVERTED, 1);
	cxa_esp32_usart_init_noHH(&usart_btle, UART_NUM_1, 57600);
	cxa_blueGiga_btle_client_init(&btleClient, cxa_usart_getIoStream(&usart_btle.super), &gpio_btleReset.super);
	ovr_beaconManager_init(&beaconManager, &btleClient.super, &rpcNode_root.super);

	cxa_console_addCommand("noti", "notify", NULL, 0, consoleCb_noti, NULL);

	// schedule our user task for execution
	xTaskCreate(userTask, (const char * const)"usrTask", 4096, NULL, tskIDLE_PRIORITY, NULL);
}


static void userTask(void *pvParameters)
{
	cxa_network_wifiManager_startNormal();

	// does not return
	cxa_runLoop_execute();
}


static void consoleCb_noti(cxa_array_t *const argsIn, cxa_ioStream_t *const ioStreamIn, void* userVarIn)
{
	char notiPayload[] = "{ \"beaconUuid\":\"acfc6376-c217-4a67-ada1-5a674473b291\", \"temp_c\":20, \"rssi\":-60, \"batt_pcnt\":80 }";

	cxa_mqtt_rpc_node_publishNotification(&rpcNode_root.super, "ovrBeaconUpdate", CXA_MQTT_QOS_ATMOST_ONCE, (void*)notiPayload, strlen(notiPayload));
}
