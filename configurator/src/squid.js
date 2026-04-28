export const idtype_t = {
    "NONE": 0,
    "SERIAL_NUMBER": 1,
    "CAA_REGISTRATION_ID": 2,
    "ID_ASSIGNED_UUID": 3,
    "SPECIFIC_SESSION_ID": 4
};

export const uatype_t = {
    "NONE": 0,
    "AEROPLANE": 1,
    "HELICOPTER_OR_MULTIROTOR": 2,
    "GYROPLANE": 3,
    "HYBRID_LIFT": 4,
    "ORNITHOPTER": 5,
    "GLIDER": 6,
    "KITE": 7,
    "FREE_BALLOON": 8,
    "CAPTIVE_BALLOON": 9,
    "AIRSHIP": 10,
    "FREE_FALL_PARACHUTE": 11,
    "ROCKET": 12,
    "TETHERED_POWERED_AIRCRAFT": 13,
    "GROUND_OBSTACLE": 14,
    "OTHER": 15
};

export const status_t = {
    "UNDECLARED": 0,
    "GROUND": 1,
    "AIRBORNE": 2,
    "EMERGENCY": 3,
    "REMOTE_ID_SYSTEM_FAILURE": 4
};


export const external_t = {
    "NONE": 0,
    "GPS": 1,
    "LTM": 2,
};

export const ghost_id_scheme_t = {
    "DERIVE_FROM_PRIMARY": 0,
    "FULLY_RANDOM_PER_GHOST": 1,
    "CUSTOM_PREFIX_PLUS_SLOT": 2,
};

export const ghost_flight_mode_t = {
    "RING": 0,
    "RANDOM_WANDER": 1,
    "MIXED_SWARM": 2,
};

export const ghost_altitude_mode_t = {
    "STAGGERED": 0,
    "RANDOM_BAND": 1,
    "FOLLOW_PRIMARY_OFFSET": 2,
};
