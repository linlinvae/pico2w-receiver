/* Exercise the actual BLE state machine with SDK-compatible mock calls.
 * This does not simulate the radio or the WCH library internals. */
#include "../src/stadia_ble.c"

volatile uint32_t g_sys_tick_ms;
static int s_fail, s_scan_calls, s_connect_calls, s_disconnect_calls, s_frees;
static bStatus_t s_scan_status, s_connect_status, s_disconnect_status, s_write_status;
static uint8_t s_alloc[16];
static bStatus_t s_addr_status;
static uint8_t s_addr_type;
static bool s_addr_is_null;
static int s_primary_calls, s_discovery_timers;
static bStatus_t s_primary_status;
static tmosTimer s_last_discovery_delay;
static peer_store_t mock_peer = {.loaded=true};
static bool mock_save_ok=true, mock_clear_ok=true;
static int peer_saves, peer_clears, bond_erases;
static uint8_t bond_count, pair_mode, bond_enabled, bond_auto, bond_fail_action;
static bStatus_t bond_erase_status, scan_cancel_status;
static uint16_t terminated_handle;

bool peer_store_load(peer_store_t *s) { *s=mock_peer; return s->loaded; }
bool peer_store_save(peer_store_t *s,uint8_t type,const uint8_t address[6])
{
    ++peer_saves;
    if (!mock_save_ok) return false;
    s->valid=true; s->address_type=type; memcpy(s->address,address,6); return true;
}
bool peer_store_clear(peer_store_t *s)
{ ++peer_clears; if (!mock_clear_ok) return false; s->valid=false; return true; }

static void check(const char *name, bool ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) s_fail = 1;
}

void log_write(uint8_t level, const char *mod, const char *fmt, ...) {}
void StadiaBLE_OnConnected(uint16_t handle) {}
void StadiaBLE_OnDisconnected(uint16_t handle) {}
BOOL tmos_memcmp(const void *a, const void *b, uint32_t n) { return memcmp(a,b,n) == 0; }
void tmos_memcpy(void *dst, const void *src, uint32_t n) { memcpy(dst,src,n); }
bStatus_t GAP_SetParamValue(uint16_t p, uint16_t v) { return SUCCESS; }
bStatus_t GAPBondMgr_SetParameter(uint16_t p, uint8_t n, void *v)
{
    if (p==GAPBOND_ERASE_ALLBONDS) { ++bond_erases; return bond_erase_status; }
    if (p==GAPBOND_CENT_PAIRING_MODE) pair_mode=*(uint8_t *)v;
    if (p==GAPBOND_CENT_BONDING_ENABLED) bond_enabled=*(uint8_t *)v;
    if (p==GAPBOND_BOND_AUTO) bond_auto=*(uint8_t *)v;
    if (p==GAPBOND_BOND_FAIL_ACTION) bond_fail_action=*(uint8_t *)v;
    return SUCCESS;
}
bStatus_t GAPBondMgr_GetParameter(uint16_t p, void *v) { *(uint8_t *)v=bond_count; return SUCCESS; }
bStatus_t GAPBondMgr_PasscodeRsp(uint16_t h, uint8_t s, uint32_t p) { return SUCCESS; }
bStatus_t GAPRole_GetParameter(uint16_t p, void *v)
{ memset(v,0,p == GAPROLE_BD_ADDR ? B_ADDR_LEN : 1); return SUCCESS; }
uint16_t GAP_GetParamValue(uint16_t p) { return 16; }
bStatus_t GAP_ConfigDeviceAddr(uint8_t t, uint8_t *a)
{ s_addr_type = t; s_addr_is_null = (a == NULL); return s_addr_status; }
bStatus_t GAPRole_CentralStartDiscovery(uint8_t m,uint8_t a,uint8_t w)
{ ++s_scan_calls; return s_scan_status; }
bStatus_t GAPRole_CentralCancelDiscovery(void) { return scan_cancel_status; }
bStatus_t GAPRole_CentralEstablishLink(uint8_t d,uint8_t w,uint8_t t,uint8_t *a)
{ ++s_connect_calls; return s_connect_status; }
bStatus_t GAPRole_TerminateLink(uint16_t h)
{ ++s_disconnect_calls; terminated_handle=h; return s_disconnect_status; }
bStatus_t GAPRole_CentralStartDevice(uint8_t t,gapBondCBs_t *b,gapCentralRoleCB_t *c) { return SUCCESS; }
bStatus_t GATT_InitClient(void) { return SUCCESS; }
void GATT_RegisterForInd(uint8_t t) {}
bStatus_t GATT_ExchangeMTU(uint16_t h,attExchangeMTUReq_t *r,uint8_t t) { return SUCCESS; }
bStatus_t GATT_DiscPrimaryServiceByUUID(uint16_t h,uint8_t *u,uint8_t n,uint8_t t)
{ ++s_primary_calls; return s_primary_status; }
bStatus_t GATT_DiscAllChars(uint16_t h,uint16_t a,uint16_t b,uint8_t t) { return SUCCESS; }
bStatus_t GATT_ReadCharValue(uint16_t h,attReadReq_t *r,uint8_t t) { return s_write_status; }
bStatus_t GATT_WriteCharValue(uint16_t h,attWriteReq_t *r,uint8_t t) { return s_write_status; }
bStatus_t GATT_WriteNoRsp(uint16_t h,attWriteReq_t *r) { return s_write_status; }
void *GATT_bm_alloc(uint16_t h,uint8_t o,uint16_t n,uint16_t *a,uint8_t f) { return s_alloc; }
void GATT_bm_free(gattMsg_t *m,uint8_t o) { ++s_frees; }
tmosTaskID TMOS_ProcessEventRegister(pTaskEventHandlerFn cb) { return 0; }
bStatus_t tmos_set_event(tmosTaskID t,tmosEvents e) { return SUCCESS; }
BOOL tmos_start_task(tmosTaskID t,tmosEvents e,tmosTimer n)
{ if (e == START_SVC_DISCOVERY_EVT) { ++s_discovery_timers; s_last_discovery_delay = n; } return TRUE; }
bStatus_t tmos_stop_task(tmosTaskID t,tmosEvents e) { return SUCCESS; }
uint8_t *tmos_msg_receive(tmosTaskID t) { return NULL; }
bStatus_t tmos_msg_deallocate(uint8_t *m) { return SUCCESS; }

static void reconnect_tests(void)
{
    gapRoleEvent_t e={0};
    s_state=BLE_STATE_IDLE; g_stadia_connected=false; s_forget_pending=false;
    s_save_peer_pending=false; s_rescan_requested=false; s_discovery_timeout_armed=false;
    s_auto_enabled=true; s_retry_pending=false; s_retry_delay=1000;
    mock_peer.valid=true; mock_peer.address_type=0;
    const uint8_t address[]={0x23,0x2e,0x29,0x04,0xe0,0xd8};
    memcpy(mock_peer.address,address,6);
    g_sys_tick_ms=100;
    StadiaBLE_Init();
    check("native Bond Manager initiates pairing and saves keys without forced security disconnect",
          pair_mode==GAPBOND_PAIRING_MODE_INITIATE&&bond_enabled&&bond_auto&&bond_fail_action==GAPBOND_FAIL_NO_ACTION);
    e.gap.opcode=GAP_DEVICE_INIT_DONE_EVENT; stadia_EventCB(&e);
    int scans=s_scan_calls;
    g_sys_tick_ms=349; StadiaBLE_Poll();
    check("startup reconnect waits for BLE ready and startup delay",s_scan_calls==scans);
    g_sys_tick_ms=350; StadiaBLE_Poll();
    check("remembered peer scans automatically after power-on",s_scan_calls==scans+1&&s_target_only);
    uint8_t named[]={7,9,'S','t','a','d','i','a'};
    memset(&e,0,sizeof(e)); e.deviceInfo.opcode=GAP_DEVICE_INFO_EVENT;
    e.deviceInfo.pEvtData=named; e.deviceInfo.dataLen=sizeof(named); e.deviceInfo.addr[0]=99;
    stadia_EventCB(&e);
    check("automatic scan cannot switch to a different named Stadia",!s_stadia_found);
    memcpy(e.deviceInfo.addr,address,6); e.deviceInfo.addrType=1; stadia_EventCB(&e);
    check("matching bytes with different address type cannot select saved peer",!s_stadia_found);
    e.deviceInfo.addrType=0; e.deviceInfo.dataLen=0; e.deviceInfo.pEvtData=NULL; stadia_EventCB(&e);
    check("known peer can reconnect without a local name",s_stadia_found);
    e.gap.opcode=GAP_DEVICE_DISCOVERY_EVENT; stadia_EventCB(&e);
    check("saved advertisement starts connection after scan completion",s_state==BLE_STATE_CONNECTING);
    int disconnects=s_disconnect_calls;
    g_sys_tick_ms=10349; StadiaBLE_Poll();
    check("pending connection is not cancelled early",s_disconnect_calls==disconnects);
    g_sys_tick_ms=10350; StadiaBLE_Poll();
    check("pending connection deadline cancels via SDK invalid handle",
          s_disconnect_calls==disconnects+1&&terminated_handle==INVALID_CONNHANDLE&&s_state==BLE_STATE_CONNECTING);
    StadiaBLE_Poll();
    check("accepted connection cancellation is not repeated",s_disconnect_calls==disconnects+1);
    e.gap.opcode=GAP_LINK_ESTABLISHED_EVENT; e.gap.hdr.status=bleTimeout; stadia_EventCB(&e);
    check("failed saved-peer connection schedules bounded retry",s_retry_pending&&s_retry_at==11350&&s_retry_delay==2000);
    g_sys_tick_ms=11350; StadiaBLE_Poll();
    e.gap.opcode=GAP_DEVICE_DISCOVERY_EVENT; stadia_EventCB(&e);
    check("missing saved peer increases retry delay",s_retry_pending&&s_retry_at==13350&&s_retry_delay==4000);
    for (int i=0;i<8;++i) stadia_schedule_reconnect();
    check("repeated failures cap backoff at ten seconds",s_retry_delay==10000&&s_retry_at==21350);
    StadiaBLE_StopScan(); g_sys_tick_ms=30000; scans=s_scan_calls; StadiaBLE_Poll();
    check("short stop pauses automatic reconnect including backoff",!s_retry_pending&&s_scan_calls==scans&&!s_auto_enabled);
    StadiaBLE_StartScan();
    check("short scan resumes targeting remembered peer",s_auto_enabled&&s_target_only&&StadiaBLE_IsScanning());
    StadiaBLE_StopScan(); StadiaBLE_StartScan();
    check("rescan waits for old asynchronous cancellation",s_state==BLE_STATE_SCAN_STOPPING&&s_rescan_requested);
    e.gap.opcode=GAP_DEVICE_DISCOVERY_EVENT; stadia_EventCB(&e);
    check("cancel completion starts one queued scan",StadiaBLE_IsScanning()&&!s_rescan_requested);
    memset(&e,0,sizeof(e)); e.deviceDirectInfo.opcode=GAP_DIRECT_DEVICE_INFO_EVENT;
    memcpy(e.deviceDirectInfo.addr,address,6); stadia_EventCB(&e);
    check("known direct advertisement selects saved peer",s_stadia_found);
    e.gap.opcode=GAP_DEVICE_DISCOVERY_EVENT; stadia_EventCB(&e);
    e.gap.opcode=GAP_LINK_ESTABLISHED_EVENT; e.linkCmpl.connectionHandle=7; stadia_EventCB(&e);
    int saves=peer_saves; StadiaBLE_Poll();
    check("a link alone cannot persist an unverified HID peer",peer_saves==saves);
    s_procedure_in_progress=false; s_disc_state=DISC_IDLE;
    stadia_PairStateCB(8,GAPBOND_PAIRING_STATE_STARTED,SUCCESS);
    check("stale security callback cannot block current discovery",!s_pairing_in_progress);
    stadia_PairStateCB(7,GAPBOND_PAIRING_STATE_STARTED,SUCCESS);
    int primary=s_primary_calls; stadia_StartDiscovery();
    check("HID setup waits while native pairing is active",s_primary_calls==primary&&s_pairing_in_progress);
    g_sys_tick_ms+=10000; StadiaBLE_CheckDiscoveryTimeout();
    check("first-time pairing can outlast normal HID discovery inactivity window",s_state==BLE_STATE_CONNECTED&&s_pairing_in_progress);
    stadia_PairStateCB(7,GAPBOND_PAIRING_STATE_COMPLETE,SUCCESS); stadia_StartDiscovery();
    check("HID setup resumes after native pairing",s_primary_calls==primary+1&&!s_pairing_in_progress);
    s_disc_state=DISC_CCCD;
    gattMsgEvent_t msg={0}; msg.connHandle=7; msg.method=ATT_WRITE_RSP; stadia_GATTMsgCB(&msg);
    check("successful HID subscription defers persistence outside GAP callback",s_save_peer_pending&&peer_saves==saves);
    s_disc_state=DISC_IDLE; s_discovery_timeout_armed=false; StadiaBLE_Poll();
    check("main loop persists only the usable peer and resets retry delay",peer_saves==saves+1&&s_peer.valid&&s_retry_delay==1000);
    e.gap.opcode=GAP_LINK_TERMINATED_EVENT; stadia_EventCB(&e);
    check("unrequested link loss automatically schedules remembered peer",s_retry_pending&&s_retry_at==g_sys_tick_ms+1000);
    s_retry_pending=false; s_retry_delay=1000; g_sys_tick_ms=UINT32_MAX-500;
    stadia_schedule_reconnect(); scans=s_scan_calls; g_sys_tick_ms=498; StadiaBLE_Poll();
    check("reconnect deadline across wrap does not fire early",s_scan_calls==scans);
    g_sys_tick_ms=499; StadiaBLE_Poll();
    check("reconnect deadline across wrap starts at correct time",s_scan_calls==scans+1);

    /* Forget must quiesce the radio before either store is changed. */
    int clears=peer_clears, erases=bond_erases;
    StadiaBLE_ForgetPeer(); StadiaBLE_Poll();
    check("forget cancels scan before touching Flash",s_forget_pending&&s_state==BLE_STATE_SCAN_STOPPING&&peer_clears==clears&&bond_erases==erases);
    e.gap.opcode=GAP_DEVICE_DISCOVERY_EVENT; stadia_EventCB(&e);
    mock_clear_ok=false; g_sys_tick_ms+=1000; StadiaBLE_Poll();
    check("failed journal clear cannot claim forgotten or erase SDK keys",s_forget_pending&&s_peer.valid&&bond_erases==erases);
    mock_clear_ok=true; bond_erase_status=blePending; g_sys_tick_ms+=1000; StadiaBLE_Poll();
    check("SDK erase rejection keeps forget pending",s_forget_pending&&!s_peer.valid&&!s_keys_erased);
    bond_erase_status=SUCCESS; bond_count=1; g_sys_tick_ms+=1000; StadiaBLE_Poll();
    check("forget waits for SDK bond count to reach zero",s_forget_pending&&s_keys_erased&&s_state==BLE_STATE_IDLE);
    erases=bond_erases; bond_count=0; g_sys_tick_ms+=1000; StadiaBLE_Poll();
    check("completed forget scans for a new Stadia without repeating SDK erase",
          !s_forget_pending&&!s_peer.valid&&!s_target_only&&StadiaBLE_IsScanning()&&bond_erases==erases);

    /* A connection accepted just before forget must not revive a saved peer. */
    s_state=BLE_STATE_CONNECTING; s_connect_cancelled=false; s_peer=mock_peer;
    StadiaBLE_ForgetPeer(); StadiaBLE_Poll();
    check("forget cancels in-flight connection using SDK supported handle",s_connect_cancelled&&terminated_handle==INVALID_CONNHANDLE);
    e.gap.opcode=GAP_LINK_ESTABLISHED_EVENT; e.gap.hdr.status=SUCCESS; e.linkCmpl.connectionHandle=9;
    saves=peer_saves; stadia_EventCB(&e); StadiaBLE_Poll();
    check("late connection during forget is disconnected without resaving peer",s_state==BLE_STATE_DISCONNECTING&&peer_saves==saves&&!s_save_peer_pending);
    e.gap.opcode=GAP_LINK_TERMINATED_EVENT; stadia_EventCB(&e);
    g_sys_tick_ms+=1000; StadiaBLE_Poll();
    check("forget completes after late connection terminates",!s_forget_pending&&!s_peer.valid&&StadiaBLE_IsScanning());
    s_peer=mock_peer; s_state=BLE_STATE_CONNECTED; g_stadia_connected=true;
    s_hid_notifications_enabled=true; s_stadia_found=true; s_save_peer_pending=true;
    s_discovery_timeout_armed=false; mock_save_ok=false; StadiaBLE_Poll();
    check("peer persistence failure preserves working link and old identity",s_state==BLE_STATE_CONNECTED&&s_peer.valid&&!s_save_peer_pending);
    mock_save_ok=true;
    s_state=BLE_STATE_SCANNING; g_stadia_connected=false; s_stadia_found=false;
    scan_cancel_status=blePending; StadiaBLE_StopScan();
    memset(&e,0,sizeof(e)); e.deviceInfo.opcode=GAP_DEVICE_INFO_EVENT;
    memcpy(e.deviceInfo.addr,address,6); stadia_EventCB(&e);
    check("failed manual scan cancellation still ignores later saved-peer advertisements",!s_stadia_found&&!s_auto_enabled);
    e.gap.opcode=GAP_DEVICE_DISCOVERY_EVENT; scans=s_scan_calls; stadia_EventCB(&e);
    check("natural completion after rejected cancellation cannot restart scanning",s_state==BLE_STATE_IDLE&&s_scan_calls==scans);
    scan_cancel_status=SUCCESS;
    s_state=BLE_STATE_CONNECTED; g_stadia_connected=true; s_disconnect_status=blePending;
    StadiaBLE_ForgetPeer(); StadiaBLE_Poll(); disconnects=s_disconnect_calls;
    StadiaBLE_Poll();
    check("rejected forget disconnect is retried without a busy loop",s_forget_pending&&s_state==BLE_STATE_CONNECTED&&s_disconnect_calls==disconnects);
    s_disconnect_status=SUCCESS; g_sys_tick_ms+=1000; StadiaBLE_Poll();
    check("forget retries rejected disconnect after one second",s_state==BLE_STATE_DISCONNECTING&&s_disconnect_calls==disconnects+1);
    e.gap.opcode=GAP_LINK_TERMINATED_EVENT; stadia_EventCB(&e);
    g_sys_tick_ms+=1000; StadiaBLE_Poll();
    check("forget resumes pairing scan after disconnect retry succeeds",!s_forget_pending&&StadiaBLE_IsScanning());
}

int main(void)
{
    gapRoleEvent_t event;
    memset(&event,0,sizeof(event));
    event.gap.opcode = GAP_DEVICE_INIT_DONE_EVENT;
    stadia_EventCB(&event);
    check("central uses factory public address without inventing random-static identity",
          g_ble_ready && s_addr_type == ADDRTYPE_PUBLIC && s_addr_is_null);
    s_addr_status = bleNotReady;
    stadia_EventCB(&event);
    check("address configuration rejection cannot report BLE ready", !g_ble_ready);
    s_addr_status = SUCCESS;
    event.gap.hdr.status = bleNotReady;
    stadia_EventCB(&event);
    check("failed device init cannot report BLE ready", !g_ble_ready);
    event.gap.hdr.status = SUCCESS;
    stadia_EventCB(&event);
    StadiaBLE_StartScan();
    check("scan enters SCANNING", StadiaBLE_IsScanning() && s_scan_calls == 1);
    s_stadia_found = true;
    StadiaBLE_StopScan();
    event.gap.opcode = GAP_DEVICE_DISCOVERY_EVENT;
    stadia_EventCB(&event);
    check("cancelled scan completion cannot connect", s_connect_calls == 0 && s_state == BLE_STATE_IDLE);

    s_scan_status = blePending;
    StadiaBLE_StartScan();
    check("scan API rejection returns to IDLE", s_state == BLE_STATE_IDLE);
    s_scan_status = SUCCESS;
    StadiaBLE_StartScan();
    s_stadia_found = true;
    s_connect_status = blePending;
    stadia_EventCB(&event);
    check("connect API rejection permits manual retry", s_state == BLE_STATE_IDLE);

    s_connect_status = SUCCESS;
    StadiaBLE_StartScan();
    event.gap.opcode = GAP_LINK_ESTABLISHED_EVENT;
    event.gap.hdr.status = bleTimeout;
    stadia_EventCB(&event);
    check("failed connection restarts in SCANNING", StadiaBLE_IsScanning());

    event.gap.hdr.status = SUCCESS;
    event.linkCmpl.connectionHandle = 7;
    stadia_EventCB(&event);
    int scans_before = s_scan_calls;
    StadiaBLE_Disconnect();
    StadiaBLE_StartScan();
    check("BOOT defers scan while disconnecting",
          s_state == BLE_STATE_DISCONNECTING && s_scan_calls == scans_before && s_rescan_requested);
    g_stadia_report_ready = true;
    g_stadia_bat_read_pending = true;
    event.gap.opcode = GAP_LINK_TERMINATED_EVENT;
    stadia_EventCB(&event);
    check("disconnect clears stale input and battery pending",
          !g_stadia_report_ready && !g_stadia_bat_read_pending && !g_stadia_connected);
    check("disconnect event starts exactly one requested scan",
          StadiaBLE_IsScanning() && s_scan_calls == scans_before + 1 && !s_rescan_requested);

    /* Malformed AD length must not let the name parser read past the buffer. */
    uint8_t ad[] = { 30, 9, 'S', 't', 'a', 'd', 'i', 'a' };
    event.gap.opcode = GAP_DEVICE_INFO_EVENT;
    event.deviceInfo.pEvtData = ad;
    event.deviceInfo.dataLen = sizeof(ad);
    stadia_EventCB(&event);
    check("truncated advertisement cannot match Stadia", !s_stadia_found);

    /* The exact Windows-visible name must match, including a scan response. */
    uint8_t named_ad[] = { 2, 1, 6, 16, 9,
        'S','t','a','d','i','a','Z','6','J','Z','-','2','e','2','3' };
    event.deviceInfo.pEvtData = named_ad;
    event.deviceInfo.dataLen = sizeof(named_ad);
    event.deviceInfo.eventType = GAP_ADRPT_SCAN_RSP;
    event.deviceInfo.addrType = ADDRTYPE_STATIC;
    for (unsigned i = 0; i < B_ADDR_LEN; ++i) event.deviceInfo.addr[i] = (uint8_t)(i + 1);
    stadia_EventCB(&event);
    check("Windows-visible StadiaZ6JZ-2e23 name matches in scan response",
          s_stadia_found && s_stadia_addr_type == ADDRTYPE_STATIC &&
          s_stadia_addr[0] == 1 && s_stadia_addr[5] == 6);
    s_stadia_found = false;
    named_ad[4] = 8;
    event.deviceInfo.eventType = GAP_ADRPT_ADV_IND;
    stadia_EventCB(&event);
    check("shortened-name AD field also accepts the Stadia prefix", s_stadia_found);
    s_stadia_found = false;
    event.deviceInfo.pEvtData = NULL;
    stadia_EventCB(&event);
    check("invalid null advertisement is ignored", !s_stadia_found);

    /* Extended events have a different layout and must not be cast as legacy. */
    memset(&event, 0, sizeof(event));
    event.deviceExtAdvInfo.opcode = GAP_EXT_ADV_DEVICE_INFO_EVENT;
    event.deviceExtAdvInfo.eventType = GAP_ADRPT_EXT_SCAN_RESPONSE;
    event.deviceExtAdvInfo.primaryPHY = 1;
    event.deviceExtAdvInfo.secondaryPHY = 2;
    event.deviceExtAdvInfo.rssi = -42;
    event.deviceExtAdvInfo.addrType = ADDRTYPE_STATIC;
    event.deviceExtAdvInfo.addr[0] = 0xA5;
    event.deviceExtAdvInfo.addr[5] = 0xB6;
    event.deviceExtAdvInfo.pEvtData = named_ad;
    event.deviceExtAdvInfo.dataLen = sizeof(named_ad);
    stadia_EventCB(&event);
    check("extended scan response matches name and preserves device address",
          s_stadia_found && s_stadia_addr[0] == 0xA5 && s_stadia_addr[5] == 0xB6 &&
          s_stadia_addr_type == ADDRTYPE_STATIC);
    s_stadia_found = false;
    event.deviceExtAdvInfo.eventType |= GAP_ADRPT_EXT_DATA_INCOMPLETE;
    stadia_EventCB(&event);
    check("incomplete extended payload cannot trigger connection", !s_stadia_found);
    event.deviceExtAdvInfo.eventType = GAP_ADRPT_EXT_SCAN_RESPONSE | GAP_ADRPT_EXT_DATA_LAST;
    stadia_EventCB(&event);
    check("truncated extended payload cannot trigger connection", !s_stadia_found);
    event.deviceExtAdvInfo.eventType = GAP_ADRPT_EXT_SCAN_RESPONSE;
    event.deviceExtAdvInfo.pEvtData = NULL;
    stadia_EventCB(&event);
    check("null extended payload is ignored safely", !s_stadia_found);
    event.deviceExtAdvInfo.pEvtData = named_ad;
    StadiaBLE_StopScan();
    stadia_EventCB(&event);
    check("extended advertisement after cancelled scan is ignored", !s_stadia_found);
    StadiaBLE_StartScan();
    gapRoleEvent_t cancelled={0}; cancelled.gap.opcode=GAP_DEVICE_DISCOVERY_EVENT;
    stadia_EventCB(&cancelled);
    memset(&event, 0, sizeof(event));
    event.deviceDirectInfo.opcode = GAP_DIRECT_DEVICE_INFO_EVENT;
    event.deviceDirectInfo.addr[0] = 0xC7;
    stadia_EventCB(&event);
    check("direct advertisement is counted without guessing controller identity",
          !s_stadia_found && s_scan_res == 1 && s_dev_list[0].addr[0] == 0xC7);
#if STADIA_SCAN_DIAGNOSTICS
    check("direct diagnostic report has no payload and counts once",
          s_scan_reports == 1 && s_scan_direct == 1);
    event.gap.opcode = GAP_PHY_UPDATE_EVENT;
    stadia_EventCB(&event);
    check("unhandled scan event is visible in diagnostics", s_scan_other == 1);
#endif

    /* Replay the R8 hardware order: the one-second timer fires before the
     * MTU response at ~1.46 seconds. Discovery must wait, then actually start. */
    g_sys_tick_ms = 0;
    memset(&event, 0, sizeof(event));
    event.gap.opcode = GAP_LINK_ESTABLISHED_EVENT;
    event.linkCmpl.connectionHandle = 7;
    stadia_EventCB(&event);
    int primary_before = s_primary_calls;
    int timer_before = s_discovery_timers;
    g_sys_tick_ms = 1000;
    stadia_ProcessEvent(s_task_id, START_SVC_DISCOVERY_EVT);
    check("one-second discovery timer waits for delayed MTU",
          s_primary_calls == primary_before && s_disc_state == DISC_IDLE &&
          s_discovery_timers == timer_before + 1 && s_last_discovery_delay == SVC_DISCOVERY_RETRY);
    gattMsgEvent_t delayed;
    memset(&delayed, 0, sizeof(delayed));
    delayed.connHandle = 7;
    delayed.method = ATT_EXCHANGE_MTU_RSP;
    g_sys_tick_ms = 1460;
    stadia_GATTMsgCB(&delayed);
    s_primary_status = blePending;
    g_sys_tick_ms = 1500;
    stadia_ProcessEvent(s_task_id, START_SVC_DISCOVERY_EVT);
    check("busy HID discovery request retries without entering a false active state",
          s_disc_state == DISC_IDLE && s_discovery_timers == timer_before + 2 &&
          s_last_discovery_delay == SVC_DISCOVERY_RETRY);
    s_primary_status = SUCCESS;
    g_sys_tick_ms = 1550;
    stadia_ProcessEvent(s_task_id, START_SVC_DISCOVERY_EVT);
    check("HID discovery starts after MTU and ATT queue become ready",
          s_disc_state == DISC_SVC && s_primary_calls == primary_before + 2);
    stadia_ProcessEvent(s_task_id, START_SVC_DISCOVERY_EVT);
    check("duplicate discovery timer cannot restart an accepted request",
          s_primary_calls == primary_before + 2);
    int delayed_disconnects = s_disconnect_calls;
    g_sys_tick_ms = 3000;
    StadiaBLE_CheckDiscoveryTimeout();
    check("valid slow discovery is not disconnected at three seconds",
          s_disconnect_calls == delayed_disconnects && s_state == BLE_STATE_CONNECTED);
    uint8_t hid_range[] = { 0x10, 0, 0x20, 0 };
    delayed.method = ATT_FIND_BY_TYPE_VALUE_RSP;
    delayed.msg.findByTypeValueRsp.numInfo = 1;
    delayed.msg.findByTypeValueRsp.pHandlesInfo = hid_range;
    g_sys_tick_ms = 4000;
    stadia_GATTMsgCB(&delayed);
    g_sys_tick_ms = 13000;
    StadiaBLE_CheckDiscoveryTimeout();
    check("real service response renews the discovery inactivity deadline",
          s_disconnect_calls == delayed_disconnects && s_svc_start == 0x10 && s_svc_end == 0x20);
    delayed.method = ATT_HANDLE_VALUE_NOTI;
    delayed.msg.handleValueNoti.handle = 99;
    delayed.msg.handleValueNoti.len = 0;
    stadia_GATTMsgCB(&delayed);
    delayed.method = ATT_ERROR_RSP;
    delayed.msg.errorRsp.reqOpcode = ATT_READ_REQ;
    stadia_GATTMsgCB(&delayed);
    check("unrelated notifications and read errors cannot postpone HID timeout",
          s_discovery_progress_ms == 4000 && s_disc_state == DISC_SVC);
    g_sys_tick_ms = 14000;
    StadiaBLE_CheckDiscoveryTimeout();
    check("truly stalled mandatory discovery still disconnects after its deadline",
          s_disconnect_calls == delayed_disconnects + 1 && s_state == BLE_STATE_DISCONNECTING);
    StadiaBLE_CheckDiscoveryTimeout();
    check("discovery timeout cannot repeatedly terminate the same link",
          s_disconnect_calls == delayed_disconnects + 1);

    s_state = BLE_STATE_CONNECTED;
    s_disc_state = DISC_IDLE;
    s_procedure_in_progress = true;
    s_discovery_timeout_armed = true;
    s_discovery_progress_ms = UINT32_MAX - 5000;
    g_sys_tick_ms = 4998;
    StadiaBLE_CheckDiscoveryTimeout();
    check("MTU wait timeout remains correct across timer wrap",
          s_state == BLE_STATE_CONNECTED);
    g_sys_tick_ms = 4999;
    StadiaBLE_CheckDiscoveryTimeout();
    check("missing MTU response has a bounded wait across timer wrap",
          s_state == BLE_STATE_DISCONNECTING);
    event.gap.opcode = GAP_LINK_TERMINATED_EVENT;
    stadia_EventCB(&event);
    check("disconnect cancels pending discovery deadline", !s_discovery_timeout_armed);

    /* Complete a valid slow HID setup taking more than the old three seconds. */
    g_sys_tick_ms = 0;
    memset(&event, 0, sizeof(event));
    event.gap.opcode = GAP_LINK_ESTABLISHED_EVENT;
    event.linkCmpl.connectionHandle = 7;
    stadia_EventCB(&event);
    memset(&delayed, 0, sizeof(delayed));
    delayed.connHandle = 7;
    delayed.method = ATT_EXCHANGE_MTU_RSP;
    g_sys_tick_ms = 300;
    stadia_GATTMsgCB(&delayed);
    g_sys_tick_ms = 1000;
    stadia_ProcessEvent(s_task_id, START_SVC_DISCOVERY_EVT);
    delayed.method = ATT_FIND_BY_TYPE_VALUE_RSP;
    delayed.hdr.status = bleProcedureComplete;
    delayed.msg.findByTypeValueRsp.numInfo = 1;
    delayed.msg.findByTypeValueRsp.pHandlesInfo = hid_range;
    g_sys_tick_ms = 2300;
    stadia_GATTMsgCB(&delayed);
    uint8_t chars[] = { 0x11,0,0x10,0x12,0,0x4D,0x2A, 0x14,0,0x08,0x15,0,0x4D,0x2A };
    delayed.method = ATT_READ_BY_TYPE_RSP;
    delayed.hdr.status = SUCCESS;
    delayed.msg.readByTypeRsp.numPairs = 2;
    delayed.msg.readByTypeRsp.len = 7;
    delayed.msg.readByTypeRsp.pDataList = chars;
    g_sys_tick_ms = 4600;
    stadia_GATTMsgCB(&delayed);
    StadiaBLE_CheckDiscoveryTimeout();
    check("slow valid HID discovery reaches characteristic handles past three seconds",
          s_state == BLE_STATE_CONNECTED && s_disc_state == DISC_CHAR_ALL &&
          g_hid_input_handle == 0x12 && g_hid_output_handle == 0x15);
    delayed.hdr.status = bleProcedureComplete;
    delayed.msg.readByTypeRsp.numPairs = 0;
    g_sys_tick_ms = 4900;
    stadia_GATTMsgCB(&delayed);
    delayed.method = ATT_WRITE_RSP;
    delayed.hdr.status = SUCCESS;
    g_sys_tick_ms = 5000;
    stadia_GATTMsgCB(&delayed);
    check("late successful HID subscription proceeds to optional battery discovery",
          s_hid_notifications_enabled && s_disc_state == DISC_BAT_SVC);
    delayed.method = ATT_ERROR_RSP;
    delayed.msg.errorRsp.reqOpcode = ATT_FIND_BY_TYPE_VALUE_REQ;
    delayed.msg.errorRsp.errCode = ATT_ERR_ATTR_NOT_FOUND;
    g_sys_tick_ms = 6000;
    stadia_GATTMsgCB(&delayed);
    check("finished optional discovery disables the initialization deadline",
          !s_discovery_timeout_armed && StadiaBLE_IsOutputReady());
    delayed_disconnects = s_disconnect_calls;
    g_sys_tick_ms = 20000;
    StadiaBLE_CheckDiscoveryTimeout();
    check("completed healthy HID link is not disconnected by a later deadline",
          s_disconnect_calls == delayed_disconnects && s_state == BLE_STATE_CONNECTED);
    uint8_t input[] = { 1,2,3,4,5,6,7,8,9,10 };
    delayed.method = ATT_HANDLE_VALUE_NOTI;
    delayed.msg.handleValueNoti.handle = 0x12;
    delayed.msg.handleValueNoti.len = sizeof(input);
    delayed.msg.handleValueNoti.pValue = input;
    stadia_GATTMsgCB(&delayed);
    check("slowly established HID subscription still forwards controller input",
          g_stadia_report_ready && g_stadia_raw_len == sizeof(input) &&
          memcmp(g_stadia_raw_data, input, sizeof(input)) == 0);

    s_state = BLE_STATE_CONNECTED;
    g_stadia_connected = true;
    g_conn_handle = 7;
    g_hid_output_handle = 22;
    s_disc_state = DISC_IDLE;
    s_procedure_in_progress = false;
    s_hid_notifications_enabled = false;
    check("output waits for HID subscription", !StadiaBLE_IsOutputReady());
    int disconnects_before = s_disconnect_calls;
    StadiaBLE_ForceDiscoveryDone();
    check("incomplete HID discovery disconnects rather than pretending ready",
          s_disconnect_calls == disconnects_before + 1 && s_state == BLE_STATE_DISCONNECTING);
    s_state = BLE_STATE_CONNECTED;
    s_hid_notifications_enabled = true;
    s_disc_state = DISC_BAT_SVC;
    StadiaBLE_ForceDiscoveryDone();
    check("optional battery discovery timeout preserves working HID", StadiaBLE_IsOutputReady());

    s_write_status = blePending;
    int frees_before = s_frees;
    check("rejected CCCD write releases allocated buffer",
          !stadia_write_cccd(23) && s_frees == frees_before + 1);
    g_stadia_bat_handle = 30;
    StadiaBLE_ReadBattery();
    check("rejected battery read does not leave pending set", !g_stadia_bat_read_pending);
    g_stadia_bat_read_pending = true;
    gattMsgEvent_t msg;
    memset(&msg,0,sizeof(msg));
    msg.connHandle = 7;
    msg.method = ATT_ERROR_RSP;
    msg.msg.errorRsp.reqOpcode = ATT_READ_REQ;
    stadia_GATTMsgCB(&msg);
    check("battery error response releases pending", !g_stadia_bat_read_pending);

    s_disc_state = DISC_CCCD;
    s_svc_start = 1;
    msg.method = ATT_WRITE_RSP;
    stadia_GATTMsgCB(&msg);
    check("battery discovery starts with a fresh service range", s_svc_start == 0 && s_disc_state == DISC_BAT_SVC);
    msg.method = ATT_ERROR_RSP;
    msg.msg.errorRsp.reqOpcode = ATT_FIND_BY_TYPE_VALUE_REQ;
    stadia_GATTMsgCB(&msg);
    check("missing battery service does not reuse HID handles", s_disc_state == DISC_IDLE);
    s_disc_state = DISC_CHAR_ALL;
    g_hid_input_handle = 0;
    uint8_t short_pair[1] = { 0 };
    msg.method = ATT_READ_BY_TYPE_RSP;
    msg.hdr.status = SUCCESS;
    msg.msg.readByTypeRsp.numPairs = 1;
    msg.msg.readByTypeRsp.len = 1;
    msg.msg.readByTypeRsp.pDataList = short_pair;
    stadia_GATTMsgCB(&msg);
    check("short characteristic entries are ignored", g_hid_input_handle == 0);
    reconnect_tests();
    printf("RESULT: %s\n", s_fail ? "FAIL" : "PASS");
    return s_fail;
}
