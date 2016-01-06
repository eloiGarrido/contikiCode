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
    static struct etimer pt_broadcast;
    static struct etimer pt_message;
    static struct etimer energy_info;

    PROCESS_EXITHANDLER(broadcast_close(&broadcast); energytrace_stop();)

    PROCESS_BEGIN();

    broadcast_open(&broadcast, 129, &broadcast_call);
    energytrace_start(); // Energy trace initializer
    /* Delay 2-4 seconds */
    etimer_set(&pt_broadcast, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));
    etimer_set(&pt_message, CLOCK_SECOND * 10 + random_rand() % (CLOCK_SECOND * 10));
    etimer_set(&energy_info, CLOCK_SECOND * 6);
    while(1){
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER){
            if(etimer_expired(&pt_broadcast)){
              // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
                if (node_state == NODE_ACTIVE){
                    packetbuf_copyfrom("Hello",6);
                    broadcast_send(&broadcast);
                    printf("Braodcast message sent\n");
                    etimer_reset(&pt_broadcast);
               // etimer_set(&pt_broadcast, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));
                }     
            }
            else if(etimer_expired(&pt_message)){
            printf("---------------------Periodic message---------------------\n");
            etimer_reset(&pt_message);
            }
            else if (etimer_expired(&energy_info)){
                compute_node_state();
                compute_node_duty_cycle();
                compute_harvesting_rate();
                etimer_reset(&energy_info);
                printf("node_state: %d, node_duty_cycle %u, harvesting_rate: %u\n",get_node_state(), get_duty_cycle(),get_harvesting_rate() );
            }

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