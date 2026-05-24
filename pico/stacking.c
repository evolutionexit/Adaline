// C/C++ libraries
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

// main hardware includes
#include "pico/stdlib.h"
// #include "pico/multicore.h"
#include "pico/cyw43_arch.h"

// secondary hardware includes
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

// wi-fi includes
#include "lwip/ip4_addr.h"

// protothreads and thread communication
#include "pt_cornell_rp2040_v1_1_2.h"

// uart
#include "hardware/uart.h"

// Wi-Fi setup
#include "wifi_config.h"

// Setup UART
#define UART_ID uart0
#define BAUD_RATE 115200

#define UART_TX_PIN 1
#define UART_RX_PIN 0

static struct udp_pcb *upcb;

static char udp_buf[256] = {0};

static bool wifi_connected = false;

static bool is_waiting_ack = false;

static struct pt pt_blink;
static struct pt pt_send;
static struct pt pt_recv;
static struct pt pt_tud;
static struct pt pt_hid;
static struct pt pt_wifi_connect;

static bool msg_received_hid = false;
static bool msg_received_udp = false;

static int hid_char_index = -1;  // -1 = idle, >= 0 = sending
static bool hid_key_down = false;


int wifi_connect_rc = -1;

enum  {
   BLINK_NOT_MOUNTED = 250,
   BLINK_MOUNTED = 1000,
   BLINK_SUSPENDED = 2500,
};
 
static uint32_t blink_interval_ms = 500;

//--------------------------------------------------------------------+
// CDC Printf Implementation
//--------------------------------------------------------------------+

static char wifi_status_buffer[512] = {0};
static int wifi_status_len = 0;

void log_printf(const char *fmt, ...) {
    static char buffer[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len <= 0) return;
    uart_write_blocking(UART_ID, (uint8_t*)buffer, len);
    if (wifi_status_len + len < sizeof(wifi_status_buffer) -1) {
        memcpy(wifi_status_buffer + wifi_status_len, buffer, len);
        wifi_status_len += len;
        wifi_status_buffer[wifi_status_len] = '\0';
    }

    if (tud_cdc_connected()) {
        tud_cdc_write(buffer, len);
        tud_cdc_write_flush();
    }

}

//--------------------------------------------------------------------+
// Device Callbacks
//--------------------------------------------------------------------+

void pico_set_led(bool led_on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
}

static int scan_result_cb(void *env, const cyw43_ev_scan_result_t *res) {
    if (!res) return 0; // end of scan
    const char *ssid = (const char *)res->ssid;
    log_printf("Found SSID: %s, channel: %d, rssi: %d, auth: %d\n",
        ssid, res->channel, res->rssi, res->auth_mode);
    return 0;
}

void udp_send_message(const char *ip, int port, const void * data, int data_size) {
    ip4_addr_t destAddr;
    ip4addr_aton(ip, &destAddr);
    struct pbuf *p = pbuf_alloc (PBUF_TRANSPORT, data_size, PBUF_RAM);
    if (!p) return;
    memcpy (p->payload, data, data_size);
    cyw43_arch_lwip_begin();
    udp_sendto (upcb, p, &destAddr, port);
    cyw43_arch_lwip_end();
    pbuf_free(p);
}


void tud_mount_cb(void) {
    blink_interval_ms = BLINK_MOUNTED;
}

void tud_umount_cb(void) {
    blink_interval_ms = BLINK_NOT_MOUNTED;
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

void tud_resume_cb(void) {
    blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// CDC Callbacks
//--------------------------------------------------------------------+

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void) itf;
    (void) rts;

    if (dtr) {
        // Host opened the port
        log_printf("\nCDC Terminal Connected!\n");

        // Print saved wifi connect result
        log_printf("wifi_connect_rc = %d\n", wifi_connect_rc);

        if (wifi_status_len > 0) {
            tud_cdc_write(wifi_status_buffer, wifi_status_len);
            tud_cdc_write_flush();
        }

        // Check if Wi-Fi is connected NOW (when CDC opens)
        cyw43_arch_lwip_begin();
        struct netif *netif_ptr = &cyw43_state.netif[CYW43_ITF_STA];
        if (netif_is_up(netif_ptr)) {
            ip4_addr_t ip = netif_ptr->ip_addr;
            log_printf("IP address: %s\n", ip4addr_ntoa(&ip));
        } else {
            log_printf("Network interface not up (Wi-Fi not connected yet)\n");
        }
        cyw43_arch_lwip_end();
    } else {
        // Host closed the port
        log_printf("CDC Terminal Disconnected\n");
    }
}

void tud_cdc_rx_cb(uint8_t itf) {
    (void) itf;
    // Handle incoming CDC data if needed
    // Read available data
    if (tud_cdc_available()) {
        char buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));
        // Echo back
        tud_cdc_write(buf, count);
        tud_cdc_write_flush();
    }
}

//--------------------------------------------------------------------+
// HID Functionality
//--------------------------------------------------------------------+


static void send_hid_report(uint8_t report_id, const char *udp_buf, int buf_size) {
    if (!tud_hid_ready()) return;

    switch(report_id)
    {
        case REPORT_ID_KEYBOARD:
        {
            
        }
        break;
        case REPORT_ID_MOUSE:
        {          
            // Mouse implementation if needed
        }
        break;
        case REPORT_ID_CONSUMER_CONTROL:
        {
            // Consumer control implementation if needed
        }
        break;
        case REPORT_ID_GAMEPAD:
        {
            // Gamepad implementation if needed
        }
        break;

        default: break;
    }
}

void udp_recv_message(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p->len >= 2) {
        // We only need the first 2 bytes: [Modifier, Keycode]
        memcpy(udp_buf, p->payload, 2);
        msg_received_hid = true;
    }
    pbuf_free(p);
}

void hid_task(void) {
    // interval_ms can be much lower now (e.g., 1ms or 2ms)
    const uint32_t interval_ms = 2; 
    static uint32_t last_report_ms = 0;
    static bool is_pressed = false;

    if (board_millis() - last_report_ms < interval_ms) return;
    if (!tud_hid_ready()) return;

    if (msg_received_hid && !is_pressed) {
        // STEP 1: Send the Press
        uint8_t modifier = (uint8_t)udp_buf[0];
        uint8_t keycodes[6] = { (uint8_t)udp_buf[1], 0, 0, 0, 0, 0 };
        
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycodes);
        
        is_pressed = true;
        last_report_ms = board_millis();
        msg_received_hid = false; // We consumed the UDP trigger
    } 
    else if (is_pressed) {
        // STEP 2: Send the Release (All Zeros)
        uint8_t keycodes[6] = { 0 };
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycodes);
        
        is_pressed = false;
        last_report_ms = board_millis();
        log_printf("Sending ACK...\n");
        is_waiting_ack = true;
    }
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len) {
    if (is_waiting_ack) {
        is_waiting_ack = false;
        const char *msg = "ACK\n";
        udp_send_message(RECEIVER_IP, RECEIVER_PORT, msg, strlen(msg));
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    //
}

//--------------------------------------------------------------------+
// Protothreads
//--------------------------------------------------------------------+

static PT_THREAD (protothread_udp_send(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        PT_WAIT_UNTIL(pt, msg_received_udp);
        const char *msg = "Hello from Pico W!\n";
        udp_send_message(RECEIVER_IP, RECEIVER_PORT, msg, strlen(msg));
        log_printf("UDP message sent to:\nRCEIEVER_IP:%s \nRECEIVER_PORT:%d\n", RECEIVER_IP, RECEIVER_PORT);
        msg_received_udp = false;
        PT_YIELD(pt);
    }
    PT_END(pt);
}

static PT_THREAD (protothread_udp_recv(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        cyw43_arch_poll();
        PT_YIELD(pt);
    }
    PT_END(pt);
}

static PT_THREAD (protothread_blink(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        pico_set_led(true);
        PT_YIELD_usec(blink_interval_ms * 1000);  // 1s
        pico_set_led(false);
        PT_YIELD_usec(blink_interval_ms * 1000);
    }
    PT_END(pt);
}

static PT_THREAD (protothread_tud(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        tud_task();
        PT_YIELD_usec(100);  // 1 ms
    }
    PT_END(pt);
}

static PT_THREAD (protothread_hid(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        hid_task();
        PT_YIELD_usec(1000); // 1 ms
    }
    PT_END(pt);
}

static PT_THREAD (protothread_wifi_connect(struct pt *pt)) {
    PT_BEGIN(pt);

    static uint32_t start_time;
    start_time = board_millis();

    // Wait 2 seconds for USB to enumerate
    while ((board_millis() - start_time) <= 2000) {
        tud_task();
        PT_YIELD_usec(1000);
    }


    log_printf("\n\nStarting Wi-Fi connection...\n");
    cyw43_arch_enable_sta_mode();
    /* cyw43_wifi_scan_options_t scan_options = {0};
    cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result_cb); // scan all
    log_printf("Found %d networks\n", scan_options); */
    log_printf("Connecting to Wi-Fi...\n");
    wifi_connect_rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_MIXED_PSK, 30000);
    if (wifi_connect_rc != 0) {
        log_printf("Wi-Fi connect failed: %d\n", wifi_connect_rc);
        while(1) PT_YIELD(pt);
    }
    log_printf("Connected!\n");

    cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);

    /*while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) {

        int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        log_printf("Wi-Fi status: %d\n", status);

        cyw43_arch_poll();
        tud_task();
        PT_YIELD_usec(10000);
    }*/
    
    
    log_printf("Connected to Wi-Fi!\n");
    cyw43_arch_lwip_begin();
    struct netif *netif_ptr = &cyw43_state.netif[CYW43_ITF_STA];
    ip4_addr_t ip = netif_ptr->ip_addr;
    cyw43_arch_lwip_end();

    log_printf("IP address: %s\n", ip4addr_ntoa(&ip));

    // Setup UDP
    upcb = udp_new();
    if (upcb && udp_bind(upcb, IP_ADDR_ANY, OWN_PORT) == ERR_OK) {
        udp_recv(upcb, udp_recv_message, NULL);
        log_printf("UDP listening on port %d\n", OWN_PORT);
        wifi_connected = true;

        udp_send_message(
            RECEIVER_IP,
            RECEIVER_PORT,
            "TinyUSB HID+CDC Device Starting...\n",
            36
        );
    }


    while (1) PT_YIELD(pt);
    PT_END(pt);
}

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

int main()
{
    stdio_init_all();
    board_init();
    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    if (cyw43_arch_init()) return 1;

    tud_init(BOARD_TUD_RHPORT);
    if (board_init_after_tusb) board_init_after_tusb();

    log_printf("\n\n=== Pico W Starting ===\n");
    log_printf("Starting protothreads...\n");

    pt_add_thread(protothread_tud);          // USB FIRST
    pt_add_thread(protothread_blink);
    pt_add_thread(protothread_hid);
    pt_add_thread(protothread_wifi_connect);
    pt_add_thread(protothread_udp_recv);
    pt_add_thread(protothread_udp_send);

    pt_schedule_start;
    return 0;
}
