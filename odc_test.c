#include "node-id.h"
#include "odc.h"

/* Print ODC protocol stats */
PROCESS(odc_print_stats_process, "ODC print stats");
PROCESS_THREAD(odc_print_stats_process, ev, data) {
	PROCESS_BEGIN();

	static struct etimer et;

	etimer_set(&et, CLOCK_SECOND*55+(random_rand()%(CLOCK_SECOND*10)));
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	while(1) {
		odc_print_stats();
		//Print node data every 10 seconds
		etimer_set(&et,CLOCK_SECOND*10);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

	PROCESS_END();
}

/* Harvest energy and update node variables */
PROCESS(odc_energy_manager_process, "ODC EnergyMan");
PROCESS_THREAD(odc_energy_manager_process, ev, data) {
	PROCESS_BEGIN();

	PROCESS_END();
}

/* Main process, calls odc main program */
PROCESS(odc_main_process, "ODC Main process");
PROCESS_THREAD(odc_main_process, ev, data) {
	PROCESS_BEGIN();

	PROCESS_END();
}

AUTOSTART_PROCESSES(&odc_main_process,&odc_energy_manager_process);