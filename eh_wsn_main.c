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


#define CHANNEL 135
#define MY_TIMEOUT 1 * CLOCK_SECOND
/*---------------------------------------------------------------------------*/
PROCESS(my_first_app_process,"My_First_App");
// PROCESS(energy_monitoring, "Energy_Monitoring_Process");
AUTOSTART_PROCESSES(&my_first_app_process/*, &energy_monitoring*/);
/*---------------------------------------------------------------------------*/
struct ctimer c;


/*---------------------------------------------------------------------------*/
/* Broadcast message receive */
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // printf("broadcast message received from %d.%d: '%s'\n",
         // from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

    //TODO Add packet to queue, update hop count and delay and process info.
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

    etimer_set(&pt_broadcast, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));
    etimer_set(&pt_message, CLOCK_SECOND * 10 );
    etimer_set(&energy_info, CLOCK_SECOND / 2 );

    while(1){
        PROCESS_WAIT_EVENT();
        
        if (ev == PROCESS_EVENT_TIMER)
        {
            if(etimer_expired(&pt_broadcast))
            {
              // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
                if (node_state == NODE_ACTIVE)
                {
                    packetbuf_copyfrom("Hello",6);
                    broadcast_send(&broadcast);
                }     
                etimer_reset(&pt_broadcast);
            }
            else if(etimer_expired(&pt_message))
            {
                printf("---------------------Periodic message---------------------\n");
                etimer_reset(&pt_message);
            }
            else if (etimer_expired(&energy_info))
            {

                printf("node_state: %d, remaining_energy: %lu, node_duty_cycle %d, harvesting_rate: %lu\n",node_state ,remaining_energy, node_duty_cycle, harvesting_rate );
                etimer_reset(&energy_info);
            }
            else
            {
                printf("Inside ev == PROCESS_EVENT_TIMER, but non of the defined\n");
            }
        }
        else if(ev == PROCESS_EVENT_MSG)
        {
            printf("Data: node_state = %s\n", data );

        }
        else
        {
            printf("WARNING: Not a defined EVENT\n" );
        }
    }
    /* Process End */
    PROCESS_END();            
}
