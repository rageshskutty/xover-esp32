/*
 * wifi_load.c - Wi-Fi load generator for the biquad benchmark.
 *
 * Compiled only when CONFIG_BENCH_WIFI_LOAD is set (sdkconfig.wifi). It
 * replaces app_main: bench_main.c is built with app_main renamed to
 * bench_app_main, so the measured code is identical to the plain build.
 *
 * What it does: starts a SoftAP, then a TCP server on port 5001. Every client
 * is read from and written to at full speed by tasks pinned to core 0, so the
 * radio, lwIP and the TCP stack are busy while the DSP kernel runs on core 1.
 * It waits for sustained traffic, starts the benchmark, and prints one
 * "WIFI,<ms>,<rx kB>,<tx kB>,<clients>" line per second into the same serial log.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"

void bench_app_main(void);

#define LOAD_PORT            5001
#define LOAD_CORE            0
#define LOAD_CHUNK_BYTES     1440
#define LOAD_TASK_STACK      6144
#define LOAD_TASK_PRIO       5
#define LOAD_MAX_CLIENTS     2
#define LOAD_AP_CHANNEL      6
#define LOAD_RCV_TIMEOUT_S   1
#define SAMPLER_STACK        3072
#define SAMPLER_PRIO         3
#define SAMPLE_PERIOD_MS     1000
#define WAIT_MIN_KB_PER_S    256u
#define WAIT_STABLE_SAMPLES  3

static const char *TAG = "wifi_load";

static volatile uint32_t s_rx_bytes;
static volatile uint32_t s_tx_bytes;
static volatile int s_clients;

typedef struct {
    int sock;
    TaskHandle_t rx_handle;
} conn_t;

static void tx_task(void *arg)
{
    const conn_t conn = *(const conn_t *)arg;
    uint8_t *buf = (uint8_t *)malloc(LOAD_CHUNK_BYTES);
    if (buf != NULL) {
        memset(buf, 0xA5, LOAD_CHUNK_BYTES);
        for (;;) {
            const int n = send(conn.sock, buf, LOAD_CHUNK_BYTES, 0);
            if (n <= 0) {
                break;
            }
            __atomic_fetch_add(&s_tx_bytes, (uint32_t)n, __ATOMIC_RELAXED);
        }
        free(buf);
    }
    xTaskNotifyGive(conn.rx_handle);
    vTaskDelete(NULL);
}

static void rx_task(void *arg)
{
    conn_t *conn = (conn_t *)arg;
    conn->rx_handle = xTaskGetCurrentTaskHandle();

    const struct timeval tv = { .tv_sec = LOAD_RCV_TIMEOUT_S, .tv_usec = 0 };
    setsockopt(conn->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    __atomic_fetch_add(&s_clients, 1, __ATOMIC_RELAXED);

    const BaseType_t tx_ok = xTaskCreatePinnedToCore(
        tx_task, "load_tx", LOAD_TASK_STACK, conn, LOAD_TASK_PRIO, NULL,
        LOAD_CORE);

    uint8_t *buf = (uint8_t *)malloc(LOAD_CHUNK_BYTES);
    if (tx_ok == pdPASS && buf != NULL) {
        for (;;) {
            const int n = recv(conn->sock, buf, LOAD_CHUNK_BYTES, 0);
            if (n > 0) {
                __atomic_fetch_add(&s_rx_bytes, (uint32_t)n, __ATOMIC_RELAXED);
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }
            break;
        }
    }
    free(buf);

    /* shutdown() makes a blocked send() in tx_task fail, so it exits. */
    shutdown(conn->sock, SHUT_RDWR);
    if (tx_ok == pdPASS) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3000));
    }
    close(conn->sock);
    __atomic_fetch_sub(&s_clients, 1, __ATOMIC_RELAXED);
    free(conn);
    vTaskDelete(NULL);
}

static void server_task(void *arg)
{
    (void)arg;
    const int ls = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (ls < 0) {
        ESP_LOGE(TAG, "socket() failed, errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    const int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LOAD_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) != 0
        || listen(ls, LOAD_MAX_CLIENTS) != 0) {
        ESP_LOGE(TAG, "bind/listen failed, errno %d", errno);
        close(ls);
        vTaskDelete(NULL);
        return;
    }

    for (;;) {
        const int sock = accept(ls, NULL, NULL);
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (s_clients >= LOAD_MAX_CLIENTS) {
            close(sock);
            continue;
        }
        conn_t *conn = (conn_t *)malloc(sizeof(*conn));
        if (conn == NULL) {
            close(sock);
            continue;
        }
        conn->sock = sock;
        conn->rx_handle = NULL;
        if (xTaskCreatePinnedToCore(rx_task, "load_rx", LOAD_TASK_STACK, conn,
                                    LOAD_TASK_PRIO, NULL, LOAD_CORE)
            != pdPASS) {
            close(sock);
            free(conn);
        }
    }
}

static esp_err_t wifi_start_ap(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES
        || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        return err;
    }
    if (esp_netif_create_default_wifi_ap() == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strlcpy((char *)cfg.ap.ssid, CONFIG_BENCH_WIFI_SSID, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len = (uint8_t)strlen(CONFIG_BENCH_WIFI_SSID);
    strlcpy((char *)cfg.ap.password, CONFIG_BENCH_WIFI_PASS,
            sizeof(cfg.ap.password));
    cfg.ap.channel = LOAD_AP_CHANNEL;
    cfg.ap.max_connection = LOAD_MAX_CLIENTS;
    cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }
    (void)esp_wifi_set_ps(WIFI_PS_NONE);
    return ESP_OK;
}

static void sample_rates(uint32_t *prev_rx, uint32_t *prev_tx,
                         uint32_t *rx_kb, uint32_t *tx_kb)
{
    const uint32_t rx = s_rx_bytes;
    const uint32_t tx = s_tx_bytes;
    *rx_kb = (rx - *prev_rx) / 1024u;
    *tx_kb = (tx - *prev_tx) / 1024u;
    *prev_rx = rx;
    *prev_tx = tx;
}

static void sampler_task(void *arg)
{
    (void)arg;
    uint32_t prev_rx = s_rx_bytes;
    uint32_t prev_tx = s_tx_bytes;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
        uint32_t rx_kb;
        uint32_t tx_kb;
        sample_rates(&prev_rx, &prev_tx, &rx_kb, &tx_kb);
        printf("WIFI,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%d\n",
               (uint32_t)(esp_timer_get_time() / 1000), rx_kb, tx_kb,
               (int)s_clients);
    }
}

void app_main(void)
{
    printf("\n=== Biquad Benchmark: Wi-Fi load build ===\n");

    const esp_err_t err = wifi_start_ap();
    if (err != ESP_OK) {
        printf("WIFI_STATUS,START_FAILED,%d\n", (int)err);
        printf("WIFI_TRAFFIC,NO_WIFI\n");
        bench_app_main();
        return;
    }

    xTaskCreatePinnedToCore(server_task, "load_srv", LOAD_TASK_STACK, NULL,
                            LOAD_TASK_PRIO, NULL, LOAD_CORE);
    printf("WIFI_STATUS,AP_UP,ssid=%s,ip=192.168.4.1,port=%d\n",
           CONFIG_BENCH_WIFI_SSID, LOAD_PORT);
    printf("WIFI_STATUS,WAITING (join the AP, run tools/biquad-bench/"
           "wifi_load_client.py, up to %d s)\n", CONFIG_BENCH_WIFI_WAIT_S);

    bool traffic = false;
    int stable = 0;
    uint32_t prev_rx = s_rx_bytes;
    uint32_t prev_tx = s_tx_bytes;
    for (int t = 0; t < CONFIG_BENCH_WIFI_WAIT_S && !traffic; t++) {
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
        uint32_t rx_kb;
        uint32_t tx_kb;
        sample_rates(&prev_rx, &prev_tx, &rx_kb, &tx_kb);
        stable = ((rx_kb + tx_kb) >= WAIT_MIN_KB_PER_S) ? stable + 1 : 0;
        printf("WIFI_WAIT,%d,%" PRIu32 ",%" PRIu32 ",%d\n", t + 1, rx_kb,
               tx_kb, (int)s_clients);
        traffic = (stable >= WAIT_STABLE_SAMPLES);
    }
    printf("WIFI_TRAFFIC,%s\n", traffic ? "YES" : "NO_CLIENT");

    xTaskCreatePinnedToCore(sampler_task, "wifi_smp", SAMPLER_STACK, NULL,
                            SAMPLER_PRIO, NULL, LOAD_CORE);
    bench_app_main();
}
