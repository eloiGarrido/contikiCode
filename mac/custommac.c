/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         A MAC protocol that does not do anything.
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "net/mac/custommac.h"
#include "net/netstack.h"
#include "net/ip/uip.h"
#include "net/ip/tcpip.h"
#include "net/packetbuf.h"
#include "net/netstack.h"

#include "sys/compower.h"
#include "net/rime/announcement.h"
#include "rimeaddr.h"
#include <stdlib.h>
#include <stdio.h>
#include "net/packetbuf.h"
#include "net/mac/mac.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "net/netstack.h"
/*---------------------------------------------------------------------------*/
#define TYPE_PROBE        1
#define TYPE_DATA         2

#define LOWEST_OFF_TIME (CLOCK_SECOND / 8)
#define LISTEN_TIME (CLOCK_SECOND / 128)
#define OFF_TIME (CLOCK_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE - LISTEN_TIME)
#define MAX_QUEUED_PACKETS 4
#define PACKET_LIFETIME (LISTEN_TIME + OFF_TIME)
#define UNICAST_TIMEOUT (1 * PACKET_LIFETIME + PACKET_LIFETIME / 2)

#define ENCOUNTER_LIFETIME (16 * OFF_TIME)


static clock_time_t off_time_adjustment = 0;
static clock_time_t off_time = OFF_TIME;
static uint8_t is_listening = 0;
static uint8_t mac_is_on;
static struct ctimer timer;
static struct pt dutycycle_pt;
static struct compower_activity current_packet;

struct queue_list_item {
  struct queue_list_item *next;
  struct queuebuf *packet;
  struct ctimer removal_timer;
  struct compower_activity compower;
  mac_callback_t sent_callback;
  void *sent_callback_ptr;
  uint8_t num_transmissions;
};


struct mac_hdr {
  uint16_t type;
  rimeaddr_t sender;
  rimeaddr_t receiver;
};
#define BEACON_MSG_HEADERLEN 5
struct beacon_data{
  uint16_t  beacon_remaining_energy;
  uint16_t  beacon_harvesting_rate;
  uint16_t  beacon_metric;
  uint16_t  id;
  uint16_t  value;
};

struct beacon_msg{
  uint16_t num;
  struct beacon_data data[];
};

LIST(pending_packets_list);
LIST(queued_packets_list);
MEMB(queued_packets_memb, struct queue_list_item, MAX_QUEUED_PACKETS);

struct encounter {
  struct encounter *next;
  rimeaddr_t neighbor;
  clock_time_t time;
  struct ctimer remove_timer;
  struct ctimer turn_on_radio_timer;
};

#define MAX_ENCOUNTERS 4
LIST(encounter_list);
MEMB(encounter_memb, struct encounter, MAX_ENCOUNTERS);

/*---------------------------------------------------------------------------*/
static void
turn_radio_off(void)
{
  // if(mac_is_on) {
    printf("Turn Radio OFF\n");
    NETSTACK_RADIO.off();
  // }
  /*  leds_off(LEDS_YELLOW);*/
}

static void
turn_radio_on(void)
{
  // if(mac_is_on == 0) {
    printf("Turn Radio ON\n");
    NETSTACK_RADIO.on();
  // }
  /*  leds_off(LEDS_YELLOW);*/
}
/*---------------------------------------------------------------------------*/

static void
remove_encounter(void *encounter)
{
  struct encounter *e = encounter;

  ctimer_stop(&e->remove_timer);
  ctimer_stop(&e->turn_on_radio_timer);
  list_remove(encounter_list, e);
  memb_free(&encounter_memb, e);
}

static void
remove_queued_packet(struct queue_list_item *i, uint8_t tx_ok)
{
  mac_callback_t sent;
  void *ptr;
  int num_transmissions = 0;
  int status;
  
  printf("%d.%d: removing queued packet\n",
     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);


  queuebuf_to_packetbuf(i->packet);
  
  ctimer_stop(&i->removal_timer);
  queuebuf_free(i->packet);
  list_remove(pending_packets_list, i);
  list_remove(queued_packets_list, i);

  /* XXX potential optimization */
  if(list_length(queued_packets_list) == 0 && is_listening == 0) {
    turn_radio_off();
    compower_accumulate(&i->compower);
  }

  sent = i->sent_callback;
  ptr = i->sent_callback_ptr;
  num_transmissions = i->num_transmissions;
  memb_free(&queued_packets_memb, i);
  if(num_transmissions == 0 || tx_ok == 0) {
    status = MAC_TX_NOACK;
  } else {
    status = MAC_TX_OK;
  }
  mac_call_sent_callback(sent, ptr, status, num_transmissions);
}

/*---------------------------------------------------------------------------*/
static void
remove_queued_old_packet_callback(void *item)
{
  remove_queued_packet(item, 0);
}

/*---------------------------------------------------------------------------*/
static void
turn_radio_on_for_neighbor(rimeaddr_t *neighbor, struct queue_list_item *i)
{
    if(rimeaddr_cmp(neighbor, &rimeaddr_null)) {
        list_add(queued_packets_list, i);
        return;
    }
    /* We did not find the neighbor in the list of recent encounters, so
     we just turn on the radio. */
    /*  printf("Neighbor %d.%d not found in recent encounters\n",
      neighbor->u8[0], neighbor->u8[1]);*/
    turn_radio_on();
    list_add(queued_packets_list, i);
    return;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
  // NETSTACK_RDC.send(sent, ptr);
    struct mac_hdr hdr;
    clock_time_t timeout;
    uint8_t is_broadcast = 0;

    rimeaddr_copy(&hdr.sender, &rimeaddr_node_addr);
    rimeaddr_copy(&hdr.receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
    if(rimeaddr_cmp(&hdr.receiver, &rimeaddr_null)) {
        is_broadcast = 1;
    }
    hdr.type = TYPE_DATA;
    packetbuf_hdralloc(sizeof(struct mac_hdr));
    memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct mac_hdr));
    packetbuf_compact();
    packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);

    {  //FIXME What is this?
        int hdrlen = NETSTACK_FRAMER.create();
        if(hdrlen == 0) {
            /* Failed to send */
            mac_call_sent_callback(sent, ptr, MAC_TX_ERR_FATAL, 0);
            return;
        }
    }
    printf("%d.%d: queueing packet to %d.%d, channel %d\n",rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], hdr.receiver.u8[0], hdr.receiver.u8[1], packetbuf_attr(PACKETBUF_ATTR_CHANNEL));
    // #if WITH_ADAPTIVE_OFF_TIME
    // off_time = LOWEST_OFF_TIME;
    // restart_dutycycle(off_time);
    // #endif /* WITH_ADAPTIVE_OFF_TIME */
    {
        struct queue_list_item *i;
        i = memb_alloc(&queued_packets_memb);
        if(i != NULL) {
            i->sent_callback = sent;
            i->sent_callback_ptr = ptr;
            i->num_transmissions = 0;
            i->packet = queuebuf_new_from_packetbuf();
            if(i->packet == NULL) {
                memb_free(&queued_packets_memb, i);
                printf("null packet\n");
                mac_call_sent_callback(sent, ptr, MAC_TX_ERR, 0);
                return;
            } else {
                if(is_broadcast) {
                    timeout = PACKET_LIFETIME;
                } else {
                    timeout = UNICAST_TIMEOUT;
                }
                // ctimer_set(&i->removal_timer, timeout, remove_queued_old_packet_callback, i);
                
                /* Wait for a probe packet from a neighbor. The actual packet
                transmission is handled by the read_packet() function,
                which receives the probe from the neighbor. */
                turn_radio_on_for_neighbor(&hdr.receiver, i);

            }
        } else {
            printf("i == NULL\n");
            mac_call_sent_callback(sent, ptr, MAC_TX_ERR, 0);
        }
    }
}
/*---------------------------------------------------------------------------*/
static void
register_encounter(rimeaddr_t *neighbor, clock_time_t time)
{
  struct encounter *e;

  /* If we have an entry for this neighbor already, we renew it. */
  for(e = list_head(encounter_list); e != NULL; e = list_item_next(e)) {
    if(rimeaddr_cmp(neighbor, &e->neighbor)) {
      e->time = time;
      ctimer_set(&e->remove_timer, ENCOUNTER_LIFETIME, remove_encounter, e);
      break;
    }
  }
  /* No matchin encounter was found, so we allocate a new one. */
  if(e == NULL) {
    e = memb_alloc(&encounter_memb);
    if(e == NULL) {
      /* We could not allocate memory for this encounter, so we just drop it. */
      return;
    }
    rimeaddr_copy(&e->neighbor, neighbor);
    e->time = time;
    ctimer_set(&e->remove_timer, ENCOUNTER_LIFETIME, remove_encounter, e);
    list_add(encounter_list, e);
  }
}
/*---------------------------------------------------------------------------*/
static int
detect_ack(void)
{
#define INTER_PACKET_INTERVAL              RTIMER_ARCH_SECOND / 5000
#define ACK_LEN 3
#define AFTER_ACK_DETECTECT_WAIT_TIME      RTIMER_ARCH_SECOND / 1000
  rtimer_clock_t wt;
  uint8_t ack_received = 0;
  
  wt = RTIMER_NOW();
  // leds_on(LEDS_GREEN);
  while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + INTER_PACKET_INTERVAL)) { }
  // leds_off(LEDS_GREEN);
  /* Check for incoming ACK. */
  if((NETSTACK_RADIO.receiving_packet() ||
      NETSTACK_RADIO.pending_packet() ||
      NETSTACK_RADIO.channel_clear() == 0)) {
    int len;
    uint8_t ackbuf[ACK_LEN + 2];
    
    wt = RTIMER_NOW();
    while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTECT_WAIT_TIME)) { }
    
    len = NETSTACK_RADIO.read(ackbuf, ACK_LEN);
    if(len == ACK_LEN) {
      ack_received = 1;
    }
  }
  if(ack_received) {
    // leds_toggle(LEDS_RED);
  }
  return ack_received;
}

static void
packet_input(void)
{
    struct mac_hdr hdr;
    clock_time_t reception_time;

    reception_time = clock_time();

    if(!NETSTACK_FRAMER.parse()) {
        printf("custommac input_packet framer error\n");
    }
    memcpy(&hdr, packetbuf_dataptr(), sizeof(struct mac_hdr));;
    packetbuf_hdrreduce(sizeof(struct mac_hdr));

    if(hdr.type == TYPE_PROBE) {
        struct beacon_msg adata;
        /* Register the encounter with the sending node. We now know the
        neighbor's phase. */
        register_encounter(&hdr.sender, reception_time);

        /* Parse incoming announcements */
        memcpy(&adata, packetbuf_dataptr(), MIN(packetbuf_datalen(), sizeof(adata)));
        if(list_length(queued_packets_list) > 0) {
            struct queue_list_item *i;
            for(i = list_head(queued_packets_list); i != NULL; i = list_item_next(i)) {
                const rimeaddr_t *receiver;
                uint8_t sent;

                sent = 0;

                receiver = queuebuf_addr(i->packet, PACKETBUF_ADDR_RECEIVER);
                if(rimeaddr_cmp(receiver, &hdr.sender) || rimeaddr_cmp(receiver, &rimeaddr_null)) {
                    queuebuf_to_packetbuf(i->packet);
                    /* Attribute the energy spent on listening for the probe
                     to this packet transmission. */
                    compower_accumulate(&i->compower);
                    if(!rimeaddr_cmp(receiver, &rimeaddr_null)) {
                        // if(detect_ack()) {
                        //     remove_queued_packet(i, 1);
                        // } else {
                        //     remove_queued_packet(i, 0);
                        // }

                    }

                    if(sent) {
                        turn_radio_off();
                    }
                }
            }
        }

    } else if(hdr.type == TYPE_DATA) {
        turn_radio_off();
        if(!rimeaddr_cmp(&hdr.receiver, &rimeaddr_null)) {
            if(!rimeaddr_cmp(&hdr.receiver, &rimeaddr_node_addr)) {
                /* Not broadcast or for us */
                printf("%d.%d: data not for us from %d.%d\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], hdr.sender.u8[0], hdr.sender.u8[1]);
                return;
            }
            packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &hdr.receiver);
        }
        packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &hdr.sender);

        printf("%d.%d: got data from %d.%d\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], hdr.sender.u8[0], hdr.sender.u8[1]);

        /* Accumulate the power consumption for the packet reception. */
        compower_accumulate(&current_packet);
        /* Convert the accumulated power consumption for the received
           packet to packet attributes so that the higher levels can
           keep track of the amount of energy spent on receiving the
           packet. */
        compower_attrconv(&current_packet);
      
        /* Clear the accumulated power consumption so that it is ready
           for the next packet. */
        compower_clear(&current_packet);

        // NETSTACK_LLSEC.input();
        NETSTACK_MAC.input();
    }
}


static int
on(void)
{
  mac_is_on = 1;
  turn_radio_on();
  // return NETSTACK_RDC.on();
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
  mac_is_on = 0;
  if(keep_radio_on) {
    turn_radio_on();
  } else {
    turn_radio_off();
  }
  return 1;

}

// static unsigned short
// channel_check_interval(void)
// {
//   return 0;
// }


/*---------------------------------------------------------------------------*/
static void
listen_callback(int periods)
{
  is_listening = periods;
  turn_radio_on();
}
/*---------------------------------------------------------------------------*/
static void
send_probe(void)
{
  struct mac_hdr *hdr;
  struct beacon_msg *adata;
  struct announcement *a;

  /* Set up the probe header. */
  packetbuf_clear();
  packetbuf_set_datalen(sizeof(struct mac_hdr));
  hdr = packetbuf_dataptr();
  hdr->type = TYPE_PROBE;
  rimeaddr_copy(&hdr->sender, &rimeaddr_node_addr);
  /*  rimeaddr_copy(&hdr->receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));*/
  rimeaddr_copy(&hdr->receiver, &rimeaddr_null);

  packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &rimeaddr_null);
  {
    int hdrlen = NETSTACK_FRAMER.create();
    if(hdrlen == 0) {
      /* Failed to send */
      return;
    }
  }
  
  /* Construct the announcements */
  adata = (struct beacon_msg *)((char *)hdr + sizeof(struct mac_hdr));
  
  adata->num = 0;
  for(a = announcement_list(); a != NULL; a = list_item_next(a)) { //FIXME What is this announcement_list?
    adata->data[adata->num].id = a->id;
    adata->data[adata->num].value = a->value;
    adata->num++;
  }

  packetbuf_set_datalen(sizeof(struct mac_hdr) +
          BEACON_MSG_HEADERLEN +
          sizeof(struct beacon_data) * adata->num);

  /*  PRINTF("Sending probe\n");*/

  /*  printf("probe\n");*/

  if(NETSTACK_RADIO.channel_clear()) {
    NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
  } else {
    // off_time_adjustment = random_rand() % (OFF_TIME / 2);
  }

  compower_accumulate(&compower_idle_activity);
}
/*---------------------------------------------------------------------------*/
static void
send_stream_probe(void *dummy)
{
  /* Turn on the radio for sending a probe packet and 
     anticipating a data packet from a neighbor. */
  turn_radio_on();
  
  /* Send a probe packet. */
  send_probe();

#if WITH_STREAMING
  is_streaming = 1;
#endif /* WITH_STREAMING */
}
/*---------------------------------------------------------------------------*/
static int
num_packets_to_send(void)
{
  return list_length(queued_packets_list);
}
/*---------------------------------------------------------------------------*/
static int
dutycycle(void *ptr){
    printf("dutycycle function\n");
    struct ctimer *t = ptr;
    PT_BEGIN(&dutycycle_pt);
    /* Turn on the radio for sending a probe packet and anticipating a data packet from a neighbor. */
    turn_radio_on();
    /* Send a probe packet. */
    send_probe();
    /* Set a timer so that we keep the radio on for LISTEN_TIME. */
    ctimer_set(t, LISTEN_TIME, (void (*)(void *))dutycycle, t);
    PT_YIELD(&dutycycle_pt);

    /* If we have no packets to send (indicated by the list length of
     queued_packets_list being zero), we should turn the radio
     off. Othersize, we keep the radio on. */
    if(num_packets_to_send() == 0) {
    
    /* If we are not listening for announcements, we turn the radio
    off and wait until we send the next probe. */
        if(is_listening == 0){
            int current_off_time;
            if(!NETSTACK_RADIO.receiving_packet()){

                turn_radio_off();
                compower_accumulate(&compower_idle_activity);
            }
            current_off_time = off_time - off_time_adjustment;
            if(current_off_time < LISTEN_TIME * 2){
                current_off_time = LISTEN_TIME * 2;
            }
            off_time_adjustment = 0;
            ctimer_set(t, current_off_time, (void (*)(void *))dutycycle, t);
            PT_YIELD(&dutycycle_pt);
            
            /* With Adaptive off time */
            // off_time += LOWEST_OFF_TIME;
            // if(off_time > OFF_TIME){
            //     off_time = OFF_TIME;
            // }
        }else{
            is_listening--;
            ctimer_set(t, off_time, (void (*)(void *))dutycycle, t);
            PT_YIELD(&dutycycle_pt);
        }
    }
    PT_END(&dutycycle_pt);
}
/*---------------------------------------------------------------------------*/
static void
restart_dutycycle(clock_time_t initial_wait){
    PT_INIT(&dutycycle_pt);
    ctimer_set(&timer, initial_wait, (void (*)(void *))dutycycle, &timer);
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return OFF_TIME + LISTEN_TIME;
}
/*---------------------------------------------------------------------------*/

static void
init(void)
{
    restart_dutycycle(random_rand() % OFF_TIME);
    mac_is_on = 1;
    announcement_register_listen_callback(listen_callback);

    memb_init(&queued_packets_memb);
    list_init(queued_packets_list);
    list_init(pending_packets_list);
}
/*---------------------------------------------------------------------------*/
const struct mac_driver custommac_driver = {
  "custommac",
  init,
  send_packet,
  packet_input,
  on,
  off,
  channel_check_interval,
};