#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"
#include "dev/button-sensor.h"
#include "lib/list.h"
#include "sys/ctimer.h"
#include "dev/leds.h"
#include "../apps/energytrace/energytrace.h"
#include <stdio.h>

// #include "tudLib/node_state.h"

#define CHANNEL 135
#define MY_TIMEOUT 1 * CLOCK_SECOND
/*---------------------------------------------------------------------------*/
PROCESS(my_first_app_process,"My_First_App");
AUTOSTART_PROCESSES(&my_first_app_process);
/*---------------------------------------------------------------------------*/
struct ctimer c;


/*---------------------------------------------------------------------------*/
/* Broadcast message receive */
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  printf("broadcast message received from %d.%d: '%s'\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

/* Functions */

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(my_first_app_process,ev,data)
{ 
    /* Declare variables required */
	static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast); energytrace_stop();)

  PROCESS_BEGIN();

	broadcast_open(&broadcast, 129, &broadcast_call);
  energytrace_start(); // Energy trace initializer

	while(1)
	{
		/* Delay 2-4 seconds */
    	etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));
    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      if (node_state == NODE_ACTIVE)
      {
        packetbuf_copyfrom("Hello",6);
        broadcast_send(&broadcast);
        printf("Braodcast message sent\n");
      }

	}

    /* Process End */
    PROCESS_END();            
}


/** Xin's Energy Trace 
In your source file, you can use the variables: remaining_energy and node_state, which are defined in energytrace.h.
To start and stop the tracer, use energytrace_start() and energytrace_stop() functions.
When the tracer is running, it prints energy consumption per second.
*/