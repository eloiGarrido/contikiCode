
#ifndef __ODC_H__
#define __ODC_H__


#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"
#include "dev/button-sensor.h"
#include "lib/list.h"
#include "sys/ctimer.h"
#include "dev/leds.h"
#include "../apps/energytrace/energytrace.h"
#include <stdio.h>
#include "metric.h"
#include <string.h>
#include "core/net/mac/mac.h"
#include "dev/cc2420_const.h"
#include "dev/leds.h"
#include "dev/spi.h"


enum mac_state{
	disabled = 0,
	enabled = 1,
	idle = 1,
	wait_to_send = 2,
	wait_beacon_ack = 3,
	sending_ack = 4,
	beacon_sent = 5,
	wait_select = 6,
	select_received = 7
} ;



#endif /* __ODC_H__ */