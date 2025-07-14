#include "hardware/structs/rosc.h"
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#define MQTT_SERVER_HOST "broker.hivemq.com"
#define MQTT_SERVER_PORT 1883
#define MQTT_TLS 0
#define BUFFER_SIZE 256

#define LED_PIN_R 13
#define LED_PIN_G 11
#define LED_PIN_B 12

#define PIN_STATUS 15
#define PIN_CONTROL 16

typedef struct MQTT_CLIENT_T_ {
    ip_addr_t remote_addr;
    mqtt_client_t *mqtt_client;
    u32_t counter;
} MQTT_CLIENT_T;

MQTT_CLIENT_T* mqtt_client_init(void) {
    MQTT_CLIENT_T *state = calloc(1, sizeof(MQTT_CLIENT_T));
    return state;
}

void dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    MQTT_CLIENT_T *state = (MQTT_CLIENT_T*)callback_arg;
    if (ipaddr) {
        state->remote_addr = *ipaddr;
        printf("DNS resolved: %s\n", ip4addr_ntoa(ipaddr));
    } else {
        printf("DNS resolution failed.\n");
    }
}

void run_dns_lookup(MQTT_CLIENT_T *state) {
    printf("Resolving %s...\n", MQTT_SERVER_HOST);
    if (dns_gethostbyname(MQTT_SERVER_HOST, &(state->remote_addr), dns_found, state) == ERR_INPROGRESS) {
        while (state->remote_addr.addr == 0) {
            cyw43_arch_poll();
            sleep_ms(10);
        }
    }
}

void mqtt_incoming_cb(void *arg, const char *topic, u32_t tot_len) {
    printf("Mensagem recebida no tópico: %s\n", topic);
}

void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    MQTT_CLIENT_T *state = (MQTT_CLIENT_T *)arg;
    char msg[len + 1];
    memcpy(msg, data, len);
    msg[len] = '\0';

    printf("Comando recebido: %s\n", msg);

    char confirm_msg[BUFFER_SIZE];

    if (strcmp(msg, "R_ON") == 0) {
        gpio_put(LED_PIN_R, 1);
        snprintf(confirm_msg, BUFFER_SIZE, "{\"led\":\"R\",\"state\":\"on\"}");
    } else if (strcmp(msg, "R_OFF") == 0) {
        gpio_put(LED_PIN_R, 0);
        snprintf(confirm_msg, BUFFER_SIZE, "{\"led\":\"R\",\"state\":\"off\"}");
    } else if (strcmp(msg, "G_ON") == 0) {
        gpio_put(LED_PIN_G, 1);
        snprintf(confirm_msg, BUFFER_SIZE, "{\"led\":\"G\",\"state\":\"on\"}");
    } else if (strcmp(msg, "G_OFF") == 0) {
        gpio_put(LED_PIN_G, 0);
        snprintf(confirm_msg, BUFFER_SIZE, "{\"led\":\"G\",\"state\":\"off\"}");
    } else if (strcmp(msg, "B_ON") == 0) {
        gpio_put(LED_PIN_B, 1);
        snprintf(confirm_msg, BUFFER_SIZE, "{\"led\":\"B\",\"state\":\"on\"}");
    } else if (strcmp(msg, "B_OFF") == 0) {
        gpio_put(LED_PIN_B, 0);
        snprintf(confirm_msg, BUFFER_SIZE, "{\"led\":\"B\",\"state\":\"off\"}");
    } else if (strcmp(msg, "PIN_ON") == 0) {
        gpio_put(PIN_CONTROL, 1);
        snprintf(confirm_msg, BUFFER_SIZE, "{\"pin\":\"control\",\"state\":\"on\"}");
    } else if (strcmp(msg, "PIN_OFF") == 0) {
        gpio_put(PIN_CONTROL, 0);
        snprintf(confirm_msg, BUFFER_SIZE, "{\"pin\":\"control\",\"state\":\"off\"}");
    } else {
        printf("Comando inválido recebido: %s\n", msg);
        return;
    }

    // Publicar confirmação
    mqtt_publish(state->mqtt_client, "bitdoglab/confirm", confirm_msg, strlen(confirm_msg), 0, 0, NULL, NULL);
    printf("Confirmação enviada: %s\n", confirm_msg);
}

void mqtt_pub_cb(void *arg, err_t err) {
    printf("Publish status: %d\n", err);
}

void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        gpio_put(LED_PIN_G, 1); 
        printf("MQTT connected.\n");

        mqtt_set_inpub_callback(client, mqtt_incoming_cb, mqtt_incoming_data_cb, arg);
        mqtt_subscribe(client, "bitdoglab/control", 0, NULL, NULL);
    } else {
        gpio_put(LED_PIN_R, 1); 
        printf("MQTT connection failed: %d\n", status);
    }
}

err_t mqtt_publish_status(MQTT_CLIENT_T *state, int pin_value) {
    char msg[BUFFER_SIZE];
    snprintf(msg, BUFFER_SIZE, "{\"status\":%d}", pin_value);
    return mqtt_publish(state->mqtt_client, "bitdoglab/status", msg, strlen(msg), 0, 0, mqtt_pub_cb, NULL);
}

err_t mqtt_connect_to_broker(MQTT_CLIENT_T *state) {
    struct mqtt_connect_client_info_t ci = {0};
    ci.client_id = "bitdoglab-monitor";
    return mqtt_client_connect(state->mqtt_client, &state->remote_addr, MQTT_SERVER_PORT, mqtt_connection_cb, state, &ci);
}

void mqtt_run_loop(MQTT_CLIENT_T *state) {
    state->mqtt_client = mqtt_client_new();
    if (!state->mqtt_client) {
        printf("Failed to create MQTT client\n");
        return;
    }

    if (mqtt_connect_to_broker(state) != ERR_OK) {
        printf("MQTT connect failed.\n");
        return;
    }

    while (true) {
        cyw43_arch_poll();

        if (mqtt_client_is_connected(state->mqtt_client)) {
            int pin_value = gpio_get(PIN_CONTROL);
            mqtt_publish_status(state, pin_value);
            printf("Status do pino %d: %d\n", PIN_CONTROL, pin_value);
            sleep_ms(1000);
        } else {
            printf("MQTT desconectado. Reconectando...\n");
            sleep_ms(2000);
            mqtt_connect_to_broker(state);
        }
    }
}

int main() {
    stdio_init_all();

    gpio_init(LED_PIN_R); gpio_set_dir(LED_PIN_R, GPIO_OUT); gpio_put(LED_PIN_R, 0);
    gpio_init(LED_PIN_G); gpio_set_dir(LED_PIN_G, GPIO_OUT); gpio_put(LED_PIN_G, 0);
    gpio_init(LED_PIN_B); gpio_set_dir(LED_PIN_B, GPIO_OUT); gpio_put(LED_PIN_B, 0);

    gpio_init(PIN_CONTROL); gpio_set_dir(PIN_CONTROL, GPIO_OUT); gpio_put(PIN_CONTROL, 0);

    gpio_init(PIN_STATUS);
    gpio_set_dir(PIN_STATUS, GPIO_IN);
    gpio_pull_up(PIN_STATUS);  

    if (cyw43_arch_init()) {
        printf("Failed to init WiFi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("WiFi connection failed\n");
        return 1;
    }

    printf("Connected to WiFi.\n");

    MQTT_CLIENT_T *state = mqtt_client_init();
    run_dns_lookup(state);
    mqtt_run_loop(state);

    cyw43_arch_deinit();
    return 0;
}
