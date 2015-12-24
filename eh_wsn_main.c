#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>

PROCESS(my_first_app_process,"My_First_App");
AUTOSTART_PROCESSES(&my_first_app_process);

PROCESS_THREAD(my_first_app_process,ev,data)
{ 
    /* Declare variables required */
    static int i=652;         

    /* Begin Process */
    PROCESS_BEGIN();          

    /* Set of C statement(s) */
    printf("EE-%d is an awesome course at USC\n",i);             
    
    /* Process End */
    PROCESS_END();            
}