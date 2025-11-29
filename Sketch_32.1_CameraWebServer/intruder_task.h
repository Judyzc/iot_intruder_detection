// intruder_task.h
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void intruder_task_init(void);

bool intruder_queue_send(uint32_t msg);
#ifdef __cplusplus
}
#endif
