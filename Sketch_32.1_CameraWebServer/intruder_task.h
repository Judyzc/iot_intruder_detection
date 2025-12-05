#ifndef INTRUDER_TASK_H
#define INTRUDER_TASK_H

#include <stdint.h>

void intruder_task_init(void);

bool intruder_queue_send(uint32_t msg);
#endif
