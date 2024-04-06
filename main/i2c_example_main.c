
#include<string.h>

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"

#include "ssd1306.h"
#include "font8x8_basic.h"

static const char *TAG = "i2c-ssd1306";

#define _I2C_NUMBER(num) I2C_NUM_##num
#define I2C_NUMBER(num) _I2C_NUMBER(num)

#define DATA_LENGTH 512                  /*!< Data buffer length of test buffer */
#define RW_TEST_LENGTH 128               /*!< Data length for r/w test, [0,DATA_LENGTH] */
#define DELAY_TIME_BETWEEN_ITEMS_MS 1000 /*!< delay time between different test items */

#define I2C_MASTER_SCL_IO 4               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 5               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUMBER(CONFIG_I2C_MASTER_PORT_NUM) /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_MASTER_FREQUENCY        /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */

SemaphoreHandle_t print_mux = NULL;


/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}


void ssd1306_init() {
	esp_err_t espRc;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

	i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true);
	i2c_master_write_byte(cmd, 0x14, true);

	i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP, true); // reverse left-right mapping
	i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE, true); // reverse up-bottom mapping

	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_NORMAL, true);
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_OFF, true);
	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (espRc == ESP_OK) {
		ESP_LOGI(TAG, "OLED configured successfully");
	} else {
		ESP_LOGE(TAG, "OLED configuration failed. code: 0x%.2X", espRc);
	}
	i2c_cmd_link_delete(cmd);
}

void task_ssd1306_display_text(const void *arg_text) {
	char *text = (char*)arg_text;
	uint8_t text_len = strlen(text);

	i2c_cmd_handle_t cmd;

	uint8_t cur_page = 0;

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	i2c_master_write_byte(cmd, 0x00, true); // reset column - choose column --> 0
	i2c_master_write_byte(cmd, 0x10, true); // reset line - choose line --> 0
	i2c_master_write_byte(cmd, 0xB0 | cur_page, true); // reset page

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	for (uint8_t i = 0; i < text_len; i++) {
		if (text[i] == '\n') {
			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

			i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
			i2c_master_write_byte(cmd, 0x00, true); // reset column
			i2c_master_write_byte(cmd, 0x10, true);
			i2c_master_write_byte(cmd, 0xB0 | ++cur_page, true); // increment page

			i2c_master_stop(cmd);
			i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
		} else {
			cmd = i2c_cmd_link_create();
			i2c_master_start(cmd);
			i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

			i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
			i2c_master_write(cmd, font8x8_basic_tr[(uint8_t)text[i]], 8, true);

			i2c_master_stop(cmd);
			i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
			i2c_cmd_link_delete(cmd);
		}
	}

	vTaskDelete(NULL);
}

void task_ssd1306_display_clear(void *ignore) {
	i2c_cmd_handle_t cmd;

	uint8_t clear[128];
	for (uint8_t i = 0; i < 128; i++) {
		clear[i] = 0;
	}
	for (uint8_t i = 0; i < 8; i++) {
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
		i2c_master_write_byte(cmd, 0xB0 | i, true);

		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
		i2c_master_write(cmd, clear, 128, true);
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);
	}

	vTaskDelete(NULL);
}

void task_ssd1306_display_image(uint8_t *logo){
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	for(int i = 0; i < 8; i++){
		
		uint8_t segs[128];
		for (int j = 0; j < 128; j++) {
			segs[j] = logo[j*8 + i];
		}

		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
		
		i2c_master_write_byte(cmd, 0xB0 | (uint8_t) i, true);

		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
		i2c_master_write(cmd, segs, 128, true);
		
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);

	}

	vTaskDelete(NULL);
}

static uint8_t logo[1024] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc7, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x01, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x07, 0x87, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x3f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x78, 0x7c, 0x1f, 0xff, 0xff, 0xf0, 0x00, 0x1f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x07, 0xc7, 0xc3, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x1f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x3c, 0x3c, 0x3f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0x80, 0x01, 0xe3, 0xc3, 0xff, 0xc0, 0x00, 0x07, 0xf0, 0x00, 0x01, 0xff, 
	0xff, 0xff, 0xff, 0xfe, 0x00, 0x07, 0x1e, 0x1f, 0xfc, 0x00, 0x00, 0xff, 0xf8, 0x00, 0x00, 0x7f, 
	0xff, 0xff, 0xff, 0xf8, 0x00, 0x38, 0xf1, 0xff, 0xc0, 0x00, 0xfe, 0x7f, 0xfc, 0x00, 0x78, 0x1f, 
	0xff, 0xff, 0xff, 0xe0, 0x01, 0xc7, 0x8f, 0xfc, 0x00, 0x0f, 0xff, 0x1f, 0xff, 0x83, 0xfe, 0x0f, 
	0xff, 0xff, 0xff, 0xc0, 0x07, 0x1c, 0x7f, 0xe0, 0x00, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff, 0x87, 
	0xff, 0xff, 0xff, 0x80, 0x38, 0xe1, 0xff, 0x00, 0x0f, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xc3, 
	0xff, 0xff, 0xfe, 0x00, 0xe3, 0x8f, 0xf8, 0x00, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xe3, 
	0xff, 0xff, 0xfc, 0x03, 0x9c, 0x7f, 0xc0, 0x07, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xe1, 
	0xff, 0xff, 0xf8, 0x0c, 0x71, 0xfe, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xf1, 
	0xff, 0xff, 0xf0, 0x31, 0xc7, 0xf8, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xf1, 
	0xff, 0xff, 0xe0, 0xc7, 0x3f, 0xc0, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xf9, 
	0xff, 0xff, 0xe3, 0x1c, 0xff, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xf9, 
	0xff, 0xff, 0xcc, 0x73, 0xf8, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xf9, 
	0xff, 0xff, 0xf9, 0xcf, 0xe0, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xf9, 
	0xff, 0xff, 0xe7, 0x3f, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xf3, 
	0xff, 0xff, 0x8c, 0xfe, 0x00, 0x7f, 0x9f, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xf3, 
	0xff, 0xff, 0x31, 0xf8, 0x01, 0xff, 0x03, 0xff, 0x01, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xf7, 
	0xff, 0xff, 0xe7, 0xe0, 0x07, 0xfc, 0x00, 0xfc, 0x00, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xe7, 
	0xff, 0xff, 0x9f, 0xc0, 0x1f, 0xf8, 0x01, 0xfe, 0x00, 0x7f, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xef, 
	0xff, 0xfe, 0x3f, 0x00, 0x7f, 0xf0, 0x07, 0x83, 0x80, 0x3f, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xdf, 
	0xff, 0xfe, 0xfc, 0x00, 0xff, 0xe0, 0x1e, 0x00, 0xe0, 0x1f, 0xff, 0xff, 0xfd, 0xff, 0xff, 0x9f, 
	0xff, 0xff, 0xf8, 0x03, 0xff, 0xc0, 0x38, 0x00, 0x78, 0x0f, 0xff, 0xff, 0xfd, 0xff, 0xff, 0x3f, 
	0xff, 0xff, 0xe0, 0x07, 0xff, 0xc0, 0xe0, 0x00, 0x1c, 0x07, 0xff, 0xff, 0xfd, 0xff, 0xfe, 0x7f, 
	0xff, 0xff, 0xc0, 0x1f, 0xff, 0x81, 0xc0, 0x00, 0x0e, 0x07, 0xff, 0xff, 0xfd, 0xff, 0xfe, 0xff, 
	0xff, 0xff, 0x00, 0x3f, 0xff, 0x83, 0x80, 0x00, 0x03, 0x83, 0xff, 0xff, 0xfd, 0xff, 0xfd, 0xff, 
	0xff, 0xfe, 0x00, 0x7f, 0xff, 0x86, 0x00, 0x00, 0x01, 0xc3, 0xff, 0xff, 0xf9, 0xff, 0xfb, 0xff, 
	0xff, 0xfc, 0x01, 0xff, 0xff, 0xff, 0x80, 0x00, 0x03, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xf7, 0xff, 
	0xff, 0xf0, 0x03, 0xf8, 0x07, 0x3f, 0xfc, 0x00, 0x7f, 0xf1, 0x80, 0x7f, 0xf9, 0xff, 0xff, 0xff, 
	0xff, 0xe0, 0x07, 0xf8, 0x07, 0x30, 0x1f, 0xc7, 0xe0, 0x39, 0x80, 0x7f, 0xfb, 0xff, 0xff, 0xff, 
	0xff, 0xc0, 0x0f, 0xf8, 0x06, 0x70, 0x01, 0xfe, 0x00, 0x19, 0x80, 0x3f, 0xf3, 0xff, 0xff, 0xff, 
	0xff, 0x80, 0x1f, 0xf8, 0x06, 0xe0, 0x01, 0xff, 0x00, 0x0d, 0x80, 0x7f, 0xf7, 0xff, 0xff, 0xff, 
	0xff, 0x00, 0x3f, 0xf8, 0x07, 0xc0, 0x0f, 0xff, 0xc0, 0x0f, 0x80, 0x7f, 0xe7, 0xff, 0xff, 0xff, 
	0xfe, 0x00, 0x7f, 0xfc, 0x07, 0x80, 0x3f, 0xff, 0xf8, 0x07, 0x80, 0xff, 0xef, 0xff, 0xff, 0xff, 
	0xfc, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff, 
	0xf8, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xff, 
	0xf8, 0x01, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xff, 0xfe, 0x00, 0x1f, 0xff, 0x3f, 0xff, 0xff, 0xff, 
	0xf0, 0x01, 0xff, 0xff, 0xf8, 0x00, 0x7f, 0xff, 0xf8, 0x00, 0x7f, 0xfe, 0x7f, 0xff, 0xff, 0xff, 
	0xe0, 0x03, 0xff, 0xff, 0xfe, 0x00, 0x3f, 0xff, 0xf0, 0x01, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 
	0xe0, 0x03, 0xff, 0x00, 0x3f, 0xc0, 0x1f, 0xff, 0xe0, 0x07, 0xf0, 0x01, 0xff, 0xff, 0xff, 0xff, 
	0xc0, 0x03, 0xff, 0x80, 0x0f, 0xf0, 0x07, 0xff, 0xc0, 0x1f, 0xc0, 0x03, 0xff, 0xff, 0xff, 0xff, 
	0xc0, 0x03, 0xff, 0xc0, 0x03, 0xfc, 0x03, 0xff, 0x00, 0xff, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 
	0xc0, 0x03, 0xff, 0xf0, 0x00, 0xff, 0x01, 0xfe, 0x03, 0xfc, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 
	0x80, 0x03, 0xff, 0xf8, 0x00, 0x0f, 0xe0, 0xfc, 0x0f, 0xe0, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 
	0x80, 0x01, 0xff, 0xfe, 0x00, 0x00, 0xf8, 0x38, 0x3e, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0x80, 0x01, 0xff, 0xff, 0x80, 0x00, 0x00, 0x10, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0x80, 0x00, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xc0, 0x00, 0x7f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xc0, 0x00, 0x3f, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xc0, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xe0, 0x00, 0x07, 0xff, 0xff, 0xff, 0xfe, 0x03, 0xc1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xf0, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xf8, 0x00, 0x00, 0x07, 0xff, 0xff, 0xfc, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xfc, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xe0, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};



void app_main(void)
{	
	ESP_ERROR_CHECK(i2c_master_init());
	ssd1306_init();
	xTaskCreate(&task_ssd1306_display_clear, "ssd1306_display_clear",  2048, NULL, 6, NULL);
	vTaskDelay(100/portTICK_PERIOD_MS);
	
	// xTaskCreate(&task_ssd1306_display_text, "ssd1306_display_text",  2048, (void *)"19522031\n19522509", 6, NULL);
	// vTaskDelay(5000/portTICK_PERIOD_MS);
	// xTaskCreate(&task_ssd1306_display_clear, "ssd1306_display_clear",  2048, NULL, 6, NULL);
	
	xTaskCreate(&task_ssd1306_display_image, "ssd1306_display_image",  2048, logo, 6, NULL);

}