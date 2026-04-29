#include "options.h"
#include <Arduino.h>
#include "parameters.h"
#include <nvs_flash.h>
#include <string.h>
#include "romfs.h"
#include "util.h"

Parameters g;
static nvs_handle handle;

const Parameters::Param Parameters::params[] = {
    { "LOCK_LEVEL",        Parameters::ParamType::INT8,  (const void*)&g.lock_level,       0, -1, 2 },
    { "CAN_NODE",          Parameters::ParamType::UINT8,  (const void*)&g.can_node,         0, 0, 127 },
#if defined(PIN_CAN_TERM)
    { "CAN_TERMINATE",     Parameters::ParamType::UINT8,  (const void*)&g.can_term,        0, 0, 1 },
#endif
    { "UAS_TYPE",          Parameters::ParamType::UINT8,  (const void*)&g.ua_type,          0, 0, 15 },
    { "UAS_ID_TYPE",       Parameters::ParamType::UINT8,  (const void*)&g.id_type,          0, 0, 4 },
    { "UAS_ID",            Parameters::ParamType::CHAR20, (const void*)&g.uas_id[0],        0, 0, 0 },
    { "UAS_TYPE_2",        Parameters::ParamType::UINT8,  (const void*)&g.ua_type_2,          0, 0, 15 },
    { "UAS_ID_TYPE_2",     Parameters::ParamType::UINT8,  (const void*)&g.id_type_2,          0, 0, 4 },
    { "UAS_ID_2",          Parameters::ParamType::CHAR20, (const void*)&g.uas_id_2[0],        0, 0, 0 },
    { "BAUDRATE",          Parameters::ParamType::UINT32, (const void*)&g.baudrate,         57600, 9600, 921600 },
    { "WIFI_NAN_RATE",     Parameters::ParamType::FLOAT,  (const void*)&g.wifi_nan_rate,    0, 0, 5 },
    { "WIFI_BCN_RATE",     Parameters::ParamType::FLOAT,  (const void*)&g.wifi_beacon_rate,    0, 0, 5 },
    { "WIFI_POWER",        Parameters::ParamType::FLOAT,  (const void*)&g.wifi_power,       20, 2, 20 },
    { "BT4_RATE",          Parameters::ParamType::FLOAT,  (const void*)&g.bt4_rate,         1, 0, 5 },
    { "BT4_POWER",         Parameters::ParamType::FLOAT,  (const void*)&g.bt4_power,        18, -27, 18 },
    { "BT5_RATE",          Parameters::ParamType::FLOAT,  (const void*)&g.bt5_rate,         1, 0, 5 },
    { "BT5_POWER",         Parameters::ParamType::FLOAT,  (const void*)&g.bt5_power,        18, -27, 18 },
    { "WEBSERVER_EN",      Parameters::ParamType::UINT8,  (const void*)&g.webserver_enable, 1, 0, 1 },
    { "WIFI_SSID",         Parameters::ParamType::CHAR20, (const void*)&g.wifi_ssid, },
    { "WIFI_PASSWORD",     Parameters::ParamType::CHAR20, (const void*)&g.wifi_password,    0, 0, 0, PARAM_FLAG_PASSWORD, 8 },
    { "WIFI_CHANNEL",      Parameters::ParamType::UINT8,  (const void*)&g.wifi_channel,    6, 1, 13 },
    { "BCAST_POWERUP",     Parameters::ParamType::UINT8,  (const void*)&g.bcast_powerup,    1, 0, 1 },
    { "MOCK_ENABLE",       Parameters::ParamType::UINT8,  (const void*)&g.mock_enable,      1, 0, 1 },
    { "MOCK_HOME_LAT",     Parameters::ParamType::FLOAT,  (const void*)&g.mock_home_lat,    59.4370f, -90, 90 },
    { "MOCK_HOME_LON",     Parameters::ParamType::FLOAT,  (const void*)&g.mock_home_lon,    24.7536f, -180, 180 },
    { "MOCK_GHOSTS",       Parameters::ParamType::UINT8,  (const void*)&g.mock_ghost_count, 0, 0, 4 },
    { "MOCK_RADIUS",       Parameters::ParamType::FLOAT,  (const void*)&g.mock_ring_radius, 120.0f, 10, 3000 },
    { "MOCK_SPEED",        Parameters::ParamType::FLOAT,  (const void*)&g.mock_speed,       6.0f, 0.5f, 50.0f },
    { "MOCK_OP_ID",        Parameters::ParamType::CHAR20, (const void*)&g.mock_operator_id, 0, 0, 0 },
    { "MOCK_SELF_ID",      Parameters::ParamType::CHAR20, (const void*)&g.mock_self_id,     0, 0, 0 },
    { "MOCK_MFR",          Parameters::ParamType::CHAR20, (const void*)&g.mock_manufacturer, 0, 0, 0 },
    { "MOCK_MODEL",        Parameters::ParamType::CHAR20, (const void*)&g.mock_model,        0, 0, 0 },
    { "MOCK_ID_SCHEME",    Parameters::ParamType::UINT8,  (const void*)&g.mock_id_scheme,    0, 0, 2 },
    { "MOCK_ID_PREFIX",    Parameters::ParamType::CHAR20, (const void*)&g.mock_id_prefix,    0, 0, 0 },
    { "MOCK_FLIGHT",       Parameters::ParamType::UINT8,  (const void*)&g.mock_flight_mode,  0, 0, 2 },
    { "MOCK_ALT_MODE",     Parameters::ParamType::UINT8,  (const void*)&g.mock_altitude_mode, 0, 0, 2 },
    { "MOCK_APP_MODE",     Parameters::ParamType::UINT8,  (const void*)&g.mock_app_mode,     1, 0, 2 },
    { "MOCK_FLY_MODE",     Parameters::ParamType::UINT8,  (const void*)&g.mock_fly_mode,     0, 0, 1 },
    { "MOCK_PATH_MODE",    Parameters::ParamType::UINT8,  (const void*)&g.mock_path_mode,    0, 0, 2 },
    { "S0_EN",             Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_enabled[0], 1, 0, 1 },
    { "S0_MAC",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_mac[0][0], 0, 0, 0 },
    { "S0_UAS",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_uas_id[0][0], 0, 0, 0 },
    { "S0_OP",             Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_operator_id[0][0], 0, 0, 0 },
    { "S0_SELF",           Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_self_id[0][0], 0, 0, 0 },
    { "S0_MFR",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_manufacturer[0][0], 0, 0, 0 },
    { "S0_MODEL",          Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_model[0][0], 0, 0, 0 },
    { "S0_UAT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_ua_type[0], 0, 0, 15 },
    { "S0_IDT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_id_type[0], 0, 0, 4 },
    { "S1_EN",             Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_enabled[1], 0, 0, 1 },
    { "S1_MAC",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_mac[1][0], 0, 0, 0 },
    { "S1_UAS",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_uas_id[1][0], 0, 0, 0 },
    { "S1_OP",             Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_operator_id[1][0], 0, 0, 0 },
    { "S1_SELF",           Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_self_id[1][0], 0, 0, 0 },
    { "S1_MFR",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_manufacturer[1][0], 0, 0, 0 },
    { "S1_MODEL",          Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_model[1][0], 0, 0, 0 },
    { "S1_UAT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_ua_type[1], 0, 0, 15 },
    { "S1_IDT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_id_type[1], 0, 0, 4 },
    { "S2_EN",             Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_enabled[2], 0, 0, 1 },
    { "S2_MAC",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_mac[2][0], 0, 0, 0 },
    { "S2_UAS",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_uas_id[2][0], 0, 0, 0 },
    { "S2_OP",             Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_operator_id[2][0], 0, 0, 0 },
    { "S2_SELF",           Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_self_id[2][0], 0, 0, 0 },
    { "S2_MFR",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_manufacturer[2][0], 0, 0, 0 },
    { "S2_MODEL",          Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_model[2][0], 0, 0, 0 },
    { "S2_UAT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_ua_type[2], 0, 0, 15 },
    { "S2_IDT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_id_type[2], 0, 0, 4 },
    { "S3_EN",             Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_enabled[3], 0, 0, 1 },
    { "S3_MAC",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_mac[3][0], 0, 0, 0 },
    { "S3_UAS",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_uas_id[3][0], 0, 0, 0 },
    { "S3_OP",             Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_operator_id[3][0], 0, 0, 0 },
    { "S3_SELF",           Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_self_id[3][0], 0, 0, 0 },
    { "S3_MFR",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_manufacturer[3][0], 0, 0, 0 },
    { "S3_MODEL",          Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_model[3][0], 0, 0, 0 },
    { "S3_UAT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_ua_type[3], 0, 0, 15 },
    { "S3_IDT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_id_type[3], 0, 0, 4 },
    { "S4_EN",             Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_enabled[4], 0, 0, 1 },
    { "S4_MAC",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_mac[4][0], 0, 0, 0 },
    { "S4_UAS",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_uas_id[4][0], 0, 0, 0 },
    { "S4_OP",             Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_operator_id[4][0], 0, 0, 0 },
    { "S4_SELF",           Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_self_id[4][0], 0, 0, 0 },
    { "S4_MFR",            Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_manufacturer[4][0], 0, 0, 0 },
    { "S4_MODEL",          Parameters::ParamType::CHAR20, (const void*)&g.mock_slot_model[4][0], 0, 0, 0 },
    { "S4_UAT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_ua_type[4], 0, 0, 15 },
    { "S4_IDT",            Parameters::ParamType::UINT8,  (const void*)&g.mock_slot_id_type[4], 0, 0, 4 },
    { "PUBLIC_KEY1",       Parameters::ParamType::CHAR64, (const void*)&g.public_keys[0], },
    { "PUBLIC_KEY2",       Parameters::ParamType::CHAR64, (const void*)&g.public_keys[1], },
    { "PUBLIC_KEY3",       Parameters::ParamType::CHAR64, (const void*)&g.public_keys[2], },
    { "PUBLIC_KEY4",       Parameters::ParamType::CHAR64, (const void*)&g.public_keys[3], },
    { "PUBLIC_KEY5",       Parameters::ParamType::CHAR64, (const void*)&g.public_keys[4], },
    { "MAVLINK_SYSID",     Parameters::ParamType::UINT8,  (const void*)&g.mavlink_sysid,    0, 0, 254 },
    { "OPTIONS",           Parameters::ParamType::UINT8,  (const void*)&g.options,          0, 0, 254 },
    { "TO_DEFAULTS",     Parameters::ParamType::UINT8,  (const void*)&g.to_factory_defaults,    0, 0, 1 }, //if set to 1, reset to factory defaults and make 0.
    { "DONE_INIT",         Parameters::ParamType::UINT8,  (const void*)&g.done_init,        0, 0, 0, PARAM_FLAG_HIDDEN},
    { "",                  Parameters::ParamType::NONE,   nullptr,  },
};

/*
  get count of parameters capable of being converted to load
 */
uint16_t Parameters::param_count_float(void)
{
    uint16_t count = 0;
    for (const auto &p : params) {
        if (p.flags & PARAM_FLAG_HIDDEN) {
            continue;
        }
        switch (p.ptype) {
        case ParamType::UINT8:
        case ParamType::INT8:
        case ParamType::UINT32:
        case ParamType::FLOAT:
            count++;
            break;
        }
    }
    // remove 1 for DONE_INIT
    return count-1;
}

/*
  get index of parameter counting only those capable of representation as float
 */
int16_t Parameters::param_index_float(const Parameters::Param *f)
{
    uint16_t count = 0;
    for (const auto &p : params) {
        if (p.flags & PARAM_FLAG_HIDDEN) {
            continue;
        }
        switch (p.ptype) {
        case ParamType::UINT8:
        case ParamType::INT8:
        case ParamType::UINT32:
        case ParamType::FLOAT:
            if (&p == f) {
                return count;
            }
            count++;
            break;
        }
    }
    return -1;
}

/*
  find by name
 */
const Parameters::Param *Parameters::find(const char *name)
{
    for (const auto &p : params) {
        if (strcmp(name, p.name) == 0) {
            return &p;
        }
    }
    return nullptr;
}

/*
  find by index
 */
const Parameters::Param *Parameters::find_by_index(uint16_t index)
{
    if (index >= ARRAY_SIZE(params)-2) {
        return nullptr;
    }
    return &params[index];
}

/*
  find by index
 */
const Parameters::Param *Parameters::find_by_index_float(uint16_t index)
{
    uint16_t count = 0;
    for (const auto &p : params) {
        if (p.flags & PARAM_FLAG_HIDDEN) {
            continue;
        }
        switch (p.ptype) {
        case ParamType::UINT8:
        case ParamType::INT8:
        case ParamType::UINT32:
        case ParamType::FLOAT:
            if (index == count) {
                return &p;
            }
            count++;
            break;
        }
    }
    return nullptr;
}

void Parameters::Param::set_uint8(uint8_t v) const
{
    auto *p = (uint8_t *)ptr;
    *p = v;
    nvs_set_u8(handle, name, *p);
    if (strcmp(name, "TO_DEFAULTS") == 0) {
        if (v == 1) {
            nvs_flash_erase();
            esp_restart();
        }
    }
}

void Parameters::Param::set_int8(int8_t v) const
{
    auto *p = (int8_t *)ptr;
    *p = v;
    nvs_set_i8(handle, name, *p);
}

void Parameters::Param::set_uint32(uint32_t v) const
{
    auto *p = (uint32_t *)ptr;
    *p = v;
    nvs_set_u32(handle, name, *p);
}

void Parameters::Param::set_float(float v) const
{
    auto *p = (float *)ptr;
    *p = v;
    union {
        float f;
        uint32_t u32;
    } u;
    u.f = v;
    nvs_set_u32(handle, name, u.u32);
}

void Parameters::Param::set_char20(const char *v) const
{
    if (min_len > 0 && strlen(v) < min_len) {
        return;
    }
    memset((void*)ptr, 0, 21);
    strncpy((char *)ptr, v, 20);
    nvs_set_str(handle, name, v);
}

void Parameters::Param::set_char64(const char *v) const
{
    if (min_len > 0 && strlen(v) < min_len) {
        return;
    }
    memset((void*)ptr, 0, 65);
    strncpy((char *)ptr, v, 64);
    nvs_set_str(handle, name, v);
}

uint8_t Parameters::Param::get_uint8() const
{
    const auto *p = (const uint8_t *)ptr;
    return *p;
}

int8_t Parameters::Param::get_int8() const
{
    const auto *p = (const int8_t *)ptr;
    return *p;
}

uint32_t Parameters::Param::get_uint32() const
{
    const auto *p = (const uint32_t *)ptr;
    return *p;
}

float Parameters::Param::get_float() const
{
    const auto *p = (const float *)ptr;
    return *p;
}

const char *Parameters::Param::get_char20() const
{
    const char *p = (const char *)ptr;
    return p;
}

const char *Parameters::Param::get_char64() const
{
    const char *p = (const char *)ptr;
    return p;
}

/*
  get parameter as a float
 */
bool Parameters::Param::get_as_float(float &v) const
{
    switch (ptype) {
        case ParamType::UINT8:
            v = float(get_uint8());
            break;
        case ParamType::INT8:
            v = float(get_int8());
            break;
        case ParamType::UINT32:
            v = float(get_uint32());
            break;
        case ParamType::FLOAT:
            v = get_float();
            break;
    default:
        return false;
    }
    return true;
}

/*
  set parameter from a float
 */
void Parameters::Param::set_as_float(float v) const
{
    switch (ptype) {
        case ParamType::UINT8:
            set_uint8(uint8_t(v));
            break;
        case ParamType::INT8:
            set_int8(int8_t(v));
            break;
        case ParamType::UINT32:
            set_uint32(uint32_t(v));
            break;
        case ParamType::FLOAT:
            set_float(v);
            break;
    }
}


/*
  load defaults from parameter table
 */
void Parameters::load_defaults(void)
{
    for (const auto &p : params) {
        switch (p.ptype) {
        case ParamType::UINT8:
            *(uint8_t *)p.ptr = uint8_t(p.default_value);
            break;
        case ParamType::INT8:
            *(int8_t *)p.ptr = int8_t(p.default_value);
            break;
        case ParamType::UINT32:
            *(uint32_t *)p.ptr = uint32_t(p.default_value);
            break;
        case ParamType::FLOAT:
            *(float *)p.ptr = p.default_value;
            break;
        }
    }
}

void Parameters::init(void)
{
    load_defaults();

    if (nvs_flash_init() != ESP_OK ||
        nvs_open("storage", NVS_READWRITE, &handle) != ESP_OK) {
        Serial.printf("NVS init failed\n");
    }
    // load values from NVS
    for (const auto &p : params) {
        switch (p.ptype) {
        case ParamType::UINT8:
            nvs_get_u8(handle, p.name, (uint8_t *)p.ptr);
            break;
        case ParamType::INT8:
            nvs_get_i8(handle, p.name, (int8_t *)p.ptr);
            break;
        case ParamType::UINT32:
            nvs_get_u32(handle, p.name, (uint32_t *)p.ptr);
            break;
        case ParamType::FLOAT:
            nvs_get_u32(handle, p.name, (uint32_t *)p.ptr);
            break;
        case ParamType::CHAR20: {
            size_t len = 21;
            nvs_get_str(handle, p.name, (char *)p.ptr, &len);
            break;
        }
        case ParamType::CHAR64: {
            size_t len = 65;
            nvs_get_str(handle, p.name, (char *)p.ptr, &len);
            break;
        }
        }
    }

    if (strlen(g.wifi_ssid) == 0) {
        uint8_t mac[6] {};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(wifi_ssid, 20, "RID_%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    }
    if (g.to_factory_defaults == 1) {
        //should not happen, but in case the parameter is still set to 1, erase flash and reboot
        nvs_flash_erase();
        esp_restart();
    }

    if (g.done_init == 0) {
        set_by_name_uint8("DONE_INIT", 1);
        // setup public keys
        set_by_name_char64("PUBLIC_KEY1", ROMFS::find_string("public_keys/ArduPilot_public_key1.dat"));
        set_by_name_char64("PUBLIC_KEY2", ROMFS::find_string("public_keys/ArduPilot_public_key2.dat"));
#if defined(BOARD_BLUEMARK_DB200) || defined(BOARD_BLUEMARK_DB110) || defined(BOARD_BLUEMARK_DB202) || defined(BOARD_BLUEMARK_DB210) || defined(BOARD_BLUEMARK_DB203)
        set_by_name_char64("PUBLIC_KEY3", ROMFS::find_string("public_keys/BlueMark_public_key1.dat"));
#elif defined(BOARD_CUAV_RID)
        set_by_name_char64("PUBLIC_KEY3", ROMFS::find_string("public_keys/CUAV_public_key1.dat"));
#else
        set_by_name_char64("PUBLIC_KEY3", ROMFS::find_string("public_keys/ArduPilot_public_key3.dat"));
#endif

    }
}

/*
  check if BasicID info is filled in with parameters
 */
bool Parameters::have_basic_id_info(void) const
{
    return strlen(g.uas_id) > 0 && g.id_type > 0 && g.ua_type > 0;
}

bool Parameters::have_basic_id_2_info(void) const
{
    return strlen(g.uas_id_2) > 0 && g.id_type_2 > 0 && g.ua_type_2 > 0;
}

bool Parameters::set_by_name_uint8(const char *name, uint8_t v)
{
    const auto *f = find(name);
    if (!f) {
        return false;
    }
    f->set_uint8(v);
    return true;
}

bool Parameters::set_by_name_int8(const char *name, int8_t v)
{
    const auto *f = find(name);
    if (!f) {
        return false;
    }
    f->set_int8(v);
    return true;
}

bool Parameters::set_by_name_char64(const char *name, const char *s)
{
    const auto *f = find(name);
    if (!f) {
        return false;
    }
    f->set_char64(s);
    return true;
}

bool Parameters::set_by_name_string(const char *name, const char *s)
{
    const auto *f = find(name);
    if (!f) {
        return false;
    }
    switch (f->ptype) {
        case ParamType::UINT8:
            f->set_uint8(uint8_t(strtoul(s, nullptr, 0)));
            return true;
        case ParamType::INT8:
            f->set_int8(int8_t(strtoul(s, nullptr, 0)));
            return true;
        case ParamType::UINT32:
            f->set_uint32(strtoul(s, nullptr, 0));
            return true;
        case ParamType::FLOAT:
            f->set_float(atof(s));
            return true;
        case ParamType::CHAR20:
            f->set_char20(s);
            return true;
        case ParamType::CHAR64:
            f->set_char64(s);
            return true;
    }
    return false;
}

/*
  return a public key
 */
bool Parameters::get_public_key(uint8_t i, uint8_t key[32]) const
{
    if (i >= MAX_PUBLIC_KEYS) {
        return false;
    }
    const char *b64_key = g.public_keys[i].b64_key;
    const char *ktype = "PUBLIC_KEYV1:";
    if (strncmp(b64_key, ktype, strlen(ktype)) != 0) {
        return false;
    }
    b64_key += strlen(ktype);
    int32_t out_len = base64_decode(b64_key, key, 32);
    if (out_len != 32) {
        return false;
    }
    return true;
}

bool Parameters::no_public_keys(void) const
{
    for (uint8_t i=0; i<MAX_PUBLIC_KEYS; i++) {
        uint8_t key[32];
        if (get_public_key(i, key)) {
            return false;
        }
    }
    return true;
}

bool Parameters::set_public_key(uint8_t i, const uint8_t key[32])
{
    if (i >= MAX_PUBLIC_KEYS) {
        return false;
    }
    char *s = base64_encode(key, PUBLIC_KEY_LEN);
    if (s == nullptr) {
        return false;
    }
    char name[] = "PUBLIC_KEYx";
    name[strlen(name)-2] = '1'+i;
    bool ret = set_by_name_char64(name, s);
    delete[] s;
    return ret;
}

bool Parameters::remove_public_key(uint8_t i)
{
    if (i >= MAX_PUBLIC_KEYS) {
        return false;
    }
    char name[] = "PUBLIC_KEYx";
    name[strlen(name)-2] = '1'+i;
    return set_by_name_char64(name, "");
}
