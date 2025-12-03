// face_state.h
#pragma once
#include <stdint.h>

// face state (effective) kept in app_httpd.cpp
extern volatile bool detection_via_gui;
extern volatile bool recognition_via_gui;
extern volatile bool detection_via_pir;
extern volatile bool recognition_via_pir;

// enrollment variable (already in app_httpd.cpp but make it visible)
extern volatile int8_t is_enrolling;

// computed effective flags (also visible if you want to read)
extern volatile int8_t detection_enabled;
extern volatile int8_t recognition_enabled;

// Recompute effective detection/recognition from the source flags.
// Implemented in app_httpd.cpp - make non-static so other files can call it.
void recompute_face_state();
