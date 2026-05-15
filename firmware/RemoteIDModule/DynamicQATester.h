#pragma once

#include <Arduino.h>
#include <opendroneid.h>

class DynamicQATester {
public:
    struct Payload {
        ODID_UAS_Data uas_data;
        uint8_t mac[6];
        uint8_t active_slot;
        uint8_t active_slots;
        bool wifi_mac_applied;
        char status_text[40];
    };

    void init(void);
    bool enabled(void) const;
    uint8_t active_slot_count(void) const;
    bool build_payload(uint8_t slot, uint8_t slot_count, uint32_t now_ms, Payload &payload) const;
    bool get_next_payload(Payload &payload, uint32_t now_ms);
    bool apply_wifi_mac_override(const uint8_t mac[6]);

private:
    static constexpr uint8_t kMaxSlots = 5;
    bool last_wifi_mac_valid = false;
    uint8_t last_wifi_mac[6] {};

    static uint8_t clamp_slot_count(uint8_t slot_count);
    static uint8_t sanitize_heading_mode(uint8_t heading_mode);
    static float timestamp_seconds_of_hour(void);
    static uint32_t rid_system_timestamp(void);
    static bool parse_laa_mac(const char *text, uint8_t mac[6]);
    static void make_default_laa(uint8_t mac[6]);
    static void derive_slot_mac(uint8_t slot, uint8_t mac[6]);
    static void normalise_uas_seed(const char *seed, char out[ODID_ID_SIZE + 1]);
    static void make_slot_uas_id(uint8_t slot, char out[ODID_ID_SIZE + 1]);
    static void make_self_id(uint8_t slot, char out[ODID_STR_SIZE + 1]);
    static float compute_heading_deg(uint8_t mode,
                                     float north_m,
                                     float east_m,
                                     float next_north_m,
                                     float next_east_m);
    static void fill_payload_for_slot(Payload &payload, uint8_t slot, uint8_t slot_count, uint32_t now_ms);
};
