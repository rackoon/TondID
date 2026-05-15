#include "DynamicQATester.h"

#include <ctype.h>
#include <esp_wifi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "parameters.h"

static constexpr float kMetersToLat = 1.0f / 111111.0f;
static constexpr uint32_t kRidEpoch = 1546300800UL;

void DynamicQATester::init(void)
{
    last_wifi_mac_valid = false;
    memset(last_wifi_mac, 0, sizeof(last_wifi_mac));
}

bool DynamicQATester::enabled(void) const
{
    return g.qa_mode_enabled != 0;
}

uint8_t DynamicQATester::active_slot_count(void) const
{
    return clamp_slot_count(g.qa_slot_count);
}

bool DynamicQATester::build_payload(uint8_t slot, uint8_t slot_count, uint32_t now_ms, Payload &payload) const
{
    if (!enabled()) {
        return false;
    }
    const uint8_t slots = clamp_slot_count(slot_count);
    if (slot >= slots) {
        return false;
    }
    memset(&payload, 0, sizeof(payload));
    fill_payload_for_slot(payload, slot, slots, now_ms);
    return true;
}

uint8_t DynamicQATester::clamp_slot_count(uint8_t slot_count)
{
    if (slot_count < 1) {
        return 1;
    }
    if (slot_count > kMaxSlots) {
        return kMaxSlots;
    }
    return slot_count;
}

uint8_t DynamicQATester::sanitize_heading_mode(uint8_t heading_mode)
{
    return heading_mode <= 2 ? heading_mode : 0;
}

float DynamicQATester::timestamp_seconds_of_hour(void)
{
    time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm utc_tm {};
        gmtime_r(&now, &utc_tm);
        return float(utc_tm.tm_min * 60 + utc_tm.tm_sec);
    }
    return float((millis() / 1000U) % 3600U);
}

uint32_t DynamicQATester::rid_system_timestamp(void)
{
    time_t now = time(nullptr);
    if (now > time_t(kRidEpoch)) {
        return uint32_t(now - time_t(kRidEpoch));
    }
    return uint32_t(millis() / 1000U);
}

bool DynamicQATester::parse_laa_mac(const char *text, uint8_t mac[6])
{
    if (text == nullptr || text[0] == 0) {
        return false;
    }
    unsigned int bytes[6] {};
    if (sscanf(text, "%02x:%02x:%02x:%02x:%02x:%02x",
               &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != 6) {
        return false;
    }
    for (uint8_t i = 0; i < 6; i++) {
        mac[i] = uint8_t(bytes[i] & 0xFF);
    }
    const bool multicast = (mac[0] & 0x01U) != 0;
    const bool locally_administered = (mac[0] & 0x02U) != 0;
    if (multicast || !locally_administered) {
        return false;
    }
    return true;
}

void DynamicQATester::make_default_laa(uint8_t mac[6])
{
    memset(mac, 0, 6);
    mac[0] = 0x02;

    uint32_t hash = 2166136261UL;
    const char *seed = g.qa_uas_id_seed;
    for (const char *p = seed; p != nullptr && *p != 0; p++) {
        hash ^= uint8_t(*p);
        hash *= 16777619UL;
    }
    mac[2] = uint8_t((hash >> 16) & 0xFF);
    mac[3] = uint8_t((hash >> 8) & 0xFF);
    mac[4] = uint8_t(hash & 0xFF);
    mac[5] = uint8_t((hash >> 24) & 0xFF);
}

void DynamicQATester::derive_slot_mac(uint8_t slot, uint8_t mac[6])
{
    uint8_t base[6] {};
    if (!parse_laa_mac(g.qa_lab_mac_override, base)) {
        make_default_laa(base);
    }

    uint32_t tail = (uint32_t(base[3]) << 16) | (uint32_t(base[4]) << 8) | uint32_t(base[5]);
    tail += slot;

    mac[0] = uint8_t(base[0] | 0x02U);
    mac[0] &= 0xFEU;
    mac[1] = base[1];
    mac[2] = base[2];
    mac[3] = uint8_t((tail >> 16) & 0xFF);
    mac[4] = uint8_t((tail >> 8) & 0xFF);
    mac[5] = uint8_t(tail & 0xFF);
}

void DynamicQATester::normalise_uas_seed(const char *seed, char out[ODID_ID_SIZE + 1])
{
    memset(out, 0, ODID_ID_SIZE + 1);
    if (seed == nullptr || seed[0] == 0) {
        snprintf(out, ODID_ID_SIZE + 1, "QA00000000000000001");
        return;
    }

    size_t write_index = 0;
    for (size_t i = 0; seed[i] != 0 && write_index < ODID_ID_SIZE; i++) {
        const char c = seed[i];
        if (isalnum(uint8_t(c))) {
            out[write_index++] = char(toupper(uint8_t(c)));
        }
    }

    if (write_index == 0) {
        snprintf(out, ODID_ID_SIZE + 1, "QA00000000000000001");
    }
}

void DynamicQATester::make_slot_uas_id(uint8_t slot, char out[ODID_ID_SIZE + 1])
{
    char base[ODID_ID_SIZE + 1] {};
    normalise_uas_seed(g.qa_uas_id_seed, base);
    if (slot == 0) {
        snprintf(out, ODID_ID_SIZE + 1, "%s", base);
        return;
    }

    const size_t keep_len = strlen(base) > 17 ? 17 : strlen(base);
    memcpy(out, base, keep_len);
    snprintf(&out[keep_len], ODID_ID_SIZE + 1 - keep_len, "%03u", unsigned(slot + 1));
}

void DynamicQATester::make_self_id(uint8_t slot, char out[ODID_STR_SIZE + 1])
{
    memset(out, 0, ODID_STR_SIZE + 1);
    const char *label = (g.qa_lab_label[0] != 0) ? g.qa_lab_label : "QA LAB";
    if (slot == 0) {
        snprintf(out, ODID_STR_SIZE + 1, "%s", label);
        return;
    }
    snprintf(out, ODID_STR_SIZE + 1, "%s S%u", label, unsigned(slot + 1));
}

float DynamicQATester::compute_heading_deg(uint8_t mode,
                                           float north_m,
                                           float east_m,
                                           float next_north_m,
                                           float next_east_m)
{
    switch (mode) {
    case 1:
        return 0.0f;
    case 2:
        return fmodf((atan2f(-east_m, -north_m) * RAD_TO_DEG) + 360.0f, 360.0f);
    default:
        return fmodf((atan2f(next_east_m - east_m, next_north_m - north_m) * RAD_TO_DEG) + 360.0f, 360.0f);
    }
}

void DynamicQATester::fill_payload_for_slot(Payload &payload, uint8_t slot, uint8_t slot_count, uint32_t now_ms)
{
    odid_initUasData(&payload.uas_data);

    char uas_id[ODID_ID_SIZE + 1] {};
    char self_id[ODID_STR_SIZE + 1] {};
    make_slot_uas_id(slot, uas_id);
    make_self_id(slot, self_id);
    derive_slot_mac(slot, payload.mac);

    payload.uas_data.BasicID[0].UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    payload.uas_data.BasicID[0].IDType = ODID_IDTYPE_SERIAL_NUMBER;
    strncpy(payload.uas_data.BasicID[0].UASID, uas_id, sizeof(payload.uas_data.BasicID[0].UASID));
    payload.uas_data.BasicIDValid[0] = 1;

    payload.uas_data.SelfID.DescType = ODID_DESC_TYPE_TEXT;
    strncpy(payload.uas_data.SelfID.Desc, self_id, sizeof(payload.uas_data.SelfID.Desc));
    payload.uas_data.SelfIDValid = 1;

    payload.uas_data.OperatorID.OperatorIdType = ODID_OPERATOR_ID;
    strncpy(payload.uas_data.OperatorID.OperatorId, "QA-LAB-OPERATOR", sizeof(payload.uas_data.OperatorID.OperatorId));
    payload.uas_data.OperatorIDValid = 1;

    const float radius_m = g.qa_radius_m < 1.0f ? 1.0f : g.qa_radius_m;
    const float speed_mps = g.qa_speed_mps < 0.1f ? 0.1f : g.qa_speed_mps;
    const float heading_mode = sanitize_heading_mode(g.qa_heading_mode);
    const float slot_seed = 0.71f + 1.13f * float(slot + 1);
    const float orbit_scale = 0.55f + 0.12f * float(slot_count > 1 ? slot : 0);
    const float local_radius = radius_m * orbit_scale;
    const float phase_rate = speed_mps / (local_radius < 10.0f ? 10.0f : local_radius);
    const float phase = (now_ms * 0.001f * phase_rate) + slot_seed;

    const float anchor_radius = radius_m * 0.38f;
    const float anchor_angle = (TWO_PI * float(slot)) / float(slot_count == 0 ? 1 : slot_count);
    const float anchor_north = slot == 0 ? 0.0f : cosf(anchor_angle) * anchor_radius;
    const float anchor_east = slot == 0 ? 0.0f : sinf(anchor_angle) * anchor_radius;

    const float north_m = anchor_north +
                          sinf(phase * (0.88f + 0.06f * slot)) * local_radius +
                          sinf(phase * 0.41f + slot_seed) * (local_radius * 0.24f);
    const float east_m = anchor_east +
                         cosf(phase * (0.91f + 0.05f * slot)) * local_radius +
                         cosf(phase * 0.37f + slot_seed * 0.6f) * (local_radius * 0.22f);
    const float next_north_m = anchor_north +
                               sinf((phase + 0.06f) * (0.88f + 0.06f * slot)) * local_radius +
                               sinf((phase + 0.06f) * 0.41f + slot_seed) * (local_radius * 0.24f);
    const float next_east_m = anchor_east +
                              cosf((phase + 0.06f) * (0.91f + 0.05f * slot)) * local_radius +
                              cosf((phase + 0.06f) * 0.37f + slot_seed * 0.6f) * (local_radius * 0.22f);

    const float lat = g.qa_home_lat + north_m * kMetersToLat;
    const float lon_scale = cosf(g.qa_home_lat * DEG_TO_RAD);
    const float meters_to_lon = 1.0f / (111111.0f * (fabsf(lon_scale) < 0.01f ? 0.01f : lon_scale));
    const float lon = g.qa_home_lon + east_m * meters_to_lon;
    const float altitude_geo = g.qa_alt_m + 4.0f * float(slot) + 2.0f * sinf(phase * 0.63f + slot_seed);
    const float operator_alt_geo = g.qa_alt_m;
    const float height = altitude_geo - operator_alt_geo;

    payload.uas_data.System.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    payload.uas_data.System.ClassificationType = ODID_CLASSIFICATION_TYPE_UNDECLARED;
    payload.uas_data.System.OperatorLatitude = g.qa_home_lat;
    payload.uas_data.System.OperatorLongitude = g.qa_home_lon;
    payload.uas_data.System.AreaCount = 1;
    payload.uas_data.System.AreaRadius = 0;
    payload.uas_data.System.AreaCeiling = -1000.0f;
    payload.uas_data.System.AreaFloor = -1000.0f;
    payload.uas_data.System.CategoryEU = ODID_CATEGORY_EU_UNDECLARED;
    payload.uas_data.System.ClassEU = ODID_CLASS_EU_UNDECLARED;
    payload.uas_data.System.OperatorAltitudeGeo = operator_alt_geo;
    payload.uas_data.System.Timestamp = rid_system_timestamp();
    payload.uas_data.SystemValid = 1;

    payload.uas_data.Location.Status = ODID_STATUS_AIRBORNE;
    payload.uas_data.Location.Direction = compute_heading_deg(uint8_t(heading_mode), north_m, east_m, next_north_m, next_east_m);
    payload.uas_data.Location.SpeedHorizontal = speed_mps;
    payload.uas_data.Location.SpeedVertical = 0.15f * cosf(phase * 0.47f + slot_seed);
    payload.uas_data.Location.Latitude = lat;
    payload.uas_data.Location.Longitude = lon;
    payload.uas_data.Location.AltitudeBaro = altitude_geo - 2.0f;
    payload.uas_data.Location.AltitudeGeo = altitude_geo;
    payload.uas_data.Location.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
    payload.uas_data.Location.Height = height;
    payload.uas_data.Location.HorizAccuracy = ODID_HOR_ACC_10_METER;
    payload.uas_data.Location.VertAccuracy = ODID_VER_ACC_10_METER;
    payload.uas_data.Location.BaroAccuracy = ODID_VER_ACC_10_METER;
    payload.uas_data.Location.SpeedAccuracy = ODID_SPEED_ACC_1_METERS_PER_SECOND;
    payload.uas_data.Location.TSAccuracy = ODID_TIME_ACC_0_2_SECOND;
    payload.uas_data.Location.TimeStamp = timestamp_seconds_of_hour();
    payload.uas_data.LocationValid = 1;

    payload.active_slot = slot;
    payload.active_slots = slot_count;
    payload.wifi_mac_applied = false;
    snprintf(payload.status_text, sizeof(payload.status_text), "qa slot %u/%u", unsigned(slot + 1), unsigned(slot_count));
}

bool DynamicQATester::get_next_payload(Payload &payload, uint32_t now_ms)
{
    if (!enabled()) {
        return false;
    }

    memset(&payload, 0, sizeof(payload));
    const uint8_t slot_count = clamp_slot_count(g.qa_slot_count);
    const uint32_t hold_ms = 350U;
    const uint8_t slot = uint8_t((now_ms / hold_ms) % slot_count);
    return build_payload(slot, slot_count, now_ms, payload);
}

bool DynamicQATester::apply_wifi_mac_override(const uint8_t mac[6])
{
    if (last_wifi_mac_valid && memcmp(last_wifi_mac, mac, sizeof(last_wifi_mac)) == 0) {
        return true;
    }
    uint8_t local_mac[6] {};
    memcpy(local_mac, mac, sizeof(local_mac));
    local_mac[0] |= 0x02U;
    local_mac[0] &= 0xFEU;
    const esp_err_t err = esp_wifi_set_mac(WIFI_IF_AP, local_mac);
    if (err != ESP_OK) {
        return false;
    }
    memcpy(last_wifi_mac, local_mac, sizeof(last_wifi_mac));
    last_wifi_mac_valid = true;
    return true;
}
