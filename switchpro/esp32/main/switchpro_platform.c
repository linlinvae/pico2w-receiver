#include <string.h>
#include <driver/uart.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <uni.h>
#include "bt/uni_bt_allowlist.h"
#include "sdkconfig.h"
#include "sp_link.h"
#include "sp_endpoint.h"
#define BRIDGE_UART UART_NUM_1
static uni_hid_device_t *controller;
static btstack_timer_source_t timer;
static sp_decoder_t decoder;
static sp_endpoint_t endpoint;
static sp_input_t input;
static uint16_t status_seq;
static uint32_t last_status, last_input;
static bool have_input, forgetting;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void send_frame(const sp_frame_t *f) {
    uint8_t wire[24]; sp_encode(f, wire);
    /* TX uses its own driver ring; console remains on UART0. */
    uart_write_bytes(BRIDGE_UART, wire, sizeof(wire));
}
static void send_status(void) {
    sp_frame_t f = {.type = SP_INPUT, .seq = ++status_seq};
    sp_pack_input(&input, f.data); send_frame(&f);
}
static void rumble(uint8_t strong, uint8_t weak) {
    if (controller && controller->report_parser.play_dual_rumble)
        controller->report_parser.play_dual_rumble(controller, 0, (strong || weak) ? 200 : 0, weak, strong);
}
static void begin_forget(void) {
    forgetting = true; rumble(0, 0); have_input = false;
    memset(&input, 0, sizeof(input)); input.battery = 255; send_status();
    uni_bt_allow_incoming_connections(false);
    uni_bt_stop_scanning_unsafe();
    /* Clear keys only after the radio is off: late pairing cannot recreate them. */
    hci_power_control(HCI_POWER_OFF);
}
static void service(btstack_timer_source_t *ts) {
    uint32_t now = now_ms();
    uint8_t bytes[64];
    int n = uart_read_bytes(BRIDGE_UART, bytes, sizeof(bytes), 0);
    for (int i = 0; i < n; ++i) {
        sp_frame_t f; uint8_t operation;
        if (!sp_decode_byte(&decoder, bytes[i], &f) || forgetting ||
            !sp_endpoint_accept(&endpoint, &f, now, &operation)) continue;
        if (have_input && input.connected) rumble(endpoint.left, endpoint.right);
        else rumble(0, 0);
        sp_frame_t ack = {.type = SP_ACK, .seq = f.seq}; send_frame(&ack);
        if (operation == SP_OP_FORGET) begin_forget();
        else if (operation == SP_OP_SCAN && !controller) uni_bt_start_scanning_and_autoconnect_unsafe();
    }
    if (sp_endpoint_expire(&endpoint, now)) rumble(0, 0);
    if (have_input && (uint32_t)(now - last_input) >= SP_LINK_TIMEOUT_MS) {
        have_input = false; memset(&input, 0, sizeof(input)); input.battery = 255;
        rumble(0, 0);
    }
    if ((uint32_t)(now - last_status) >= 20) { last_status = now; send_status(); }
    if (forgetting && hci_get_state() == HCI_STATE_OFF) {
        uni_bt_del_keys_unsafe();
        uni_bt_allowlist_remove_all(); uni_bt_allowlist_set_enabled(false);
        /* ACK and offline state must leave UART before reboot. */
        uart_wait_tx_done(BRIDGE_UART, pdMS_TO_TICKS(100));
        esp_restart();
    }
    btstack_run_loop_set_timer(ts, 10); btstack_run_loop_add_timer(ts);
}
static void init(int argc, const char **argv) {
    (void)argc; (void)argv;
    uart_config_t config = {.baud_rate = 115200, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    ESP_ERROR_CHECK(uart_param_config(BRIDGE_UART, &config));
    ESP_ERROR_CHECK(uart_set_pin(BRIDGE_UART, CONFIG_SP_UART_TX, CONFIG_SP_UART_RX,
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BRIDGE_UART, 512, 256, 0, NULL, 0));
    input.battery = 255;
    /* Bluepad32 already maps Switch face buttons to positional Xbox layout. */
    uni_gamepad_set_mappings_type(UNI_GAMEPAD_MAPPINGS_TYPE_XBOX);
}
static void init_complete(void) {
    uni_bt_allow_incoming_connections(true);
    uni_bt_start_scanning_and_autoconnect_unsafe();
    btstack_run_loop_set_timer_handler(&timer, service);
    btstack_run_loop_set_timer(&timer, 10); btstack_run_loop_add_timer(&timer);
}
static uni_error_t discovered(bd_addr_t addr, const char *name, uint16_t cod, uint8_t rssi) {
    (void)cod; (void)rssi;
    if (forgetting || controller || !uni_bt_allowlist_is_allowed_addr(addr)) return UNI_ERROR_IGNORE_DEVICE;
    /* No-name discovery can still be resolved by SDP; final acceptance checks type and VID/PID. */
    if (name && name[0] && strcmp(name, "Pro Controller")) return UNI_ERROR_IGNORE_DEVICE;
    return UNI_ERROR_SUCCESS;
}
static void connected(uni_hid_device_t *d) { (void)d; }
static uni_error_t ready(uni_hid_device_t *d) {
    if (forgetting || controller || uni_hid_device_is_virtual_device(d) ||
        d->controller_type != CONTROLLER_TYPE_SwitchProController ||
        d->conn.protocol != UNI_BT_CONN_PROTOCOL_BR_EDR ||
        d->vendor_id != 0x057e || d->product_id != 0x2009 ||
        !uni_bt_allowlist_is_allowed_addr(d->conn.btaddr)) return UNI_ERROR_INVALID_CONTROLLER;
    controller = d; have_input = false;
    endpoint.left = endpoint.right = 0;
    memset(&input, 0, sizeof(input)); input.battery = 255;
    if (!uni_bt_allowlist_is_enabled()) {
        uni_bt_allowlist_remove_all();
        if (uni_bt_allowlist_add_addr(d->conn.btaddr)) uni_bt_allowlist_set_enabled(true);
    }
    uni_bt_stop_scanning_unsafe();
    if (d->report_parser.set_player_leds) d->report_parser.set_player_leds(d, 1);
    rumble(0, 0); return UNI_ERROR_SUCCESS;
}
static void disconnected(uni_hid_device_t *d) {
    if (d != controller) return;
    controller = NULL; have_input = false;
    memset(&input, 0, sizeof(input)); input.battery = 255; send_status();
    endpoint.left = endpoint.right = 0;
    if (!forgetting) uni_bt_start_scanning_and_autoconnect_unsafe();
}
static void data(uni_hid_device_t *d, uni_controller_t *ctl) {
    if (d != controller || forgetting || ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) return;
    uni_gamepad_t *g = &ctl->gamepad;
    const int32_t axes[4] = {g->axis_x, g->axis_y, g->axis_rx, g->axis_ry};
    bool labels = false;
#ifdef CONFIG_SP_BUTTON_LABELS
    labels = true;
#endif
    sp_map_gamepad(g->buttons, g->dpad, g->misc_buttons, axes, ctl->battery, labels, &input);
    have_input = true; last_input = now_ms();
}
static void oob(uni_platform_oob_event_t event, void *payload) { (void)event; (void)payload; }
static const uni_property_t *property(uni_property_idx_t idx) { (void)idx; return NULL; }
struct uni_platform *switchpro_platform(void) {
    static struct uni_platform p = {.name = "SwitchPro-UART-R1", .init = init,
        .on_init_complete = init_complete, .on_device_discovered = discovered,
        .on_device_connected = connected, .on_device_disconnected = disconnected,
        .on_device_ready = ready, .on_controller_data = data,
        .on_oob_event = oob, .get_property = property};
    return &p;
}
