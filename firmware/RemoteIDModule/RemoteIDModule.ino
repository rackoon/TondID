/*
  implement OpenDroneID MAVLink and DroneCAN support
 */
/*
  released under GNU GPL v2 or later
 */

#include "options.h"
#include <Arduino.h>
#include "version.h"
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <vector>
#include <opendroneid.h>
#include "mavlink.h"
#include "DroneCAN.h"
#include "WiFi_TX.h"
#include "BLE_TX.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include "parameters.h"
#include "webinterface.h"
#include "check_firmware.h"
#include <esp_ota_ops.h>
#include "efuse.h"
#include "led.h"


#if AP_DRONECAN_ENABLED
static DroneCAN dronecan;
#endif

#if AP_MAVLINK_ENABLED
static MAVLinkSerial mavlink1{Serial1, MAVLINK_COMM_0};
#endif

static WiFi_TX wifi;
static BLE_TX ble;

#define DEBUG_BAUDRATE 57600

// OpenDroneID output data structure
ODID_UAS_Data UAS_data;
String status_reason;
static uint32_t last_location_ms;
static WebInterface webif;

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

static bool arm_check_ok = false; // goes true for LED arm check status
static bool pfst_check_ok = false;
bool mock_rid_active = false;
uint8_t mock_rid_slot = 0;
uint8_t mock_rid_slots = 1;

static constexpr double MOCK_HOME_LAT = 59.4370;
static constexpr double MOCK_HOME_LON = 24.7536;
static constexpr float MOCK_HOME_ALT_GEO = 38.0f;
static constexpr float MOCK_FLIGHT_ALT_GEO = 118.0f;
static constexpr float MOCK_FLIGHT_HEIGHT = 80.0f;
static constexpr float MOCK_GROUND_SPEED = 6.0f;
static constexpr float MOCK_VERTICAL_SPEED = 0.3f;
static constexpr char MOCK_UAS_ID[] = "1596ABC1234567890";
static constexpr char MOCK_SELF_ID[] = "QA TEST FLIGHT";
static constexpr char MOCK_OPERATOR_ID[] = "EST0000000000001";
static constexpr char MOCK_MANUFACTURER[] = "ARUPILOT";
static constexpr char MOCK_MODEL[] = "RID-MOCK";
static constexpr char MOCK_GHOST_PREFIX[] = "1596GHOSTPREFIX000";
static constexpr uint8_t MOCK_UAS_LIMIT = 5;
static constexpr uint32_t SERIAL_COMPAT_BAUD = 115200;
static uint8_t serial_app_mode = 0;
static uint8_t serial_fly_mode = 0;
static uint8_t serial_path_mode = 0;
static String serial_line;
static String serial1_line;
static uint32_t serial_last_rx_ms = 0;
static uint32_t serial1_last_rx_ms = 0;
static uint32_t serial_last_push_ms = 0;
static Stream *serial_reply_port = &Serial;
static bool paused_status_valid = false;
static ODID_UAS_Data paused_status_data;

/*
  setup serial ports
 */
void setup()
{
    // disable brownout checking
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    g.init();
    if (!g.have_basic_id_info()) {
        g.ua_type = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
        g.id_type = ODID_IDTYPE_SERIAL_NUMBER;
        snprintf(g.uas_id, sizeof(g.uas_id), "%s", MOCK_UAS_ID);
    }
    if (strlen(g.mock_operator_id) == 0) {
        snprintf(g.mock_operator_id, sizeof(g.mock_operator_id), "%s", MOCK_OPERATOR_ID);
    }
    if (strlen(g.mock_self_id) == 0) {
        snprintf(g.mock_self_id, sizeof(g.mock_self_id), "%s", MOCK_SELF_ID);
    }
    if (strlen(g.mock_manufacturer) == 0) {
        snprintf(g.mock_manufacturer, sizeof(g.mock_manufacturer), "%s", MOCK_MANUFACTURER);
    }
    if (strlen(g.mock_model) == 0) {
        snprintf(g.mock_model, sizeof(g.mock_model), "%s", MOCK_MODEL);
    }
    if (strlen(g.mock_id_prefix) == 0) {
        snprintf(g.mock_id_prefix, sizeof(g.mock_id_prefix), "%s", MOCK_GHOST_PREFIX);
    }
    serial_app_mode = g.mock_app_mode;
    serial_fly_mode = g.mock_fly_mode;
    serial_path_mode = g.mock_path_mode;
    if (g.bt4_rate <= 0) {
        g.bt4_rate = 1;
    }
    if (g.bt5_rate <= 0) {
        g.bt5_rate = 1;
    }
    if (g.wifi_nan_rate <= 0) {
        g.wifi_nan_rate = 1;
    }
    if (g.wifi_beacon_rate <= 0) {
        g.wifi_beacon_rate = 1;
    }
    g.bcast_powerup = 1;

    led.set_state(Led::LedState::INIT);
    led.update();

    if (g.webserver_enable) {
        // need WiFi for web server
        wifi.init();
    }

    // Browser/WebSerial compatibility with the TondID browser UI on native USB
    Serial.begin(SERIAL_COMPAT_BAUD);

    // Serial1 can be used as a robust browser/config UART bridge on Windows
    Serial1.begin(SERIAL_COMPAT_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);

    // set all fields to invalid/initial values
    odid_initUasData(&UAS_data);

#if AP_MAVLINK_ENABLED
    mavlink1.init();
#endif
#if AP_DRONECAN_ENABLED
    dronecan.init();
#endif

    set_efuses();

    CheckFirmware::check_OTA_running();

#if defined(PIN_CAN_EN)
    // optional CAN enable pin
    pinMode(PIN_CAN_EN, OUTPUT);
    digitalWrite(PIN_CAN_EN, HIGH);
#endif

#if defined(PIN_CAN_nSILENT)
    // disable silent pin
    pinMode(PIN_CAN_nSILENT, OUTPUT);
    digitalWrite(PIN_CAN_nSILENT, HIGH);
#endif

#if defined(PIN_CAN_TERM)
#if !defined(CAN_TERM_EN)
#define CAN_TERM_EN HIGH
#endif

    // optional CAN termination control
    pinMode(PIN_CAN_TERM, OUTPUT);

    if (g.can_term == 1) {
        digitalWrite(PIN_CAN_TERM, CAN_TERM_EN);
    } else {
        digitalWrite(PIN_CAN_TERM, !CAN_TERM_EN);
    }
#endif

#if defined(BUZZER_PIN)
    //set BuZZER OUTPUT ACTIVE, just to show it works
    pinMode(GPIO_NUM_39, OUTPUT);
    digitalWrite(GPIO_NUM_39, HIGH);
#endif

#if defined(PIN_USER_BUTTON)
    pinMode(PIN_USER_BUTTON, INPUT_PULLUP);
#endif

    pfst_check_ok = true;   // note - this will need to be expanded to better capture PFST test status

    // initially set LED for fail
    led.set_state(Led::LedState::ARM_FAIL);

    esp_log_level_set("*", ESP_LOG_DEBUG);

    esp_ota_mark_app_valid_cancel_rollback();
}

#define IMIN(x,y) ((x)<(y)?(x):(y))
#define ODID_COPY_STR(to, from) strncpy(to, (const char*)from, IMIN(sizeof(to), sizeof(from)))

/*
  check parsing of UAS_data, this checks ranges of values to ensure we
  will produce a valid pack
  returns nullptr on no error, or a string error
 */
static const char *check_parse(void)
{
    String ret = "";

    {
        ODID_Location_encoded encoded {};
        if (encodeLocationMessage(&encoded, &UAS_data.Location) != ODID_SUCCESS) {
            ret += "LOC ";
        }
    }
    {
        ODID_System_encoded encoded {};
        if (encodeSystemMessage(&encoded, &UAS_data.System) != ODID_SUCCESS) {
            ret += "SYS ";
        }
    }
    {
        ODID_BasicID_encoded encoded {};
        if (UAS_data.BasicIDValid[0] == 1) {
            if (encodeBasicIDMessage(&encoded, &UAS_data.BasicID[0]) != ODID_SUCCESS) {
                ret += "ID_1 ";
            }
        }
        memset(&encoded, 0, sizeof(encoded));
        if (UAS_data.BasicIDValid[1] == 1) {
            if (encodeBasicIDMessage(&encoded, &UAS_data.BasicID[1]) != ODID_SUCCESS) {
                ret += "ID_2 ";
            }
        }
    }
    {
        ODID_SelfID_encoded encoded {};
        if (encodeSelfIDMessage(&encoded, &UAS_data.SelfID) != ODID_SUCCESS) {
            ret += "SELF_ID ";
        }
    }
    {
        ODID_OperatorID_encoded encoded {};
        if (encodeOperatorIDMessage(&encoded, &UAS_data.OperatorID) != ODID_SUCCESS) {
            ret += "OP_ID ";
        }
    }
    if (ret.length() > 0) {
        // if all errors would occur in this function, it will fit in
        // 50 chars that is also the max for the arm status message
        static char return_string[50];
        memset(return_string, 0, sizeof(return_string));
        snprintf(return_string, sizeof(return_string)-1, "bad %s data", ret.c_str());
        return return_string;
    }
    return nullptr;
}

static float mock_timestamp_seconds(void)
{
    time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm utc_tm {};
        gmtime_r(&now, &utc_tm);
        return float(utc_tm.tm_min * 60 + utc_tm.tm_sec);
    }
    return float((millis() / 1000U) % 3600U);
}

static uint32_t mock_system_timestamp(void)
{
    time_t now = time(nullptr);
    static constexpr time_t rid_epoch = 1546300800; // 2019-01-01 00:00:00 UTC
    if (now > rid_epoch) {
        return uint32_t(now - rid_epoch);
    }
    return uint32_t(millis() / 1000U);
}

static uint8_t mock_slot_count(void)
{
    uint8_t total = 0;
    for (uint8_t slot = 0; slot < MOCK_SLOT_COUNT; slot++) {
        if (g.mock_slot_enabled[slot]) {
            total++;
        }
    }
    return total > MOCK_UAS_LIMIT ? MOCK_UAS_LIMIT : total;
}

static bool mock_slot_enabled(uint8_t slot)
{
    return slot < MOCK_SLOT_COUNT && g.mock_slot_enabled[slot] != 0;
}

static uint8_t resolve_enabled_slot(uint8_t enabled_index)
{
    uint8_t seen = 0;
    for (uint8_t slot = 0; slot < MOCK_SLOT_COUNT; slot++) {
        if (!mock_slot_enabled(slot)) {
            continue;
        }
        if (seen == enabled_index) {
            return slot;
        }
        seen++;
    }
    return 0;
}

static void update_mock_ghost_count_compat(void)
{
    const uint8_t enabled = mock_slot_count();
    g.set_by_name_uint8("MOCK_GHOSTS", enabled > 0 ? uint8_t(enabled - 1) : 0);
}

static void default_slot_mac(uint8_t slot, uint8_t mac[6])
{
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    mac[0] |= 0x02;
    mac[0] &= 0xFE;
    mac[5] = uint8_t(mac[5] + slot);
}

static bool parse_mac_string(const char *s, uint8_t mac[6])
{
    if (s == nullptr || strlen(s) < 17) {
        return false;
    }
    unsigned int bytes[6] {};
    if (sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x",
               &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != 6) {
        return false;
    }
    for (uint8_t i = 0; i < 6; i++) {
        mac[i] = uint8_t(bytes[i] & 0xFF);
    }
    mac[0] |= 0x02;
    mac[0] &= 0xFE;
    return true;
}

static void format_mac_string(const uint8_t mac[6], char mac_out[18])
{
    snprintf(mac_out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void get_slot_mac(uint8_t slot, uint8_t mac[6])
{
    if (slot < MOCK_SLOT_COUNT && parse_mac_string(g.mock_slot_mac[slot], mac)) {
        return;
    }
    default_slot_mac(slot, mac);
}

static const char *slot_or_default(const char *slot_value, const char *fallback)
{
    return (slot_value != nullptr && slot_value[0] != 0) ? slot_value : fallback;
}

static uint8_t slot_ua_type(uint8_t slot, uint8_t fallback)
{
    const uint8_t configured = slot < MOCK_SLOT_COUNT ? g.mock_slot_ua_type[slot] : 0;
    return configured > 0 ? configured : fallback;
}

static uint8_t slot_id_type(uint8_t slot, uint8_t fallback)
{
    const uint8_t configured = slot < MOCK_SLOT_COUNT ? g.mock_slot_id_type[slot] : 0;
    return configured > 0 ? configured : fallback;
}

static float mock_slot_hold_ms(void)
{
    float max_rate = 0.0f;
    if (g.bt4_rate > max_rate) {
        max_rate = g.bt4_rate;
    }
    if (g.bt5_rate > max_rate) {
        max_rate = g.bt5_rate;
    }
    if (g.wifi_nan_rate > max_rate) {
        max_rate = g.wifi_nan_rate;
    }
    if (g.wifi_beacon_rate > max_rate) {
        max_rate = g.wifi_beacon_rate;
    }
    if (max_rate < 0.1f) {
        max_rate = 1.0f;
    }
    float hold = 1100.0f / max_rate;
    if (hold < 220.0f) {
        hold = 220.0f;
    }
    return hold;
}

static void make_mock_uas_id(uint8_t slot, char uas_id[ODID_ID_SIZE+1])
{
    memset(uas_id, 0, ODID_ID_SIZE + 1);
    if (slot < MOCK_SLOT_COUNT && strlen(g.mock_slot_uas_id[slot]) > 0) {
        snprintf(uas_id, ODID_ID_SIZE + 1, "%s", g.mock_slot_uas_id[slot]);
        return;
    }
    const char *base = strlen(g.uas_id) > 0 ? g.uas_id : MOCK_UAS_ID;
    if (slot == 0) {
        snprintf(uas_id, ODID_ID_SIZE + 1, "%s", base);
        return;
    }
    const uint8_t scheme = g.mock_id_scheme <= 2 ? g.mock_id_scheme : 0;
    if (scheme == 1) {
        uint32_t hash = 2166136261u;
        const char *seed = strlen(g.mock_id_prefix) > 0 ? g.mock_id_prefix : base;
        for (const char *p = seed; *p; p++) {
            hash ^= uint8_t(*p);
            hash *= 16777619u;
        }
        hash ^= uint32_t(slot) * 2654435761u;
        static const char alphabet[] = "0123456789ABCDEF";
        memcpy(uas_id, "1596", 4);
        for (uint8_t i = 4; i < ODID_ID_SIZE; i++) {
            hash ^= (hash >> 13);
            hash *= 1274126177u;
            uas_id[i] = alphabet[(hash >> 28) & 0x0F];
        }
        uas_id[ODID_ID_SIZE] = 0;
        return;
    }
    const char *prefix_base = (scheme == 2 && strlen(g.mock_id_prefix) > 0) ? g.mock_id_prefix : base;
    const size_t prefix_len = IMIN(strnlen(prefix_base, ODID_ID_SIZE), size_t(17));
    memcpy(uas_id, prefix_base, prefix_len);
    snprintf(&uas_id[prefix_len], ODID_ID_SIZE + 1 - prefix_len, "%03u", unsigned(slot));
}

static void make_mock_self_id(uint8_t slot, char self_id[ODID_STR_SIZE+1])
{
    memset(self_id, 0, ODID_STR_SIZE + 1);
    if (slot < MOCK_SLOT_COUNT && strlen(g.mock_slot_self_id[slot]) > 0) {
        snprintf(self_id, ODID_STR_SIZE + 1, "%s", g.mock_slot_self_id[slot]);
        return;
    }
    const char *base = strlen(g.mock_self_id) > 0 ? g.mock_self_id : MOCK_SELF_ID;
    if (slot == 0) {
        snprintf(self_id, ODID_STR_SIZE + 1, "%s", base);
        return;
    }
    const size_t prefix_len = IMIN(strnlen(base, ODID_STR_SIZE), size_t(18));
    memcpy(self_id, base, prefix_len);
    snprintf(&self_id[prefix_len], ODID_STR_SIZE + 1 - prefix_len, " G%02u", unsigned(slot));
}

static void fill_mock_uas_data(uint8_t slot, uint8_t slot_count)
{
    odid_initUasData(&UAS_data);

    char uas_id[ODID_ID_SIZE+1] {};
    char self_id[ODID_STR_SIZE+1] {};
    make_mock_uas_id(slot, uas_id);
    make_mock_self_id(slot, self_id);

    const ODID_uatype_t ua_type = (ODID_uatype_t)slot_ua_type(slot, g.ua_type > 0 ? g.ua_type : ODID_UATYPE_HELICOPTER_OR_MULTIROTOR);
    const ODID_idtype_t id_type = (ODID_idtype_t)slot_id_type(slot, g.id_type > 0 ? g.id_type : ODID_IDTYPE_SERIAL_NUMBER);

    UAS_data.BasicID[0].UAType = ua_type;
    UAS_data.BasicID[0].IDType = id_type;
    ODID_COPY_STR(UAS_data.BasicID[0].UASID, uas_id);
    UAS_data.BasicIDValid[0] = 1;

    UAS_data.SelfID.DescType = ODID_DESC_TYPE_TEXT;
    ODID_COPY_STR(UAS_data.SelfID.Desc, self_id);
    UAS_data.SelfIDValid = 1;

    UAS_data.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
    ODID_COPY_STR(UAS_data.OperatorID.OperatorId, slot_or_default(g.mock_slot_operator_id[slot], g.mock_operator_id));
    UAS_data.OperatorIDValid = 1;

    const float orbit_radius = g.mock_ring_radius > 10.0f ? g.mock_ring_radius : 120.0f;
    const float speed = g.mock_speed > 0.1f ? g.mock_speed : MOCK_GROUND_SPEED;
    const float slot_seed = 0.91f + slot * 1.37f;
    const double meters_to_lat = 1.0 / 111111.0;
    const double home_lat = g.mock_home_lat;
    const double home_lon = g.mock_home_lon;
    const double meters_to_lon = 1.0 / (111111.0 * cos(home_lat * DEG_TO_RAD));
    const float swarm_anchor_radius = orbit_radius * 0.55f;
    const float slot_anchor_angle = slot_seed * 0.77f + slot * 1.31f;
    const float slot_anchor_scale = slot == 0 ? 0.0f : (0.45f + 0.07f * slot);
    const float anchor_north = slot == 0 ? 0.0f : cosf(slot_anchor_angle) * swarm_anchor_radius * slot_anchor_scale;
    const float anchor_east = slot == 0 ? 0.0f : sinf(slot_anchor_angle) * swarm_anchor_radius * slot_anchor_scale;
    const float local_radius = slot == 0 ? orbit_radius * 0.18f : orbit_radius * (0.12f + 0.02f * (slot % 3));
    const float local_radius_clamped = IMIN(max(local_radius, 35.0f), orbit_radius * 0.32f);
    const float phase_rate = (speed / max(local_radius_clamped, 20.0f)) * (0.75f + 0.08f * slot);
    const float phase = millis() * 0.001f * phase_rate + slot_seed * 0.9f;
    float north_m = 0.0f;
    float east_m = 0.0f;
    float next_north = 0.0f;
    float next_east = 0.0f;
    auto fill_orbit_path = [&](float p, float &north, float &east) {
        const float radius_a = local_radius_clamped * (0.95f + 0.10f * sinf(slot_seed));
        const float radius_b = local_radius_clamped * 0.34f;
        north = anchor_north +
                sinf(p * (0.92f + 0.09f * slot) + slot_seed) * radius_a +
                sinf(p * (0.38f + 0.03f * slot) + slot_seed * 0.6f) * radius_b;
        east = anchor_east +
               cosf(p * (0.88f + 0.08f * slot) + slot_seed * 1.1f) * radius_a +
               cosf(p * (0.41f + 0.02f * slot) + slot_seed * 0.4f) * (radius_b * 0.9f);
    };
    auto fill_wander_path = [&](float p, float &north, float &east) {
        const float base = local_radius_clamped * 0.9f;
        north = anchor_north +
                sinf(p * (0.21f + slot * 0.02f) + slot_seed * 0.3f) * base +
                sinf(p * (0.67f + slot * 0.03f) + slot_seed * 1.1f) * (base * 0.75f) +
                cosf(p * (0.13f + slot * 0.01f) + slot_seed * 1.8f) * (base * 0.45f);
        east = anchor_east +
               cosf(p * (0.24f + slot * 0.02f) + slot_seed * 0.9f) * base +
               cosf(p * (0.54f + slot * 0.02f) + slot_seed * 1.4f) * (base * 0.82f) +
               sinf(p * (0.10f + slot * 0.01f) + slot_seed * 0.5f) * (base * 0.38f);
    };
    auto fill_figure8_path = [&](float p, float &north, float &east) {
        const float base = local_radius_clamped;
        north = anchor_north + sinf(p * (0.97f + slot * 0.04f) + slot_seed) * base;
        east = anchor_east + sinf((p * (0.97f + slot * 0.04f) + slot_seed) * 2.0f) * (base * 0.68f);
    };
    uint8_t movement_mode = g.mock_flight_mode <= 2 ? g.mock_flight_mode : 0;
    if (movement_mode == 2) {
        movement_mode = (slot % 3 == 0) ? 0 : ((slot % 3 == 1) ? 1 : 3);
    }
    switch (movement_mode) {
    case 1:
        fill_wander_path(phase, north_m, east_m);
        fill_wander_path(phase + 0.08f, next_north, next_east);
        break;
    case 3:
        fill_figure8_path(phase, north_m, east_m);
        fill_figure8_path(phase + 0.08f, next_north, next_east);
        break;
    default:
        fill_orbit_path(phase, north_m, east_m);
        fill_orbit_path(phase + 0.08f, next_north, next_east);
        break;
    }
    const double latitude = home_lat + north_m * meters_to_lat;
    const double longitude = home_lon + east_m * meters_to_lon;
    const float direction = fmodf((atan2f(next_east - east_m, next_north - north_m) * RAD_TO_DEG) + 360.0f, 360.0f);
    const float timestamp = mock_timestamp_seconds();
    const float altitude_wave = 3.2f * sinf(phase * (0.52f + slot * 0.03f) + slot_seed);
    const float height_wave = 2.1f * cosf(phase * (0.61f + slot * 0.05f) + slot_seed * 0.8f);
    const uint8_t altitude_mode = g.mock_altitude_mode <= 2 ? g.mock_altitude_mode : 0;
    float altitude_geo = MOCK_FLIGHT_ALT_GEO;
    float height = MOCK_FLIGHT_HEIGHT;
    const float slot_alt_seed = (sinf(slot_seed * 2.17f) * 0.5f + 0.5f);
    const float slot_height_seed = (cosf(slot_seed * 1.41f) * 0.5f + 0.5f);
    switch (altitude_mode) {
    case 1: {
        const float pseudo_band = 12.0f + slot * 16.0f + slot_alt_seed * 10.0f;
        altitude_geo = MOCK_FLIGHT_ALT_GEO + pseudo_band + altitude_wave;
        height = MOCK_FLIGHT_HEIGHT + pseudo_band * 0.72f + height_wave;
        break;
    }
    case 2:
        altitude_geo = MOCK_FLIGHT_ALT_GEO + 8.0f + slot * 11.0f + slot_alt_seed * 4.0f + altitude_wave * 0.7f;
        height = MOCK_FLIGHT_HEIGHT + 5.0f + slot * 7.0f + slot_height_seed * 3.0f + height_wave * 0.7f;
        break;
    default:
        altitude_geo = MOCK_FLIGHT_ALT_GEO + slot * 22.0f + slot_alt_seed * 3.0f + altitude_wave;
        height = MOCK_FLIGHT_HEIGHT + slot * 15.0f + slot_height_seed * 2.0f + height_wave;
        break;
    }

    UAS_data.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    UAS_data.System.ClassificationType = ODID_CLASSIFICATION_TYPE_UNDECLARED;
    UAS_data.System.OperatorLatitude = home_lat;
    UAS_data.System.OperatorLongitude = home_lon;
    UAS_data.System.AreaCount = 1;
    UAS_data.System.AreaRadius = uint16_t(IMIN(2550.0f, orbit_radius + 150.0f));
    UAS_data.System.AreaCeiling = 120.0f;
    UAS_data.System.AreaFloor = 0.0f;
    UAS_data.System.CategoryEU = ODID_CATEGORY_EU_UNDECLARED;
    UAS_data.System.ClassEU = ODID_CLASS_EU_UNDECLARED;
    UAS_data.System.OperatorAltitudeGeo = MOCK_HOME_ALT_GEO;
    UAS_data.System.Timestamp = mock_system_timestamp();
    UAS_data.SystemValid = 1;

    UAS_data.Location.Status = ODID_STATUS_AIRBORNE;
    UAS_data.Location.Direction = direction;
    UAS_data.Location.SpeedHorizontal = speed + slot * 0.7f;
    UAS_data.Location.SpeedVertical = sinf(phase * (0.45f + slot * 0.04f) + slot_seed) * (MOCK_VERTICAL_SPEED + slot * 0.05f);
    UAS_data.Location.Latitude = latitude;
    UAS_data.Location.Longitude = longitude;
    UAS_data.Location.AltitudeBaro = altitude_geo;
    UAS_data.Location.AltitudeGeo = altitude_geo;
    UAS_data.Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
    UAS_data.Location.Height = height;
    UAS_data.Location.HorizAccuracy = ODID_HOR_ACC_10_METER;
    UAS_data.Location.VertAccuracy = ODID_VER_ACC_10_METER;
    UAS_data.Location.BaroAccuracy = ODID_VER_ACC_10_METER;
    UAS_data.Location.SpeedAccuracy = ODID_SPEED_ACC_1_METERS_PER_SECOND;
    UAS_data.Location.TSAccuracy = ODID_TIME_ACC_1_0_SECOND;
    UAS_data.Location.TimeStamp = timestamp;
    UAS_data.LocationValid = 1;

    const char *reason = check_parse();
    arm_check_ok = (reason == nullptr);
    led.set_state(pfst_check_ok && arm_check_ok ? Led::LedState::ARM_OK : Led::LedState::ARM_FAIL);
}

static void capture_paused_status(void)
{
    if (mock_rid_active) {
        fill_mock_uas_data(0, mock_slot_count());
    }
    paused_status_data = UAS_data;
    paused_status_valid = true;
}

static void stop_rf_output(void)
{
    ble.stop();
    wifi.stop();
}

static void split_serial_fields(const String &input, std::vector<String> &fields)
{
    fields.clear();
    int start = 0;
    while (start <= input.length()) {
        const int delim = input.indexOf('|', start);
        if (delim < 0) {
            fields.push_back(input.substring(start));
            break;
        }
        fields.push_back(input.substring(start, delim));
        start = delim + 1;
    }
}

static uint8_t serial_mock_slots(void)
{
    return mock_slot_count();
}

static void serial_make_mac(uint8_t slot, char mac_out[18])
{
    uint8_t mac[6] {};
    get_slot_mac(slot, mac);
    format_mac_string(mac, mac_out);
}

static Print &serial_out(void)
{
    return *serial_reply_port;
}

static void serial_printf(const char *fmt, ...)
{
    char buffer[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    serial_out().print(buffer);
}

static void serial_send_ok(void)
{
    serial_printf("$%%\r\n");
}

static void serial_send_version(void)
{
    serial_printf("$V|%u\r\n", FW_VERSION_MAJOR * 1000U + FW_VERSION_MINOR);
}

static void serial_send_summary_from(const ODID_UAS_Data &data)
{
    serial_printf("$C|%f|%f|%f|%f|%f|%f|%d|%d|%d|%d|%d\r\n",
                  data.Location.Latitude,
                  data.Location.Longitude,
                  data.System.OperatorLatitude,
                  data.System.OperatorLongitude,
                  data.Location.AltitudeGeo,
                  data.System.OperatorAltitudeGeo,
                  int(data.Location.SpeedHorizontal),
                  int(data.Location.Direction),
                  10,
                  serial_fly_mode,
                  serial_path_mode);
}

static void serial_send_current(void)
{
    const uint8_t slots = serial_mock_slots();
    if (serial_app_mode == 1 && serial_fly_mode == 1) {
        for (uint8_t enabled_slot = 0; enabled_slot < slots; enabled_slot++) {
            const uint8_t slot = resolve_enabled_slot(enabled_slot);
            fill_mock_uas_data(slot, slots);
            char mac[18] {};
            serial_make_mac(slot, mac);
            serial_printf("$T|%f|%f|%f|%d|%d|%s|%s|%s|%s|%d|%d|%d|%s|%s\r\n",
                          UAS_data.Location.Latitude,
                          UAS_data.Location.Longitude,
                          UAS_data.Location.AltitudeGeo,
                          int(UAS_data.Location.SpeedHorizontal),
                          int(UAS_data.Location.Direction),
                          mac,
                          UAS_data.BasicID[0].UASID,
                          g.mock_operator_id,
                          UAS_data.SelfID.Desc,
                          int(UAS_data.BasicID[0].UAType),
                          int(UAS_data.BasicID[0].IDType),
                          serial_fly_mode,
                          g.mock_manufacturer,
                          g.mock_model);
        }
        return;
    }

    if (serial_fly_mode == 0) {
        if (!paused_status_valid) {
            capture_paused_status();
        }
        serial_send_summary_from(paused_status_data);
        return;
    }

    if (slots > 0) {
        fill_mock_uas_data(resolve_enabled_slot(0), slots);
        serial_send_summary_from(UAS_data);
    }
}

static void serial_send_data(void)
{
    char mac[18] {};
    serial_make_mac(0, mac);
    const uint8_t total_uas = serial_mock_slots();
    serial_printf("$D|%u|%s|%s|%s|%d|%d|%f|%f|%d|%f|%f|%d|%d|%d|%s|%d|%f|%f|%d|%d|0|0|0|0|0|0|0|0|0|%s|%s|%d|%s|%d|%d\r\n",
                  FW_VERSION_MAJOR * 1000U + FW_VERSION_MINOR,
                  g.uas_id,
                  g.mock_operator_id,
                  g.mock_self_id,
                  int(g.ua_type),
                  int(g.id_type),
                  g.mock_home_lat,
                  g.mock_home_lon,
                  int(MOCK_FLIGHT_HEIGHT),
                  g.mock_home_lat,
                  g.mock_home_lon,
                  int(MOCK_HOME_ALT_GEO),
                  int(g.mock_speed),
                  10,
                  mac,
                  serial_app_mode,
                  g.mock_home_lat,
                  g.mock_home_lon,
                  int(g.mock_ring_radius),
                  int(total_uas),
                  g.mock_manufacturer,
                  g.mock_model,
                  int(g.mock_id_scheme),
                  g.mock_id_prefix,
                  int(g.mock_flight_mode),
                  int(g.mock_altitude_mode));
}

static void serial_send_slot(uint8_t slot)
{
    if (slot >= MOCK_SLOT_COUNT) {
        serial_printf("$-\r\n");
        return;
    }
    char mac[18] {};
    serial_make_mac(slot, mac);
    char uas_id[ODID_ID_SIZE+1] {};
    char self_id[ODID_STR_SIZE+1] {};
    make_mock_uas_id(slot, uas_id);
    make_mock_self_id(slot, self_id);
    const char *op = slot_or_default(g.mock_slot_operator_id[slot], g.mock_operator_id);
    const char *mfr = slot_or_default(g.mock_slot_manufacturer[slot], g.mock_manufacturer);
    const char *model = slot_or_default(g.mock_slot_model[slot], g.mock_model);
    serial_printf("$G|%u|%u|%s|%s|%s|%s|%s|%s|%u|%u\r\n",
                  unsigned(slot),
                  unsigned(mock_slot_enabled(slot) ? 1 : 0),
                  mac,
                  uas_id,
                  op,
                  self_id,
                  mfr,
                  model,
                  unsigned(slot_ua_type(slot, g.ua_type)),
                  unsigned(slot_id_type(slot, g.id_type)));
}

static void serial_push_status(void)
{
    const uint32_t now_ms = millis();
    if (now_ms - serial_last_push_ms < 1000) {
        return;
    }
    serial_last_push_ms = now_ms;
    serial_send_data();
    serial_send_current();
}

static void serial_apply_data(const std::vector<String> &fields)
{
    if (fields.size() < 19) {
        return;
    }
    g.set_by_name_string("UAS_ID", fields[0].c_str());
    g.set_by_name_string("MOCK_OP_ID", fields[1].c_str());
    g.set_by_name_string("MOCK_SELF_ID", fields[2].c_str());
    g.set_by_name_string("UAS_TYPE", fields[3].c_str());
    g.set_by_name_string("UAS_ID_TYPE", fields[4].c_str());
    serial_app_mode = uint8_t(fields[14].toInt());
    g.set_by_name_uint8("MOCK_APP_MODE", serial_app_mode);

    const float origin_lat = fields[5].toFloat();
    const float origin_lon = fields[6].toFloat();
    const bool have_origin = (origin_lat != 0.0f || origin_lon != 0.0f);
    if (have_origin) {
        g.set_by_name_string("MOCK_HOME_LAT", String(origin_lat, 6).c_str());
        g.set_by_name_string("MOCK_HOME_LON", String(origin_lon, 6).c_str());
    }
    g.set_by_name_string("MOCK_RADIUS", fields[17].c_str());

    int total_uas = fields[18].toInt();
    if (total_uas < 0) {
        total_uas = 0;
    }
    if (total_uas > MOCK_UAS_LIMIT) {
        total_uas = MOCK_UAS_LIMIT;
    }
    g.set_by_name_uint8("MOCK_GHOSTS", total_uas > 0 ? uint8_t(total_uas - 1) : 0);
    if (fields.size() > 11) {
        g.set_by_name_string("MOCK_SPEED", fields[11].c_str());
    }
    if (fields.size() > 27) {
        g.set_by_name_string("MOCK_MFR", fields[27].c_str());
    }
    if (fields.size() > 28) {
        g.set_by_name_string("MOCK_MODEL", fields[28].c_str());
    }
    if (fields.size() > 29) {
        g.set_by_name_uint8("MOCK_ID_SCHEME", uint8_t(fields[29].toInt()));
    }
    if (fields.size() > 30) {
        g.set_by_name_string("MOCK_ID_PREFIX", fields[30].c_str());
    }
    if (fields.size() > 31) {
        g.set_by_name_uint8("MOCK_FLIGHT", uint8_t(fields[31].toInt()));
    }
    if (fields.size() > 32) {
        g.set_by_name_uint8("MOCK_ALT_MODE", uint8_t(fields[32].toInt()));
    }
    g.set_by_name_string("S0_UAS", fields[0].c_str());
    g.set_by_name_string("S0_OP", fields[1].c_str());
    g.set_by_name_string("S0_SELF", fields[2].c_str());
    g.set_by_name_uint8("S0_UAT", uint8_t(fields[3].toInt()));
    g.set_by_name_uint8("S0_IDT", uint8_t(fields[4].toInt()));
    if (fields.size() > 27) {
        g.set_by_name_string("S0_MFR", fields[27].c_str());
    }
    if (fields.size() > 28) {
        g.set_by_name_string("S0_MODEL", fields[28].c_str());
    }
    g.set_by_name_uint8("S0_EN", total_uas > 0 ? 1 : 0);
    g.set_by_name_uint8("MOCK_ENABLE", 1);
    update_mock_ghost_count_compat();
    paused_status_valid = false;
    serial_send_ok();
    serial_send_data();
    serial_send_current();
}

static void serial_apply_slot(const std::vector<String> &fields)
{
    if (fields.size() < 10) {
        return;
    }
    const uint8_t slot = uint8_t(fields[0].toInt());
    if (slot >= MOCK_SLOT_COUNT) {
        serial_printf("$-\r\n");
        return;
    }

    const String idx = String(slot);
    g.set_by_name_uint8(("S" + idx + "_EN").c_str(), uint8_t(fields[1].toInt()));
    g.set_by_name_string(("S" + idx + "_MAC").c_str(), fields[2].c_str());
    g.set_by_name_string(("S" + idx + "_UAS").c_str(), fields[3].c_str());
    g.set_by_name_string(("S" + idx + "_OP").c_str(), fields[4].c_str());
    g.set_by_name_string(("S" + idx + "_SELF").c_str(), fields[5].c_str());
    g.set_by_name_string(("S" + idx + "_MFR").c_str(), fields[6].c_str());
    g.set_by_name_string(("S" + idx + "_MODEL").c_str(), fields[7].c_str());
    g.set_by_name_uint8(("S" + idx + "_UAT").c_str(), uint8_t(fields[8].toInt()));
    g.set_by_name_uint8(("S" + idx + "_IDT").c_str(), uint8_t(fields[9].toInt()));

    if (slot == 0) {
        g.set_by_name_string("UAS_ID", fields[3].c_str());
        g.set_by_name_string("MOCK_OP_ID", fields[4].c_str());
        g.set_by_name_string("MOCK_SELF_ID", fields[5].c_str());
        g.set_by_name_string("MOCK_MFR", fields[6].c_str());
        g.set_by_name_string("MOCK_MODEL", fields[7].c_str());
        g.set_by_name_uint8("UAS_TYPE", uint8_t(fields[8].toInt()));
        g.set_by_name_uint8("UAS_ID_TYPE", uint8_t(fields[9].toInt()));
    }
    update_mock_ghost_count_compat();
    serial_send_ok();
    serial_send_slot(slot);
}

static void serial_apply_mode(const std::vector<String> &fields)
{
    if (fields.size() < 2) {
        return;
    }
    serial_app_mode = uint8_t(fields[0].toInt());
    serial_fly_mode = uint8_t(fields[1].toInt());
    if (fields.size() > 2) {
        serial_path_mode = uint8_t(fields[2].toInt());
    }
    g.set_by_name_uint8("MOCK_APP_MODE", serial_app_mode);
    g.set_by_name_uint8("MOCK_FLY_MODE", serial_fly_mode);
    g.set_by_name_uint8("MOCK_PATH_MODE", serial_path_mode);
    if (fields.size() > 3) {
        g.set_by_name_string("MOCK_SPEED", fields[3].c_str());
    }
    if (serial_fly_mode == 0) {
        capture_paused_status();
        stop_rf_output();
    } else {
        paused_status_valid = false;
    }
    serial_send_ok();
    serial_send_data();
    serial_send_current();
}

static void update_button_start(void)
{
#if defined(PIN_USER_BUTTON)
    static bool last_raw = true;
    static bool stable_state = true;
    static uint32_t last_change_ms = 0;
    const bool raw = digitalRead(PIN_USER_BUTTON);
    const uint32_t now_ms = millis();
    if (raw != last_raw) {
        last_raw = raw;
        last_change_ms = now_ms;
    }
    if (raw != stable_state && now_ms - last_change_ms > 35) {
        stable_state = raw;
        if (!stable_state) {
            serial_fly_mode = serial_fly_mode ? 0 : 1;
            g.set_by_name_uint8("MOCK_FLY_MODE", serial_fly_mode);
            g.set_by_name_uint8("MOCK_ENABLE", 1);
            if (serial_fly_mode == 0) {
                capture_paused_status();
                stop_rf_output();
            } else {
                paused_status_valid = false;
            }
            serial_send_data();
            serial_send_current();
        }
    }
#endif
}

static void process_serial_command(String line)
{
    line.trim();
    if (!line.startsWith("$")) {
        return;
    }

    int delim = line.indexOf('|');
    String command = delim < 0 ? line : line.substring(0, delim);
    String payload = delim < 0 ? "" : line.substring(delim + 1);
    std::vector<String> fields;
    split_serial_fields(payload, fields);

    if (command == "$V") {
        serial_send_version();
    } else if (command == "$D") {
        serial_send_data();
    } else if (command == "$C") {
        serial_send_current();
    } else if (command == "$SD") {
        serial_apply_data(fields);
    } else if (command == "$GS") {
        if (fields.size() > 0) {
            serial_send_slot(uint8_t(fields[0].toInt()));
        }
    } else if (command == "$SS") {
        serial_apply_slot(fields);
    } else if (command == "$SM") {
        serial_apply_mode(fields);
    } else if (command == "$R") {
        ESP.restart();
    } else {
        serial_printf("$-\r\n");
    }
}

static void process_serial_compat_port(Stream &port, String &line, uint32_t &last_rx_ms)
{
    while (port.available() > 0) {
        const char c = char(port.read());
        last_rx_ms = millis();
        serial_reply_port = &port;
        if (c == '$' && line.length() > 0) {
            process_serial_command(line);
            line = "";
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            process_serial_command(line);
            line = "";
            continue;
        }
        line += c;
    }

    if (line.length() > 0 && millis() - last_rx_ms > 40) {
        serial_reply_port = &port;
        process_serial_command(line);
        line = "";
    }
}

/*
  fill in UAS_data from MAVLink packets
 */
static void set_data(Transport &t)
{
    const auto &operator_id = t.get_operator_id();
    const auto &basic_id = t.get_basic_id();
    const auto &system = t.get_system();
    const auto &self_id = t.get_self_id();
    const auto &location = t.get_location();

    odid_initUasData(&UAS_data);

    /*
      if we don't have BasicID info from parameters and we have it
      from the DroneCAN or MAVLink transport then copy it to the
      parameters to persist it. This makes it possible to set the
      UAS_ID string via a MAVLink BASIC_ID message and also offers a
      migration path from the old approach of GCS setting these values
      to having them as parameters

      BasicID 2 can be set in parameters, or provided via mavlink We
      don't persist the BasicID2 if provided via mavlink to allow
      users to change BasicID2 on different days
     */
    if (!g.have_basic_id_info() && !(g.options & OPTIONS_DONT_SAVE_BASIC_ID_TO_PARAMETERS)) {
        if (basic_id.ua_type != 0 &&
            basic_id.id_type != 0 &&
            strnlen((const char *)basic_id.uas_id, 20) > 0) {
            g.set_by_name_uint8("UAS_TYPE", basic_id.ua_type);
            g.set_by_name_uint8("UAS_ID_TYPE", basic_id.id_type);
            char uas_id[21] {};
            ODID_COPY_STR(uas_id, basic_id.uas_id);
            g.set_by_name_string("UAS_ID", uas_id);
        }
    }

    // BasicID
    if (g.have_basic_id_info() && !(g.options & OPTIONS_DONT_SAVE_BASIC_ID_TO_PARAMETERS)) {
        // from parameters
        UAS_data.BasicID[0].UAType = (ODID_uatype_t)g.ua_type;
        UAS_data.BasicID[0].IDType = (ODID_idtype_t)g.id_type;
        ODID_COPY_STR(UAS_data.BasicID[0].UASID, g.uas_id);
        UAS_data.BasicIDValid[0] = 1;

        // BasicID 2
        if (g.have_basic_id_2_info()) {
            // from parameters
            UAS_data.BasicID[1].UAType = (ODID_uatype_t)g.ua_type_2;
            UAS_data.BasicID[1].IDType = (ODID_idtype_t)g.id_type_2;
            ODID_COPY_STR(UAS_data.BasicID[1].UASID, g.uas_id_2);
            UAS_data.BasicIDValid[1] = 1;
        } else if (strcmp((const char*)g.uas_id, (const char*)basic_id.uas_id) != 0) {
            /*
              no BasicID 2 in the parameters, if one is provided on MAVLink
              and it is a different uas_id from the basicID1 then use it as BasicID2
            */
            if (basic_id.ua_type != 0 &&
                basic_id.id_type != 0 &&
                strnlen((const char *)basic_id.uas_id, 20) > 0) {
                UAS_data.BasicID[1].UAType = (ODID_uatype_t)basic_id.ua_type;
                UAS_data.BasicID[1].IDType = (ODID_idtype_t)basic_id.id_type;
                ODID_COPY_STR(UAS_data.BasicID[1].UASID, basic_id.uas_id);
                UAS_data.BasicIDValid[1] = 1;
            }
        }
    }

    if (g.options & OPTIONS_DONT_SAVE_BASIC_ID_TO_PARAMETERS) {
        if (basic_id.ua_type != 0 &&
        basic_id.id_type != 0 &&
        strnlen((const char *)basic_id.uas_id, 20) > 0) {
            if (strcmp((const char*)UAS_data.BasicID[0].UASID, (const char*)basic_id.uas_id) != 0 && strnlen((const char *)basic_id.uas_id, 20) > 0) {
                UAS_data.BasicID[1].UAType = (ODID_uatype_t)basic_id.ua_type;
                UAS_data.BasicID[1].IDType = (ODID_idtype_t)basic_id.id_type;
                ODID_COPY_STR(UAS_data.BasicID[1].UASID, basic_id.uas_id);
                UAS_data.BasicIDValid[1] = 1;
            } else {
                UAS_data.BasicID[0].UAType = (ODID_uatype_t)basic_id.ua_type;
                UAS_data.BasicID[0].IDType = (ODID_idtype_t)basic_id.id_type;
                ODID_COPY_STR(UAS_data.BasicID[0].UASID, basic_id.uas_id);
                UAS_data.BasicIDValid[0] = 1;
            }
        }
    }

    // OperatorID
    if (strlen(operator_id.operator_id) > 0) {
        UAS_data.OperatorID.OperatorIdType = (ODID_operatorIdType_t)operator_id.operator_id_type;
        ODID_COPY_STR(UAS_data.OperatorID.OperatorId, operator_id.operator_id);
        UAS_data.OperatorIDValid = 1;
    }

    // SelfID
    if (strlen(self_id.description) > 0) {
        UAS_data.SelfID.DescType = (ODID_desctype_t)self_id.description_type;
        ODID_COPY_STR(UAS_data.SelfID.Desc, self_id.description);
        UAS_data.SelfIDValid = 1;
    }

    // System
    if (system.timestamp != 0) {
        UAS_data.System.OperatorLocationType = (ODID_operator_location_type_t)system.operator_location_type;
        UAS_data.System.ClassificationType = (ODID_classification_type_t)system.classification_type;
        UAS_data.System.OperatorLatitude = system.operator_latitude * 1.0e-7;
        UAS_data.System.OperatorLongitude = system.operator_longitude * 1.0e-7;
        UAS_data.System.AreaCount = system.area_count;
        UAS_data.System.AreaRadius = system.area_radius;
        UAS_data.System.AreaCeiling = system.area_ceiling;
        UAS_data.System.AreaFloor = system.area_floor;
        UAS_data.System.CategoryEU = (ODID_category_EU_t)system.category_eu;
        UAS_data.System.ClassEU = (ODID_class_EU_t)system.class_eu;
        UAS_data.System.OperatorAltitudeGeo = system.operator_altitude_geo;
        UAS_data.System.Timestamp = system.timestamp;
        UAS_data.SystemValid = 1;
    }

    // Location
    if (location.timestamp != 0) {
        UAS_data.Location.Status = (ODID_status_t)location.status;
        UAS_data.Location.Direction = location.direction*0.01;
        UAS_data.Location.SpeedHorizontal = location.speed_horizontal*0.01;
        UAS_data.Location.SpeedVertical = location.speed_vertical*0.01;
        UAS_data.Location.Latitude = location.latitude*1.0e-7;
        UAS_data.Location.Longitude = location.longitude*1.0e-7;
        UAS_data.Location.AltitudeBaro = location.altitude_barometric;
        UAS_data.Location.AltitudeGeo = location.altitude_geodetic;
        UAS_data.Location.HeightType = (ODID_Height_reference_t)location.height_reference;
        UAS_data.Location.Height = location.height;
        UAS_data.Location.HorizAccuracy = (ODID_Horizontal_accuracy_t)location.horizontal_accuracy;
        UAS_data.Location.VertAccuracy = (ODID_Vertical_accuracy_t)location.vertical_accuracy;
        UAS_data.Location.BaroAccuracy = (ODID_Vertical_accuracy_t)location.barometer_accuracy;
        UAS_data.Location.SpeedAccuracy = (ODID_Speed_accuracy_t)location.speed_accuracy;
        UAS_data.Location.TSAccuracy = (ODID_Timestamp_accuracy_t)location.timestamp_accuracy;
        UAS_data.Location.TimeStamp = location.timestamp;
        UAS_data.LocationValid = 1;
    }

    const char *reason = check_parse();
    t.arm_status_check(reason);
    t.set_parse_fail(reason);

    arm_check_ok = (reason==nullptr);

    if (g.options & OPTIONS_FORCE_ARM_OK) {
        arm_check_ok = true;
    }

    led.set_state(pfst_check_ok && arm_check_ok? Led::LedState::ARM_OK : Led::LedState::ARM_FAIL);

    uint32_t now_ms = millis();
    uint32_t location_age_ms = now_ms - t.get_last_location_ms();
    uint32_t last_location_age_ms = now_ms - last_location_ms;
    if (location_age_ms < last_location_age_ms) {
        last_location_ms = t.get_last_location_ms();
    }
}

static uint8_t loop_counter = 0;

void loop()
{
    process_serial_compat_port(Serial, serial_line, serial_last_rx_ms);
    process_serial_compat_port(Serial1, serial1_line, serial1_last_rx_ms);
    update_button_start();

#if AP_MAVLINK_ENABLED
    if (!g.mock_enable) {
        mavlink1.update();
    }
#endif
#if AP_DRONECAN_ENABLED
    dronecan.update();
#endif

    const uint32_t now_ms = millis();

    // the transports have common static data, so we can just use the
    // first for status
#if AP_MAVLINK_ENABLED
    auto &transport = mavlink1;
#elif AP_DRONECAN_ENABLED
    auto &transport = dronecan;
#else
    #error "Must enable DroneCAN or MAVLink"
#endif

    const uint32_t last_location_ms = transport.get_last_location_ms();
    const uint32_t last_system_ms = transport.get_last_system_ms();

    led.update();

    status_reason = "";

    const bool stale_location = (last_location_ms == 0 || now_ms - last_location_ms > 5000);
    const bool stale_system = (last_system_ms == 0 || now_ms - last_system_ms > 5000);
    const bool parse_failed = (transport.get_parse_fail() != nullptr);
    mock_rid_active = g.mock_enable && (stale_location || stale_system || parse_failed);

    if (mock_rid_active) {
        mock_rid_slots = mock_slot_count();
        if (mock_rid_slots > 0) {
            const uint8_t enabled_index = (millis() / uint32_t(mock_slot_hold_ms())) % mock_rid_slots;
            mock_rid_slot = resolve_enabled_slot(enabled_index);
            fill_mock_uas_data(mock_rid_slot, mock_rid_slots);
            uint8_t slot_mac[6] {};
            get_slot_mac(mock_rid_slot, slot_mac);
            ble.set_active_mac(slot_mac);
            wifi.set_active_mac(slot_mac);
            status_reason = "mock rid " + String(unsigned(mock_rid_slot + 1)) + "/" + String(unsigned(mock_rid_slots));
        } else {
            mock_rid_slot = 0;
            stop_rf_output();
            status_reason = "mock rid idle";
        }
    } else {
        mock_rid_slot = 0;
        mock_rid_slots = 1;
        set_data(transport);
        if (transport.get_parse_fail() != nullptr) {
            UAS_data.Location.Status = ODID_STATUS_REMOTE_ID_SYSTEM_FAILURE;
            status_reason = String(transport.get_parse_fail());
        }
    }

    static int last_fly_mode = -1;
    if (last_fly_mode != int(serial_fly_mode)) {
        if (serial_fly_mode == 0) {
            capture_paused_status();
            stop_rf_output();
        } else {
            paused_status_valid = false;
        }
        last_fly_mode = int(serial_fly_mode);
    }

    if (g.webserver_enable) {
        webif.update();
    }

    static uint32_t last_update_wifi_nan_ms;
    if (serial_fly_mode == 1 &&
        (!mock_rid_active || mock_rid_slots > 0) &&
        g.wifi_nan_rate > 0 &&
        now_ms - last_update_wifi_nan_ms > 1000/g.wifi_nan_rate) {
        last_update_wifi_nan_ms = now_ms;
        wifi.transmit_nan(UAS_data);
    }

    static uint32_t last_update_wifi_beacon_ms;
    if (serial_fly_mode == 1 &&
        (!mock_rid_active || mock_rid_slots > 0) &&
        g.wifi_beacon_rate > 0 &&
        now_ms - last_update_wifi_beacon_ms > 1000/g.wifi_beacon_rate) {
        last_update_wifi_beacon_ms = now_ms;
        wifi.transmit_beacon(UAS_data);
    }

    static uint32_t last_update_bt5_ms;
    if (serial_fly_mode == 1 &&
        (!mock_rid_active || mock_rid_slots > 0) &&
        g.bt5_rate > 0 &&
        now_ms - last_update_bt5_ms > 1000/g.bt5_rate) {
        last_update_bt5_ms = now_ms;
        ble.transmit_longrange(UAS_data);
    }

    static uint32_t last_update_bt4_ms;
    int bt4_states = UAS_data.BasicIDValid[1] ? 7 : 6;
    if (serial_fly_mode == 1 &&
        (!mock_rid_active || mock_rid_slots > 0) &&
        g.bt4_rate > 0 &&
        now_ms - last_update_bt4_ms > (1000.0f/bt4_states)/g.bt4_rate) {
        last_update_bt4_ms = now_ms;
        ble.transmit_legacy(UAS_data);
    }

    if (mock_rid_active && mock_rid_slots > 0) {
        serial_push_status();
    }

    // sleep for a bit for power saving
    delay(1);
}
