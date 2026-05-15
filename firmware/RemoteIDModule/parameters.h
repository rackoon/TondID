#pragma once

#include <stdint.h>
#include "board_config.h"

#define MAX_PUBLIC_KEYS 5
#define PUBLIC_KEY_LEN 32
#define PARAM_NAME_MAX_LEN 16
#define MOCK_SLOT_COUNT 5

#define PARAM_FLAG_NONE 0
#define PARAM_FLAG_PASSWORD (1U<<0)
#define PARAM_FLAG_HIDDEN (1U<<1)

class Parameters {
public:
#if defined(PIN_CAN_TERM)
    uint8_t can_term = !CAN_TERM_EN;
#endif
    int8_t lock_level;
    uint8_t can_node;
    uint8_t bcast_powerup;
    uint32_t baudrate = 57600;
    uint8_t ua_type;
    uint8_t id_type;
    char uas_id[21] = "ABCD123456789";
    uint8_t ua_type_2;
    uint8_t id_type_2;
    char uas_id_2[21] = "ABCD123456789";
    float wifi_nan_rate;
    float wifi_beacon_rate;
    float wifi_power;
    float bt4_rate;
    float bt4_power;
    float bt5_rate;
    float bt5_power;
    uint8_t done_init;
    uint8_t webserver_enable;
    uint8_t mavlink_sysid;
    char wifi_ssid[21] = "";
    char wifi_password[21] = "ArduRemoteID";
    uint8_t wifi_channel = 6;
    uint8_t to_factory_defaults = 0;
    uint8_t options;
    uint8_t mock_enable = 1;
    float mock_home_lat = 59.4370f;
    float mock_home_lon = 24.7536f;
    uint8_t mock_ghost_count = 0;
    float mock_ring_radius = 120.0f;
    float mock_speed = 6.0f;
    char mock_operator_id[21] = "EST0000000000001";
    char mock_self_id[21] = "QA TEST FLIGHT";
    char mock_manufacturer[21] = "ARUPILOT";
    char mock_model[21] = "RID-MOCK";
    uint8_t mock_id_scheme = 0;
    char mock_id_prefix[21] = "";
    uint8_t mock_flight_mode = 0;
    uint8_t mock_altitude_mode = 0;
    uint8_t mock_app_mode = 1;
    uint8_t mock_fly_mode = 0;
    uint8_t mock_path_mode = 0;
    uint8_t mock_slot_enabled[MOCK_SLOT_COUNT] = {1, 0, 0, 0, 0};
    char mock_slot_mac[MOCK_SLOT_COUNT][21] = {};
    char mock_slot_uas_id[MOCK_SLOT_COUNT][21] = {};
    char mock_slot_operator_id[MOCK_SLOT_COUNT][21] = {};
    char mock_slot_self_id[MOCK_SLOT_COUNT][21] = {};
    char mock_slot_manufacturer[MOCK_SLOT_COUNT][21] = {};
    char mock_slot_model[MOCK_SLOT_COUNT][21] = {};
    uint8_t mock_slot_ua_type[MOCK_SLOT_COUNT] = {};
    uint8_t mock_slot_id_type[MOCK_SLOT_COUNT] = {};
    uint8_t qa_mode_enabled = 0;
    char qa_uas_id_seed[21] = "QA00000000000000001";
    float qa_home_lat = 59.4370f;
    float qa_home_lon = 24.7536f;
    float qa_alt_m = 65.0f;
    float qa_radius_m = 120.0f;
    float qa_speed_mps = 6.0f;
    uint8_t qa_heading_mode = 0;
    uint8_t qa_slot_count = 1;
    char qa_lab_label[21] = "QA LAB";
    char qa_lab_mac_override[21] = "";
    struct {
        char b64_key[64];
    } public_keys[MAX_PUBLIC_KEYS];

    enum class ParamType {
        NONE=0,
        UINT8=1,
        UINT32=2,
        FLOAT=3,
        CHAR20=4,
        CHAR64=5,
        INT8=6,
    };

    struct Param {
        char name[PARAM_NAME_MAX_LEN+1];
        ParamType ptype;
        const void *ptr;
        float default_value;
        float min_value;
        float max_value;
        uint16_t flags;
        uint8_t min_len;
        void set_float(float v) const;
        void set_uint8(uint8_t v) const;
        void set_int8(int8_t v) const;
        void set_uint32(uint32_t v) const;
        void set_char20(const char *v) const;
        void set_char64(const char *v) const;
        uint8_t get_uint8() const;
        int8_t get_int8() const;
        uint32_t get_uint32() const;
        float get_float() const;
        const char *get_char20() const;
        const char *get_char64() const;
        bool get_as_float(float &v) const;
        void set_as_float(float v) const;
    };
    static const struct Param params[];

    static const Param *find(const char *name);
    static const Param *find_by_index(uint16_t idx);
    static const Param *find_by_index_float(uint16_t idx);

    void init(void);

    bool have_basic_id_info(void) const;
    bool have_basic_id_2_info(void) const;

    bool set_by_name_uint8(const char *name, uint8_t v);
    bool set_by_name_int8(const char *name, int8_t v);
    bool set_by_name_char64(const char *name, const char *s);
    bool set_by_name_string(const char *name, const char *s);

    /*
      return a public key
    */
    bool get_public_key(uint8_t i, uint8_t key[32]) const;
    bool set_public_key(uint8_t i, const uint8_t key[32]);
    bool remove_public_key(uint8_t i);
    bool no_public_keys(void) const;

    static uint16_t param_count_float(void);
    static int16_t param_index_float(const Param *p);

private:
    void load_defaults(void);
};

// bits for OPTIONS parameter
#define OPTIONS_FORCE_ARM_OK (1U<<0)
#define OPTIONS_DONT_SAVE_BASIC_ID_TO_PARAMETERS (1U<<1)
#define OPTIONS_PRINT_RID_MAVLINK (1U<<2)

extern Parameters g;
