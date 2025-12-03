#ifndef FACE_STATE_H
#define FACE_STATE_H

#include <stdint.h>

extern volatile bool detection_via_gui;
extern volatile bool recognition_via_gui;
extern volatile bool detection_via_pir;
extern volatile bool recognition_via_pir;

extern volatile int8_t is_enrolling;

extern volatile int8_t detection_enabled;
extern volatile int8_t recognition_enabled;

/* recompute facial detection and recognition state on pir and gui flags */
void recompute_face_state();

#endif
