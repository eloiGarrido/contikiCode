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
static void
send_packet(mac_callback_t sent, void *ptr)
{
  NETSTACK_RDC.send(sent, ptr);
}
/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
  NETSTACK_LLSEC.input();
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return NETSTACK_RDC.on();
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
  return NETSTACK_RDC.off(keep_radio_on);
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
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
/*---------------------------------------------------------------------------*/
static void
send_probe(void)
{
  struct lpp_hdr *hdr;
  struct announcement_msg *adata;
  struct announcement *a;

  /* Set up the probe header. */
  packetbuf_clear();
  // packetbuf_set_datalen(sizeof(struct lpp_hdr));
  hdr = packetbuf_dataptr();
  // hdr->type = TYPE_PROBE;
  // rimeaddr_copy(&hdr->sender, &rimeaddr_node_addr);
  /*  rimeaddr_copy(&hdr->receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));*/
  // rimeaddr_copy(&hdr->receiver, &rimeaddr_null);

  packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &rimeaddr_null);
  {
    int hdrlen = NETSTACK_FRAMER.create();
    if(hdrlen == 0) {
      /* Failed to send */
      return;
    }
  }
  
  /* Construct the announcements */
  // adata = (struct announcement_msg *)((char *)hdr + sizeof(struct lpp_hdr));
  
  // adata->num = 0;
  // for(a = announcement_list(); a != NULL; a = list_item_next(a)) {
  //   adata->data[adata->num].id = a->id;
  //   adata->data[adata->num].value = a->value;
  //   adata->num++;
  // }

  // packetbuf_set_datalen(sizeof(struct lpp_hdr) +
          // ANNOUNCEMENT_MSG_HEADERLEN +
          // sizeof(struct announcement_data) * adata->num);

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