#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "nvs_flash.h"

#include "wpa2/utils/base64.h"

#include "driver/gpio.h"
#include "sdkconfig.h"

#define LED_PIN 2

#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

#define WEB_SERVER CONFIG_WEBAPI_SERVER
#define WEB_PORT CONFIG_WEBAPI_PORT
#define WEB_URL CONFIG_WEBAPI_URL
#define WEB_AUTH CONFIG_WEBAPI_AUTH

extern uint8_t temprature_sens_read();

static void led_blink() {
  gpio_set_level(LED_PIN, 1);
  vTaskDelay(10 / portTICK_PERIOD_MS);
  gpio_set_level(LED_PIN, 0);
}

static void led_pin_init() {
  gpio_pad_select_gpio(LED_PIN);
  gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
}

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG = "HTTPS";
static const char *REQUEST_HEADER =
    "POST " WEB_URL " HTTP/1.0\r\n"
    "Host: " WEB_SERVER "\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n" ;

extern const uint8_t
    server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");

extern const uint8_t
    server_root_cert_pem_end[] asm("_binary_server_root_cert_pem_end");

static esp_err_t event_handler(void *ctx, system_event_t *event) {
  switch (event->event_id) {
  case SYSTEM_EVENT_STA_START:
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    /* This is a workaround as ESP32 WiFi libs don't currently
       auto-reassociate. */
    esp_wifi_connect();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void initialise_wifi(void) {
  tcpip_adapter_init();
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = EXAMPLE_WIFI_SSID,
              .password = EXAMPLE_WIFI_PASS,
          },
  };
  ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

static void communicate(struct esp_tls *tls, char *request_body) {
  char buf[1024];
  int ret, len;
  size_t dummy;

  sprintf(buf, "%sContent-Length:%d\r\nAuthorization:Basic %s\r\n%s",
    REQUEST_HEADER,
    strlen(request_body),
    base64_encode((const unsigned char*)WEB_AUTH, 
        strlen(WEB_AUTH), &dummy),
    request_body);

  // ESP_LOGI(TAG, "request=%s", buf);
  ESP_LOGI(TAG, "request_body=%s", request_body);
  /* TLS write */
  size_t written_bytes = 0;
  do {
    ret = esp_tls_conn_write(tls, buf + written_bytes,
                             strlen(buf) - written_bytes);
    if (ret >= 0) {
      ESP_LOGI(TAG, "%d bytes written", ret);
      written_bytes += ret;
    } else if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
               ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
      return;
    }
  } while (written_bytes < strlen(buf));

  /* TLS read */
  ESP_LOGI(TAG, "Reading HTTP response...");

  do {
    len = sizeof(buf) - 1;
    bzero(buf, sizeof(buf));
    ret = esp_tls_conn_read(tls, (char *)buf, len);

    if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_WANT_READ) {
      continue;
    }

    if (ret < 0) {
      ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
      break;
    }

    if (ret == 0) {
      ESP_LOGI(TAG, "connection closed");
      break;
    }

    len = ret;
    ESP_LOGD(TAG, "%d bytes read", len);

    for (int i = 0; i < len; i++) {
      putchar(buf[i]);
    }
  } while (1);
}

static double measure_chip_temp() {
  return ((double)temprature_sens_read() - 32.0) / 1.8;
}

static void build_request_body(char *buf) {
  sprintf(buf, "{\"esp32\":{\"temperature\":%f}}", measure_chip_temp());
}

static void https_request(void *ignore) {

  char request_body[256];

  /* Wait for the callback to set the CONNECTED_BIT in the
     event group.
   */
  xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true,
                      portMAX_DELAY);
  ESP_LOGI(TAG, "Connected to AP");
  esp_tls_cfg_t cfg = {
      .cacert_pem_buf = server_root_cert_pem_start,
      .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
  };

  /* TLS connection */
  struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, &cfg);

  if (tls != NULL) {
    ESP_LOGI(TAG, "Connection established...");
    build_request_body(request_body);
    communicate(tls, request_body);
  } else {
    ESP_LOGE(TAG, "Connection failed...");
  }
  esp_tls_conn_delete(tls);
}

static void measure_loop(void *pvParameters) {
  https_request(pvParameters);
  led_blink();
  vTaskDelay(60000 / portTICK_PERIOD_MS);
}

static void task_measure_loop(void *pvParameters) {
  while (true) {
    measure_loop(pvParameters);
  }
  vTaskDelete(NULL);
}

void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());

  led_pin_init();
  led_blink();

  initialise_wifi();

  xTaskCreate(&task_measure_loop, "measure_loop", 1024 * 16, NULL, 6, NULL);
}
