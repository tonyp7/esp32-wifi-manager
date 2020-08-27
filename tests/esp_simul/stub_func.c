/**
 * @file stub_func.c
 * @author TheSomeMan
 * @date 2020-08-25
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <unistd.h>

void
vApplicationIdleHook(void)
{
    sleep(1);
}

void
vMainQueueSendPassed(void)
{
}
