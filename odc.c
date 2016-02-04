#include "odc.h"
#include "node-id.h"
/*--------------------------- VARIABLES-------------------------------*/
static int debug;

// Timeout timers
static struct ctimer cpowercycle_ctimer;

// Data exchange
static uint8_t mySeq,data[DATA_SIZE],seq[DATA_SIZE],ttl[DATA_SIZE];
static uint8_t read_idx,write_idx,q_size;
static uint8_t unique_bitmap[10000/8] = {0};

// ODC
static enum mac_state current_state = disabled;

/* -------------------------- RADIO FUNCTIONS ----------------------- */

static inline void radio_flush_tx(void) {
    FASTSPI_STROBE(CC2420_SFLUSHTX);
}

static inline void radio_flush_rx(void) {
    uint8_t dummy;
    FASTSPI_READ_FIFO_BYTE(dummy);
    FASTSPI_STROBE(CC2420_SFLUSHRX);
    FASTSPI_STROBE(CC2420_SFLUSHRX);
}

static inline uint8_t radio_status(void) {
    uint8_t status;
    FASTSPI_UPD_STATUS(status);
    return status;
}

static inline void radio_on(void) {
    FASTSPI_STROBE(CC2420_SRXON);
    while(!(radio_status() & (BV(CC2420_XOSC16M_STABLE))));
    ENERGEST_ON(ENERGEST_TYPE_LISTEN);
}

static inline void radio_off(void) {
#if ENERGEST_CONF_ON
    if (energest_current_mode[ENERGEST_TYPE_TRANSMIT]) {
	ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
    }
    if (energest_current_mode[ENERGEST_TYPE_LISTEN]) {
	ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
    }
#endif /* ENERGEST_CONF_ON */
    FASTSPI_STROBE(CC2420_SRFOFF);
}




/*--------------------------- DC FUNCTIONS ---------------------------*/
static void powercycle_turn_radio_off(void) {
    if ((current_state == idle) && !(IS_SINK)){
	// radio_off();
	leds_off(LEDS_BLUE);
    }
}

static void powercycle_turn_radio_on(void) {
    if (current_state != disabled) {
	PRINTF("on\n");
	// radio_on();
	// leds_on(LEDS_BLUE);
	// rendezvous_time = 0;
	// rendezvous_starting_time = RTIMER_NOW();
    }
}

static void goto_idle() {
    radio_flush_rx();
    radio_flush_tx();
    current_state = idle;
    powercycle_turn_radio_off();
    // leds_off(LEDS_RED);
    // leds_off(LEDS_GREEN);
    // fast_forward = 0;
    STOP_IDLE(); // if we go to idle before the idle timer expire we remove the timer
}

/*--------------------------- DATA FUNCTIONS -------------------------*/

static void set_bitmap(int idx){
    unique_bitmap[idx / 8] |= 1 << (idx % 8);
}

static void clear_bitmap (int idx){
    unique_bitmap[idx / 8] &= ~(1 << (idx % 8));
}

static uint8_t get_bitmap(int idx){
    return (unique_bitmap[idx / 8] >> (idx % 8)) & 1;
}

//TODO Modify data for ODC
static int add_data(uint8_t _data, uint8_t _ttl, uint8_t _seq){
    int next_idx, _unique;
    _unique = (_data-1)*100 + _seq;
    if(get_bitmap(_unique)) return 0; // if the message is already in the queue, do not add it again
    set_bitmap(_unique);
    next_idx = (write_idx+1)%DATA_SIZE;
    // error if queue is full
    if (next_idx == read_idx) return 0;
    // do not add 0 data
    if (_data == 0) return 0;
    data[write_idx] = _data;
    ttl[write_idx] = _ttl;
    seq[write_idx] = _seq;
    write_idx = next_idx;
    q_size++;
    return 1;

}

static uint8_t read_data(){
    if (read_idx == write_idx) return 0;
    return data[read_idx];
}

static uint8_t read_seq(){
    if (read_idx == write_idx) return 0;
    return seq[read_idx];
}

static uint8_t read_ttl(){
    if (read_idx == write_idx) return 0;
    return ttl[read_idx];
}

static uint8_t pop_data(){
    uint8_t _data,_seq;
    int _unique;
    if (read_idx == write_idx) return 0; // error if queue is empty
    _data = data[read_idx];
    _seq = seq[read_idx];
    _unique = (_data-1)*100 + _seq;
    clear_bitmap(_unique);
    read_idx = (read_idx+1)%DATA_SIZE;
    q_size--;
    return _data;
}

/*---------------------------- ODC FUNCTIONS -------------------------*/

int odc_send_packet(void) {
	
}