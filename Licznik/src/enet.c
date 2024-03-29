/*
 * enet.c
 *
 *  Created on: 12 lip 2023
 *      Author: student
 */

#include "../inc/enet.h"
#include "../inc/ethernet.h"

static const uint8_t EnetClkDiv[] = {4, 6, 8, 10, 14, 20, 28, 36, 40, 44,
									 48, 52, 56, 60, 64};

static uint32_t phyAddr;

void msDelay(uint32_t delay)
{
	for (uint32_t i = 0; i < delay; i++);
}

const struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};
const struct eth_addr ethzero = {{0,0,0,0,0,0}};

static err_t
etharp_send_ip(struct netif *netif, struct pbuf *p, struct eth_addr *src, struct eth_addr *dst)
{
  struct eth_hdr *ethhdr = (struct eth_hdr *)p->payload;

  LWIP_ASSERT("netif->hwaddr_len must be the same as ETHARP_HWADDR_LEN for etharp!",
              (netif->hwaddr_len == ETHARP_HWADDR_LEN));
  ETHADDR32_COPY(&ethhdr->dest, dst);
  ETHADDR16_COPY(&ethhdr->src, src);
  ethhdr->type = PP_HTONS(ETHTYPE_IP);
  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_send_ip: sending packet %p\n", (void *)p));
  /* send the packet */
  return netif->linkoutput(netif, p);
}

void Chip_ENET_Set100Mbps(LPC_ENET_T *pENET)
{
	pENET->MAC.SUPP = ENET_SUPP_100Mbps_SPEED;
}

void Chip_ENET_Set10Mbps(LPC_ENET_T *pENET)
{
	pENET->MAC.SUPP = 0;
}

void Chip_ENET_SetFullDuplex(LPC_ENET_T *pENET)
{
	pENET->MAC.MAC2 |= ENET_MAC2_FULLDUPLEX;
	pENET->CONTROL.COMMAND |= ENET_COMMAND_FULLDUPLEX;
	pENET->MAC.IPGT = ENET_IPGT_FULLDUPLEX;
}

/* Sets half duplex for the ENET interface */
void Chip_ENET_SetHalfDuplex(LPC_ENET_T *pENET)
{
	pENET->MAC.MAC2 &= ~ENET_MAC2_FULLDUPLEX;
	pENET->CONTROL.COMMAND &= ~ENET_COMMAND_FULLDUPLEX;
	pENET->MAC.IPGT = ENET_IPGT_HALFDUPLEX;
}


err_t
etharp_raw(struct netif *netif, const struct eth_addr *ethsrc_addr,
           const struct eth_addr *ethdst_addr,
           const struct eth_addr *hwsrc_addr, const ip_addr_t *ipsrc_addr,
           const struct eth_addr *hwdst_addr, const ip_addr_t *ipdst_addr,
           const u16_t opcode)
{
  struct pbuf *p;
  err_t result = ERR_OK;
  struct eth_hdr *ethhdr;
  struct etharp_hdr *hdr;
#if LWIP_AUTOIP
  const u8_t * ethdst_hwaddr;
#endif /* LWIP_AUTOIP */

  LWIP_ASSERT("netif != NULL", netif != NULL);

  /* allocate a pbuf for the outgoing ARP request packet */
  p = pbuf_alloc(PBUF_RAW, SIZEOF_ETHARP_PACKET, PBUF_RAM);
  /* could allocate a pbuf for an ARP request? */
  if (p == NULL) {
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
      ("etharp_raw: could not allocate pbuf for ARP request.\n"));
    ETHARP_STATS_INC(etharp.memerr);
    return ERR_MEM;
  }
  LWIP_ASSERT("check that first pbuf can hold struct etharp_hdr",
              (p->len >= SIZEOF_ETHARP_PACKET));

  ethhdr = (struct eth_hdr *)p->payload;
  hdr = (struct etharp_hdr *)((u8_t*)ethhdr + SIZEOF_ETH_HDR);
  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_raw: sending raw ARP packet.\n"));
  hdr->opcode = htons(opcode);

  LWIP_ASSERT("netif->hwaddr_len must be the same as ETHARP_HWADDR_LEN for etharp!",
              (netif->hwaddr_len == ETHARP_HWADDR_LEN));
#if LWIP_AUTOIP
  /* If we are using Link-Local, all ARP packets that contain a Link-Local
   * 'sender IP address' MUST be sent using link-layer broadcast instead of
   * link-layer unicast. (See RFC3927 Section 2.5, last paragraph) */
  ethdst_hwaddr = ip_addr_islinklocal(ipsrc_addr) ? (u8_t*)(ethbroadcast.addr) : ethdst_addr->addr;
#endif /* LWIP_AUTOIP */
  /* Write the ARP MAC-Addresses */
  ETHADDR16_COPY(&hdr->shwaddr, hwsrc_addr);
  ETHADDR16_COPY(&hdr->dhwaddr, hwdst_addr);
  /* Write the Ethernet MAC-Addresses */
#if LWIP_AUTOIP
  ETHADDR16_COPY(&ethhdr->dest, ethdst_hwaddr);
#else  /* LWIP_AUTOIP */
  ETHADDR16_COPY(&ethhdr->dest, ethdst_addr);
#endif /* LWIP_AUTOIP */
  ETHADDR16_COPY(&ethhdr->src, ethsrc_addr);
  /* Copy struct ip_addr2 to aligned ip_addr, to support compilers without
   * structure packing. */
  IPADDR2_COPY(&hdr->sipaddr, ipsrc_addr);
  IPADDR2_COPY(&hdr->dipaddr, ipdst_addr);

  hdr->hwtype = PP_HTONS(HWTYPE_ETHERNET);
  hdr->proto = PP_HTONS(ETHTYPE_IP);
  /* set hwlen and protolen */
  hdr->hwlen = ETHARP_HWADDR_LEN;
  hdr->protolen = sizeof(ip_addr_t);

  ethhdr->type = PP_HTONS(ETHTYPE_ARP);
  /* send ARP query */
  result = netif->linkoutput(netif, p);
  ETHARP_STATS_INC(etharp.xmit);
  /* free ARP query packet */
  pbuf_free(p);
  p = NULL;
  /* could not allocate pbuf for ARP request */

  return result;
}

static void
etharp_free_entry(int i)
{
  /* remove from SNMP ARP index tree */
  snmp_delete_arpidx_tree(arp_table[i].netif, &arp_table[i].ipaddr);
  /* and empty packet queue */
  if (arp_table[i].q != NULL) {
    /* remove all queued packets */
    LWIP_DEBUGF(ETHARP_DEBUG, ("etharp_free_entry: freeing entry %"U16_F", packet queue %p.\n", (u16_t)i, (void *)(arp_table[i].q)));
    free_etharp_q(arp_table[i].q);
    arp_table[i].q = NULL;
  }
  /* recycle entry for re-use */
  arp_table[i].state = ETHARP_STATE_EMPTY;
#ifdef LWIP_DEBUG
  /* for debugging, clean out the complete entry */
  arp_table[i].ctime = 0;
  arp_table[i].netif = NULL;
  ip_addr_set_zero(&arp_table[i].ipaddr);
  arp_table[i].ethaddr = ethzero;
#endif /* LWIP_DEBUG */
}

void
etharp_tmr(void)
{
  u8_t i;

  LWIP_DEBUGF(ETHARP_DEBUG, ("etharp_timer\n"));
  /* remove expired entries from the ARP table */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    u8_t state = arp_table[i].state;
    if (state != ETHARP_STATE_EMPTY
#if ETHARP_SUPPORT_STATIC_ENTRIES
      && (state != ETHARP_STATE_STATIC)
#endif /* ETHARP_SUPPORT_STATIC_ENTRIES */
      ) {
      arp_table[i].ctime++;
      if ((arp_table[i].ctime >= ARP_MAXAGE) ||
          ((arp_table[i].state == ETHARP_STATE_PENDING)  &&
           (arp_table[i].ctime >= ARP_MAXPENDING))) {
        /* pending or stable entry has become old! */
        LWIP_DEBUGF(ETHARP_DEBUG, ("etharp_timer: expired %s entry %"U16_F".\n",
             arp_table[i].state >= ETHARP_STATE_STABLE ? "stable" : "pending", (u16_t)i));
        /* clean up entries that have just been expired */
        etharp_free_entry(i);
      }
      else if (arp_table[i].state == ETHARP_STATE_STABLE_REREQUESTING) {
        /* Reset state to stable, so that the next transmitted packet will
           re-send an ARP request. */
        arp_table[i].state = ETHARP_STATE_STABLE;
      }
#if ARP_QUEUEING
      /* still pending entry? (not expired) */
      if (arp_table[i].state == ETHARP_STATE_PENDING) {
        /* resend an ARP query here? */
      }
#endif /* ARP_QUEUEING */
    }
  }
}

err_t
etharp_request(struct netif *netif, ip_addr_t *ipaddr)
{
  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_request: sending ARP request.\n"));
  return etharp_raw(netif, (struct eth_addr *)netif->hwaddr, &ethbroadcast,
                    (struct eth_addr *)netif->hwaddr, &netif->ip_addr, &ethzero,
                    ipaddr, ARP_REQUEST);
}

static err_t
etharp_output_to_arp_index(struct netif *netif, struct pbuf *q, u8_t arp_idx)
{
  LWIP_ASSERT("arp_table[arp_idx].state >= ETHARP_STATE_STABLE",
              arp_table[arp_idx].state >= ETHARP_STATE_STABLE);
  /* if arp table entry is about to expire: re-request it,
     but only if its state is ETHARP_STATE_STABLE to prevent flooding the
     network with ARP requests if this address is used frequently. */
  if ((arp_table[arp_idx].state == ETHARP_STATE_STABLE) &&
      (arp_table[arp_idx].ctime >= ARP_AGE_REREQUEST_USED)) {
    if (etharp_request(netif, &arp_table[arp_idx].ipaddr) == ERR_OK) {
      arp_table[arp_idx].state = ETHARP_STATE_STABLE_REREQUESTING;
    }
  }

  return etharp_send_ip(netif, q, (struct eth_addr*)(netif->hwaddr),
    &arp_table[arp_idx].ethaddr);
}

static s8_t
etharp_find_entry(ip_addr_t *ipaddr, u8_t flags)
{
  s8_t old_pending = ARP_TABLE_SIZE, old_stable = ARP_TABLE_SIZE;
  s8_t empty = ARP_TABLE_SIZE;
  u8_t i = 0, age_pending = 0, age_stable = 0;
  /* oldest entry with packets on queue */
  s8_t old_queue = ARP_TABLE_SIZE;
  /* its age */
  u8_t age_queue = 0;

  /**
   * a) do a search through the cache, remember candidates
   * b) select candidate entry
   * c) create new entry
   */

  /* a) in a single search sweep, do all of this
   * 1) remember the first empty entry (if any)
   * 2) remember the oldest stable entry (if any)
   * 3) remember the oldest pending entry without queued packets (if any)
   * 4) remember the oldest pending entry with queued packets (if any)
   * 5) search for a matching IP entry, either pending or stable
   *    until 5 matches, or all entries are searched for.
   */

  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    u8_t state = arp_table[i].state;
    /* no empty entry found yet and now we do find one? */
    if ((empty == ARP_TABLE_SIZE) && (state == ETHARP_STATE_EMPTY)) {
      LWIP_DEBUGF(ETHARP_DEBUG, ("etharp_find_entry: found empty entry %"U16_F"\n", (u16_t)i));
      /* remember first empty entry */
      empty = i;
    } else if (state != ETHARP_STATE_EMPTY) {
      LWIP_ASSERT("state == ETHARP_STATE_PENDING || state >= ETHARP_STATE_STABLE",
        state == ETHARP_STATE_PENDING || state >= ETHARP_STATE_STABLE);
      /* if given, does IP address match IP address in ARP entry? */
      if (ipaddr && ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
        LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_find_entry: found matching entry %"U16_F"\n", (u16_t)i));
        /* found exact IP address match, simply bail out */
        return i;
      }
      /* pending entry? */
      if (state == ETHARP_STATE_PENDING) {
        /* pending with queued packets? */
        if (arp_table[i].q != NULL) {
          if (arp_table[i].ctime >= age_queue) {
            old_queue = i;
            age_queue = arp_table[i].ctime;
          }
        } else
        /* pending without queued packets? */
        {
          if (arp_table[i].ctime >= age_pending) {
            old_pending = i;
            age_pending = arp_table[i].ctime;
          }
        }
      /* stable entry? */
      } else if (state >= ETHARP_STATE_STABLE) {
#if ETHARP_SUPPORT_STATIC_ENTRIES
        /* don't record old_stable for static entries since they never expire */
        if (state < ETHARP_STATE_STATIC)
#endif /* ETHARP_SUPPORT_STATIC_ENTRIES */
        {
          /* remember entry with oldest stable entry in oldest, its age in maxtime */
          if (arp_table[i].ctime >= age_stable) {
            old_stable = i;
            age_stable = arp_table[i].ctime;
          }
        }
      }
    }
  }
  /* { we have no match } => try to create a new entry */

  /* don't create new entry, only search? */
  if (((flags & ETHARP_FLAG_FIND_ONLY) != 0) ||
      /* or no empty entry found and not allowed to recycle? */
      ((empty == ARP_TABLE_SIZE) && ((flags & ETHARP_FLAG_TRY_HARD) == 0))) {
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_find_entry: no empty entry found and not allowed to recycle\n"));
    return (s8_t)ERR_MEM;
  }

  /* b) choose the least destructive entry to recycle:
   * 1) empty entry
   * 2) oldest stable entry
   * 3) oldest pending entry without queued packets
   * 4) oldest pending entry with queued packets
   *
   * { ETHARP_FLAG_TRY_HARD is set at this point }
   */

  /* 1) empty entry available? */
  if (empty < ARP_TABLE_SIZE) {
    i = empty;
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_find_entry: selecting empty entry %"U16_F"\n", (u16_t)i));
  } else {
    /* 2) found recyclable stable entry? */
    if (old_stable < ARP_TABLE_SIZE) {
      /* recycle oldest stable*/
      i = old_stable;
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_find_entry: selecting oldest stable entry %"U16_F"\n", (u16_t)i));
      /* no queued packets should exist on stable entries */
      LWIP_ASSERT("arp_table[i].q == NULL", arp_table[i].q == NULL);
    /* 3) found recyclable pending entry without queued packets? */
    } else if (old_pending < ARP_TABLE_SIZE) {
      /* recycle oldest pending */
      i = old_pending;
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_find_entry: selecting oldest pending entry %"U16_F" (without queue)\n", (u16_t)i));
    /* 4) found recyclable pending entry with queued packets? */
    } else if (old_queue < ARP_TABLE_SIZE) {
      /* recycle oldest pending (queued packets are free in etharp_free_entry) */
      i = old_queue;
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_find_entry: selecting oldest pending entry %"U16_F", freeing packet queue %p\n", (u16_t)i, (void *)(arp_table[i].q)));
      /* no empty or recyclable entries found */
    } else {
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_find_entry: no empty or recyclable entries found\n"));
      return (s8_t)ERR_MEM;
    }

    /* { empty or recyclable entry found } */
    LWIP_ASSERT("i < ARP_TABLE_SIZE", i < ARP_TABLE_SIZE);
    etharp_free_entry(i);
  }

  LWIP_ASSERT("i < ARP_TABLE_SIZE", i < ARP_TABLE_SIZE);
  LWIP_ASSERT("arp_table[i].state == ETHARP_STATE_EMPTY",
    arp_table[i].state == ETHARP_STATE_EMPTY);

  /* IP address given? */
  if (ipaddr != NULL) {
    /* set IP address */
    ip_addr_copy(arp_table[i].ipaddr, *ipaddr);
  }
  arp_table[i].ctime = 0;
  return (err_t)i;
}


err_t
etharp_query(struct netif *netif, ip_addr_t *ipaddr, struct pbuf *q)
{
  struct eth_addr * srcaddr = (struct eth_addr *)netif->hwaddr;
  err_t result = ERR_MEM;
  s8_t i; /* ARP entry index */

  /* non-unicast address? */
  if (ip_addr_isbroadcast(ipaddr, netif) ||
      ip_addr_ismulticast(ipaddr) ||
      ip_addr_isany(ipaddr)) {
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_query: will not add non-unicast IP address to ARP cache\n"));
    return ERR_ARG;
  }

  /* find entry in ARP cache, ask to create entry if queueing packet */
  i = etharp_find_entry(ipaddr, ETHARP_FLAG_TRY_HARD);

  /* could not find or create entry? */
  if (i < 0) {
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_query: could not create ARP entry\n"));
    if (q) {
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_query: packet dropped\n"));
      ETHARP_STATS_INC(etharp.memerr);
    }
    return (err_t)i;
  }

  /* mark a fresh entry as pending (we just sent a request) */
  if (arp_table[i].state == ETHARP_STATE_EMPTY) {
    arp_table[i].state = ETHARP_STATE_PENDING;
  }

  /* { i is either a STABLE or (new or existing) PENDING entry } */
  LWIP_ASSERT("arp_table[i].state == PENDING or STABLE",
  ((arp_table[i].state == ETHARP_STATE_PENDING) ||
   (arp_table[i].state >= ETHARP_STATE_STABLE)));

  /* do we have a pending entry? or an implicit query request? */
  if ((arp_table[i].state == ETHARP_STATE_PENDING) || (q == NULL)) {
    /* try to resolve it; send out ARP request */
    result = etharp_request(netif, ipaddr);
    if (result != ERR_OK) {
      /* ARP request couldn't be sent */
      /* We don't re-send arp request in etharp_tmr, but we still queue packets,
         since this failure could be temporary, and the next packet calling
         etharp_query again could lead to sending the queued packets. */
    }
    if (q == NULL) {
      return result;
    }
  }

  /* packet given? */
  LWIP_ASSERT("q != NULL", q != NULL);
  /* stable entry? */
  if (arp_table[i].state >= ETHARP_STATE_STABLE) {
    /* we have a valid IP->Ethernet address mapping */
    ETHARP_SET_HINT(netif, i);
    /* send the packet */
    result = etharp_send_ip(netif, q, srcaddr, &(arp_table[i].ethaddr));
  /* pending entry? (either just created or already pending */
  } else if (arp_table[i].state == ETHARP_STATE_PENDING) {
    /* entry is still pending, queue the given packet 'q' */
    struct pbuf *p;
    int copy_needed = 0;
    /* IF q includes a PBUF_REF, PBUF_POOL or PBUF_RAM, we have no choice but
     * to copy the whole queue into a new PBUF_RAM (see bug #11400)
     * PBUF_ROMs can be left as they are, since ROM must not get changed. */
    p = q;
    while (p) {
      LWIP_ASSERT("no packet queues allowed!", (p->len != p->tot_len) || (p->next == 0));
      if(p->type != PBUF_ROM) {
        copy_needed = 1;
        break;
      }
      p = p->next;
    }
    if(copy_needed) {
      /* copy the whole packet into new pbufs */
      p = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
      if(p != NULL) {
        if (pbuf_copy(p, q) != ERR_OK) {
          pbuf_free(p);
          p = NULL;
        }
      }
    } else {
      /* referencing the old pbuf is enough */
      p = q;
      pbuf_ref(p);
    }
    /* packet could be taken over? */
    if (p != NULL) {
      /* queue packet ... */
#if ARP_QUEUEING
      struct etharp_q_entry *new_entry;
      /* allocate a new arp queue entry */
      new_entry = (struct etharp_q_entry *)memp_malloc(MEMP_ARP_QUEUE);
      if (new_entry != NULL) {
        new_entry->next = 0;
        new_entry->p = p;
        if(arp_table[i].q != NULL) {
          /* queue was already existent, append the new entry to the end */
          struct etharp_q_entry *r;
          r = arp_table[i].q;
          while (r->next != NULL) {
            r = r->next;
          }
          r->next = new_entry;
        } else {
          /* queue did not exist, first item in queue */
          arp_table[i].q = new_entry;
        }
        LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_query: queued packet %p on ARP entry %"S16_F"\n", (void *)q, (s16_t)i));
        result = ERR_OK;
      } else {
        /* the pool MEMP_ARP_QUEUE is empty */
        pbuf_free(p);
        LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_query: could not queue a copy of PBUF_REF packet %p (out of memory)\n", (void *)q));
        result = ERR_MEM;
      }
#else /* ARP_QUEUEING */
      /* always queue one packet per ARP request only, freeing a previously queued packet */
      if (arp_table[i].q != NULL) {
        LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_query: dropped previously queued packet %p for ARP entry %"S16_F"\n", (void *)q, (s16_t)i));
        pbuf_free(arp_table[i].q);
      }
      arp_table[i].q = p;
      result = ERR_OK;
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_query: queued packet %p on ARP entry %"S16_F"\n", (void *)q, (s16_t)i));
#endif /* ARP_QUEUEING */
    } else {
      ETHARP_STATS_INC(etharp.memerr);
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_query: could not queue a copy of PBUF_REF packet %p (out of memory)\n", (void *)q));
      result = ERR_MEM;
    }
  }
  return result;
}

err_t
etharp_output(struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr)
{
  struct eth_addr *dest;
  struct eth_addr mcastaddr;
  ip_addr_t *dst_addr = ipaddr;

  LWIP_ASSERT("netif != NULL", netif != NULL);
  LWIP_ASSERT("q != NULL", q != NULL);
  LWIP_ASSERT("ipaddr != NULL", ipaddr != NULL);

  /* make room for Ethernet header - should not fail */
  if (pbuf_header(q, sizeof(struct eth_hdr)) != 0) {
    /* bail out */
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
      ("etharp_output: could not allocate room for header.\n"));
    LINK_STATS_INC(link.lenerr);
    return ERR_BUF;
  }

  /* Determine on destination hardware address. Broadcasts and multicasts
   * are special, other IP addresses are looked up in the ARP table. */

  /* broadcast destination IP address? */
  if (ip_addr_isbroadcast(ipaddr, netif)) {
    /* broadcast on Ethernet also */
    dest = (struct eth_addr *)&ethbroadcast;
  /* multicast destination IP address? */
  } else if (ip_addr_ismulticast(ipaddr)) {
    /* Hash IP multicast address to MAC address.*/
    mcastaddr.addr[0] = LL_MULTICAST_ADDR_0;
    mcastaddr.addr[1] = LL_MULTICAST_ADDR_1;
    mcastaddr.addr[2] = LL_MULTICAST_ADDR_2;
    mcastaddr.addr[3] = ip4_addr2(ipaddr) & 0x7f;
    mcastaddr.addr[4] = ip4_addr3(ipaddr);
    mcastaddr.addr[5] = ip4_addr4(ipaddr);
    /* destination Ethernet address is multicast */
    dest = &mcastaddr;
  /* unicast destination IP address? */
  } else {
    s8_t i;
    /* outside local network? if so, this can neither be a global broadcast nor
       a subnet broadcast. */
    if (!ip_addr_netcmp(ipaddr, &(netif->ip_addr), &(netif->netmask)) &&
        !ip_addr_islinklocal(ipaddr)) {
#if LWIP_AUTOIP
      struct ip_hdr *iphdr = (struct ip_hdr*)((u8_t*)q->payload +
        sizeof(struct eth_hdr));
      /* According to RFC 3297, chapter 2.6.2 (Forwarding Rules), a packet with
         a link-local source address must always be "directly to its destination
         on the same physical link. The host MUST NOT send the packet to any
         router for forwarding". */
      if (!ip_addr_islinklocal(&iphdr->src))
#endif /* LWIP_AUTOIP */
      {
        /* interface has default gateway? */
        if (!ip_addr_isany(&netif->gw)) {
          /* send to hardware address of default gateway IP address */
          dst_addr = &(netif->gw);
        /* no default gateway available */
        } else {
          /* no route to destination error (default gateway missing) */
          return ERR_RTE;
        }
      }
    }
#if LWIP_NETIF_HWADDRHINT
    if (netif->addr_hint != NULL) {
      /* per-pcb cached entry was given */
      u8_t etharp_cached_entry = *(netif->addr_hint);
      if (etharp_cached_entry < ARP_TABLE_SIZE) {
#endif /* LWIP_NETIF_HWADDRHINT */
        if ((arp_table[etharp_cached_entry].state >= ETHARP_STATE_STABLE) &&
            (ip_addr_cmp(dst_addr, &arp_table[etharp_cached_entry].ipaddr))) {
          /* the per-pcb-cached entry is stable and the right one! */
          ETHARP_STATS_INC(etharp.cachehit);
          return etharp_output_to_arp_index(netif, q, etharp_cached_entry);
        }
#if LWIP_NETIF_HWADDRHINT
      }
    }
#endif /* LWIP_NETIF_HWADDRHINT */

    /* find stable entry: do this here since this is a critical path for
       throughput and etharp_find_entry() is kind of slow */
    for (i = 0; i < ARP_TABLE_SIZE; i++) {
      if ((arp_table[i].state >= ETHARP_STATE_STABLE) &&
          (ip_addr_cmp(dst_addr, &arp_table[i].ipaddr))) {
        /* found an existing, stable entry */
        ETHARP_SET_HINT(netif, i);
        return etharp_output_to_arp_index(netif, q, i);
      }
    }
    /* no stable entry found, use the (slower) query function:
       queue on destination Ethernet address belonging to ipaddr */
    return etharp_query(netif, dst_addr, q);
  }

  /* continuation for multicast/broadcast destinations */
  /* obtain source Ethernet address of the given interface */
  /* send packet directly on the link */
  return etharp_send_ip(netif, q, (struct eth_addr*)(netif->hwaddr), dest);
}


static err_t lpc_etharp_output(struct netif *netif, struct pbuf *q,
							   ip_addr_t *ipaddr)
{
	/* Only send packet is link is up */
	if (netif->flags & NETIF_FLAG_LINK_UP) {
		return etharp_output(netif, q, ipaddr);
	}

	return ERR_CONN;
}

static inline uint8_t Chip_Clock_IsMainPLLEnabled(void)
{
#if defined(CHIP_LPC175X_6X)
	return (uint8_t) ((LPC_SYSCTL->PLL[0].PLLSTAT & SYSCTL_PLL0STS_ENABLED) != 0);
#else
	return (uint8_t) ((LPC_SYSCTL->PLL[0].PLLSTAT & SYSCTL_PLLSTS_ENABLED) != 0);
#endif
}

static inline uint8_t Chip_Clock_IsMainPLLConnected(void)
{
	return (uint8_t) ((LPC_SYSCTL->PLL[0].PLLSTAT & SYSCTL_PLL0STS_CONNECTED) != 0);
}

/* Returns the current CPU clock source */
CHIP_SYSCTL_CCLKSRC_T Chip_Clock_GetCPUClockSource(void)
{
	CHIP_SYSCTL_CCLKSRC_T src;
#if defined(CHIP_LPC175X_6X)
	/* LPC175x/6x CPU clock source is based on PLL connect status */
	if (Chip_Clock_IsMainPLLConnected()) {
		src = SYSCTL_CCLKSRC_MAINPLL;
	}
	else {
		src = SYSCTL_CCLKSRC_SYSCLK;
	}
#else
	/* LPC177x/8x and 407x/8x CPU clock source is based on CCLKSEL */
	if (LPC_SYSCTL->CCLKSEL & (1 << 8)) {
		src = SYSCTL_CCLKSRC_MAINPLL;
	}
	else {
		src = SYSCTL_CCLKSRC_SYSCLK;
	}
#endif

	return src;
}

static inline CHIP_SYSCTL_PLLCLKSRC_T Chip_Clock_GetMainPLLSource(void)
{
	return (CHIP_SYSCTL_PLLCLKSRC_T) (LPC_SYSCTL->CLKSRCSEL & 0x3);
}

static inline uint32_t Chip_Clock_GetIntOscRate(void)
{
	return SYSCTL_IRC_FREQ;
}

static inline uint32_t Chip_Clock_GetMainOscRate(void)
{
	return 12000000;
}

static inline uint32_t Chip_Clock_GetRTCOscRate(void)
{
	return 32768;
}

/* Returns the current SYSCLK clock rate */
uint32_t Chip_Clock_GetSYSCLKRate(void)
{
	/* Determine clock input rate to SYSCLK based on input selection */
	switch (Chip_Clock_GetMainPLLSource()) {
	case (uint32_t) SYSCTL_PLLCLKSRC_IRC:
		return Chip_Clock_GetIntOscRate();

	case (uint32_t) SYSCTL_PLLCLKSRC_MAINOSC:
		return Chip_Clock_GetMainOscRate();

#if defined(CHIP_LPC175X_6X)
	case (uint32_t) SYSCTL_PLLCLKSRC_RTC:
		return Chip_Clock_GetRTCOscRate();
#endif
	}
	return 0;
}

static inline uint32_t Chip_Clock_GetMainPLLInClockRate(void)
{
	return Chip_Clock_GetSYSCLKRate();
}

/* Returns the main PLL output clock rate */
uint32_t Chip_Clock_GetMainPLLOutClockRate(void)
{
	uint32_t clkhr = 0;

#if defined(CHIP_LPC175X_6X)
	/* Only valid if enabled */
	if (Chip_Clock_IsMainPLLEnabled()) {
		uint32_t msel, nsel;

		/* PLL0 rate is (FIN * 2 * MSEL) / NSEL, get MSEL and NSEL */
		msel = 1 + (LPC_SYSCTL->PLL[SYSCTL_MAIN_PLL].PLLCFG & 0x7FFF);
		nsel = 1 + ((LPC_SYSCTL->PLL[SYSCTL_MAIN_PLL].PLLCFG >> 16) & 0xFF);
		clkhr = (Chip_Clock_GetMainPLLInClockRate() * 2 * msel) / nsel;
	}
#else
	if (Chip_Clock_IsMainPLLEnabled()) {
		uint32_t msel;

		/* PLL0 rate is (FIN * MSEL) */
		msel = 1 + (LPC_SYSCTL->PLL[SYSCTL_MAIN_PLL].PLLCFG & 0x1F);
		clkhr = (Chip_Clock_GetMainPLLInClockRate() * msel);
	}
#endif

	return (uint32_t) clkhr;
}



uint32_t Chip_Clock_GetMainClockRate(void)
{
	switch (Chip_Clock_GetCPUClockSource()) {
	case SYSCTL_CCLKSRC_MAINPLL:
		return Chip_Clock_GetMainPLLOutClockRate();

	case SYSCTL_CCLKSRC_SYSCLK:
		return Chip_Clock_GetSYSCLKRate();

	default:
		return 0;
	}
}

/* Gets the CPU clock divider */
uint32_t Chip_Clock_GetCPUClockDiv(void)
{
#if defined(CHIP_LPC175X_6X)
	return (LPC_SYSCTL->CCLKSEL & 0xFF) + 1;
#else
	return LPC_SYSCTL->CCLKSEL & 0x1F;
#endif
}

/* Get CCLK rate */
uint32_t Chip_Clock_GetSystemClockRate(void)
{
	return Chip_Clock_GetMainClockRate() / Chip_Clock_GetCPUClockDiv();
}

static inline uint32_t Chip_Clock_GetENETClockRate(void)
{
	return Chip_Clock_GetSystemClockRate();
}

uint32_t Chip_ENET_FindMIIDiv(LPC_ENET_T *pENET, uint32_t clockRate)
{
	uint32_t tmp, divIdx = 0;

	/* Find desired divider value */
	tmp = Chip_Clock_GetENETClockRate() / clockRate;

	/* Determine divider index from desired divider */
	for (divIdx = 0; divIdx < (sizeof(EnetClkDiv) / sizeof(EnetClkDiv[0])); divIdx++) {
		/* Closest index, but not higher than desired rate */
		if (EnetClkDiv[divIdx] >= tmp) {
			return divIdx;
		}
	}

	/* Use maximum divider index */
	return (sizeof(EnetClkDiv) / sizeof(EnetClkDiv[0])) - 1;
}


void Chip_Clock_EnablePeriphClock(CHIP_SYSCTL_CLOCK_T clk) {
	uint32_t bs = (uint32_t) clk;

#if defined(CHIP_LPC40XX)
	if (bs >= 32) {
		LPC_SYSCTL->PCONP1 |= (1 << (bs - 32));
	}
	else {
		LPC_SYSCTL->PCONP |= (1 << bs);
	}
#else
	LPC_SYSCTL->PCONP |= (1 << bs);
#endif
}

uint16_t Chip_ENET_IncTXProduceIndex(LPC_ENET_T *pENET)
{
	/* Get current TX produce index */
	uint32_t idx = pENET->CONTROL.TX.PRODUCEINDEX;

	/* Start frame transmission by incrementing descriptor */
	idx++;
	if (idx > pENET->CONTROL.TX.DESCRIPTORNUMBER) {
		idx = 0;
	}
	pENET->CONTROL.TX.PRODUCEINDEX = idx;

	return idx;
}

static inline uint16_t Chip_ENET_GetTXProduceIndex(LPC_ENET_T *pENET)
{
	return pENET->CONTROL.TX.PRODUCEINDEX;
}

static int32_t lpc_packet_addr_notsafe(void *addr) {
#if defined(CHIP_LPC175X_6X)
	/* Check for legal address ranges */
	if ((((u32_t) addr >= 0x10000000) && ((u32_t) addr < 0x10008000)) /* 32kB local SRAM */
		|| (((u32_t) addr >= 0x1FFF0000) && ((u32_t) addr < 0x1FFF2000)) /* 8kB ROM */
		|| (((u32_t) addr >= 0x2007C000) && ((u32_t) addr < 0x20084000)) /* 32kB AHB SRAM */
		) {
		return 0;
	}
	return 1;
#else
	/* On the LPC177x_8x and LPC40xx the only areas that are safe are... */
	if ((((u32_t) addr >= 0x20000000) && ((u32_t) addr < 0x20008000)) /* 32kB peripheral SRAM */
		|| (((u32_t) addr >= 0xa0000000) && ((u32_t) addr < 0xe0000000)) /* DRAM 1 GB */
		) {
		return 0;
	}
	/* Everything else is not safe */
	return 1;
#endif
}

static inline uint16_t Chip_ENET_GetTXConsumeIndex(LPC_ENET_T *pENET)
{
	return pENET->CONTROL.TX.CONSUMEINDEX;
}

static void lpc_tx_reclaim_st(lpc_enetdata_t *lpc_enetif, u32_t cidx)
{
#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_enetif->tx_lock_mutex);
#endif

	while (cidx != lpc_enetif->lpc_last_tx_idx) {
		if (lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx] != NULL) {
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_tx_reclaim_st: Freeing packet %p (index %d)\n",
						 lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx],
						 lpc_enetif->lpc_last_tx_idx));
			pbuf_free(lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx]);
			lpc_enetif->txb[lpc_enetif->lpc_last_tx_idx] = NULL;
		}

#if NO_SYS == 0
		xSemaphoreGive(lpc_enetif->xtx_count_sem);
#endif
		lpc_enetif->lpc_last_tx_idx++;
		if (lpc_enetif->lpc_last_tx_idx >= LPC_NUM_BUFF_TXDESCS) {
			lpc_enetif->lpc_last_tx_idx = 0;
		}
	}

#if NO_SYS == 0
	/* Restore access */
	sys_mutex_unlock(&lpc_enetif->tx_lock_mutex);
#endif
}


void lpc_tx_reclaim(struct netif *netif)
{
	lpc_tx_reclaim_st((lpc_enetdata_t *) netif->state,
					  Chip_ENET_GetTXConsumeIndex(LPC_ETHERNET));
}

/* Get the number of descriptor filled */
uint32_t Chip_ENET_GetFillDescNum(LPC_ENET_T *pENET, uint16_t produceIndex, uint16_t consumeIndex, uint16_t buffSize)
{
	/* Empty descriptor list */
	if (consumeIndex == produceIndex) {
		return 0;
	}

	if (consumeIndex > produceIndex) {
		return (buffSize - consumeIndex) + produceIndex;
	}

	return produceIndex - consumeIndex;
}

static inline uint32_t Chip_ENET_GetFreeDescNum(LPC_ENET_T *pENET,
												uint16_t produceIndex,
												uint16_t consumeIndex,
												uint16_t buffSize)
{
	return buffSize - 1 - Chip_ENET_GetFillDescNum(pENET, produceIndex, consumeIndex, buffSize);
}

uint32_t lpc_tx_ready(struct netif *netif)
{
	u32_t pidx, cidx;

	cidx = Chip_ENET_GetTXConsumeIndex(LPC_ETHERNET);
	pidx = Chip_ENET_GetTXProduceIndex(LPC_ETHERNET);

	return Chip_ENET_GetFreeDescNum(LPC_ETHERNET, pidx, cidx, LPC_NUM_BUFF_TXDESCS);
}

err_t lpc_low_level_output(struct netif *netif, struct pbuf *p)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *q;

#if LPC_TX_PBUF_BOUNCE_EN == 1
	u8_t *dst;
	struct pbuf *np;
#endif
	u32_t idx;
	u32_t dn, notdmasafe = 0;

	/* Zero-copy TX buffers may be fragmented across mutliple payload
	   chains. Determine the number of descriptors needed for the
	   transfer. The pbuf chaining can be a mess! */
	dn = (u32_t) pbuf_clen(p);

	/* Test to make sure packet addresses are DMA safe. A DMA safe
	   address is once that uses external memory or periphheral RAM.
	   IRAM and FLASH are not safe! */
	for (q = p; q != NULL; q = q->next) {
		notdmasafe += lpc_packet_addr_notsafe(q->payload);
	}

#if LPC_TX_PBUF_BOUNCE_EN == 1
	/* If the pbuf is not DMA safe, a new bounce buffer (pbuf) will be
	   created that will be used instead. This requires an copy from the
	   non-safe DMA region to the new pbuf */
	if (notdmasafe) {
		/* Allocate a pbuf in DMA memory */
		np = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
		if (np == NULL) {
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_low_level_output: could not allocate TX pbuf\n"));
			return ERR_MEM;
		}

		/* This buffer better be contiguous! */
		LWIP_ASSERT("lpc_low_level_output: New transmit pbuf is chained",
					(pbuf_clen(np) == 1));

		/* Copy to DMA safe pbuf */
		dst = (u8_t *) np->payload;
		for (q = p; q != NULL; q = q->next) {
			/* Copy the buffer to the descriptor's buffer */
			MEMCPY(dst, (u8_t *) q->payload, q->len);
			dst += q->len;
		}
		np->len = p->tot_len;

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
					("lpc_low_level_output: Switched to DMA safe buffer, old=%p, new=%p\n",
					 q, np));

		/* use the new buffer for descrptor queueing. The original pbuf will
		   be de-allocated outsuide this driver. */
		p = np;
		dn = 1;
	}
#else
	if (notdmasafe) {
		LWIP_ASSERT("lpc_low_level_output: Not a DMA safe pbuf",
					(notdmasafe == 0));
	}
#endif

	/* Wait until enough descriptors are available for the transfer. */
	/* THIS WILL BLOCK UNTIL THERE ARE ENOUGH DESCRIPTORS AVAILABLE */
	while (dn > lpc_tx_ready(netif)) {
#if NO_SYS == 0
		xSemaphoreTake(lpc_enetif->xtx_count_sem, 0);
#else
		msDelay(1);
#endif
	}

	/* Get free TX buffer index */
	idx = Chip_ENET_GetTXProduceIndex(LPC_ETHERNET);

#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_enetif->tx_lock_mutex);
#endif

	/* Prevent LWIP from de-allocating this pbuf. The driver will
	   free it once it's been transmitted. */
	if (!notdmasafe) {
		pbuf_ref(p);
	}

	/* Setup transfers */
	q = p;
	while (dn > 0) {
		dn--;

		/* Only save pointer to free on last descriptor */
		if (dn == 0) {
			/* Save size of packet and signal it's ready */
			lpc_enetif->ptxd[idx].Control = ENET_TCTRL_SIZE(q->len) | ENET_TCTRL_INT |
											ENET_TCTRL_LAST;
			lpc_enetif->txb[idx] = p;
		}
		else {
			/* Save size of packet, descriptor is not last */
			lpc_enetif->ptxd[idx].Control = ENET_TCTRL_SIZE(q->len) | ENET_TCTRL_INT;
			lpc_enetif->txb[idx] = NULL;
		}

		LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
					("lpc_low_level_output: pbuf packet(%p) sent, chain#=%d,"
					 " size = %d (index=%d)\n", q->payload, dn, q->len, idx));

		lpc_enetif->ptxd[idx].Packet = (u32_t) q->payload;

		q = q->next;

		idx = Chip_ENET_IncTXProduceIndex(LPC_ETHERNET);
	}

	LINK_STATS_INC(link.xmit);

#if NO_SYS == 0
	/* Restore access */
	sys_mutex_unlock(&lpc_enetif->tx_lock_mutex);
#endif

	return ERR_OK;
}

void Board_ENET_GetMacADDR(uint8_t *mcaddr)
{
	const uint8_t boardmac[] = {0x00, 0x60, 0x37, 0x12, 0x34, 0x56};

	memcpy(mcaddr, boardmac, 6);
}

static inline void Chip_ENET_Reset(LPC_ENET_T *pENET)
{
	/* This should be called prior to IP_ENET_Init. The MAC controller may
	   not be ready for a call to init right away so a small delay should
	   occur after this call. */
	pENET->MAC.MAC1 = ENET_MAC1_RESETTX | ENET_MAC1_RESETMCSTX | ENET_MAC1_RESETRX |
					  ENET_MAC1_RESETMCSRX | ENET_MAC1_SIMRESET | ENET_MAC1_SOFTRESET;
	pENET->CONTROL.COMMAND = ENET_COMMAND_REGRESET | ENET_COMMAND_TXRESET | ENET_COMMAND_RXRESET |
							 ENET_COMMAND_PASSRUNTFRAME;
}

static inline void resetENET(LPC_ENET_T *pENET)
{
	volatile uint32_t i;

#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
	Chip_SYSCTL_PeriphReset(SYSCTL_RESET_ENET);
#endif

	/* Reset ethernet peripheral */
	Chip_ENET_Reset(pENET);
	for (i = 0; i < 100; i++) {}
}


void Chip_ENET_Init(LPC_ENET_T *pENET, uint8_t useRMII)
{
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_ENET);
	resetENET(pENET);

	/* Initial MAC configuration for  full duplex,
	   100Mbps, inter-frame gap use default values */
	pENET->MAC.MAC1 = ENET_MAC1_PARF;
	pENET->MAC.MAC2 = ENET_MAC2_FULLDUPLEX | ENET_MAC2_CRCEN | ENET_MAC2_PADCRCEN;

	if (useRMII) {
		pENET->CONTROL.COMMAND = ENET_COMMAND_FULLDUPLEX | ENET_COMMAND_PASSRUNTFRAME | ENET_COMMAND_RMII;
	}
	else {
		pENET->CONTROL.COMMAND = ENET_COMMAND_FULLDUPLEX | ENET_COMMAND_PASSRUNTFRAME;
	}

	pENET->MAC.IPGT = ENET_IPGT_FULLDUPLEX;
	pENET->MAC.IPGR = ENET_IPGR_P2_DEF;
	pENET->MAC.SUPP = ENET_SUPP_100Mbps_SPEED;
	pENET->MAC.MAXF = ENET_ETH_MAX_FLEN;
	pENET->MAC.CLRT = ENET_CLRT_DEF;

	/* Setup default filter */
	pENET->CONTROL.COMMAND |= ENET_COMMAND_PASSRXFILTER;

	/* Clear all MAC interrupts */
	pENET->MODULE_CONTROL.INTCLEAR = 0xFFFF;

	/* Disable MAC interrupts */
	pENET->MODULE_CONTROL.INTENABLE = 0;
}

void Chip_ENET_SetupMII(LPC_ENET_T *pENET, uint32_t div, uint8_t addr)
{
	/* Save clock divider and PHY address in MII address register */
	phyAddr = ENET_MADR_PHYADDR(addr);

	/*  Write to MII configuration register and reset */
	pENET->MAC.MCFG = ENET_MCFG_CLOCKSEL(div) | ENET_MCFG_RES_MII;

	/* release reset */
	pENET->MAC.MCFG &= ~(ENET_MCFG_RES_MII);
}

static p_msDelay_func_t pDelayMs;
static uint32_t physts, olddphysts;
static int32_t phyustate;

void Chip_ENET_StartMIIWrite(LPC_ENET_T *pENET, uint8_t reg, uint16_t data)
{
	/* Write value at PHY address and register */
	pENET->MAC.MCMD = 0;
	pENET->MAC.MADR = phyAddr | ENET_MADR_REGADDR(reg);
	pENET->MAC.MWTD = data;
}

static inline uint8_t Chip_ENET_IsMIIBusy(LPC_ENET_T *pENET)
{
	return (pENET->MAC.MIND & ENET_MIND_BUSY) ? true : false;
}

static uint8_t lpc_mii_write(uint8_t reg, uint16_t data)
{
	uint8_t sts = ERROR;
	int32_t mst = 250;

	/* Write value for register */
	Chip_ENET_StartMIIWrite(LPC_ETHERNET, reg, data);

	/* Wait for unbusy status */
	while (mst > 0) {
		if (Chip_ENET_IsMIIBusy(LPC_ETHERNET)) {
			mst--;
			pDelayMs(1);
		}
		else {
			mst = 0;
			sts = SUCCESS;
		}
	}

	return sts;
}

void Chip_ENET_StartMIIRead(LPC_ENET_T *pENET, uint8_t reg)
{
	/* Read value at PHY address and register */
	pENET->MAC.MADR = phyAddr | ENET_MADR_REGADDR(reg);
	pENET->MAC.MCMD = ENET_MCMD_READ;
}


uint16_t Chip_ENET_ReadMIIData(LPC_ENET_T *pENET)
{
	pENET->MAC.MCMD = 0;
	return pENET->MAC.MRDD;
}


/* Read from the PHY. Will block for delays based on the pDelayMs function. Returns
   true on success, or false on failure */
static uint8_t lpc_mii_read(uint8_t reg, uint16_t *data)
{
	uint8_t sts = ERROR;
	int32_t mst = 250;

	/* Start register read */
	Chip_ENET_StartMIIRead(LPC_ETHERNET, reg);

	/* Wait for unbusy status */
	while (mst > 0) {
		if (!Chip_ENET_IsMIIBusy(LPC_ETHERNET)) {
			mst = 0;
			*data = Chip_ENET_ReadMIIData(LPC_ETHERNET);
			sts = SUCCESS;
		}
		else {
			mst--;
			pDelayMs(1);
		}
	}

	return sts;
}



uint32_t lpc_phy_init(uint8_t rmii, p_msDelay_func_t pDelayMsFunc)
{
	uint16_t tmp;
	int32_t i;

	pDelayMs = pDelayMsFunc;

	/* Initial states for PHY status and state machine */
	olddphysts = physts = phyustate = 0;

	/* Only first read and write are checked for failure */
	/* Put the DP83848C in reset mode and wait for completion */
	if (lpc_mii_write(LAN8_BCR_REG, LAN8_RESET) != SUCCESS) {
		return ERROR;
	}
	i = 400;
	while (i > 0) {
		pDelayMs(1);
		if (lpc_mii_read(LAN8_BCR_REG, &tmp) != SUCCESS) {
			return ERROR;
		}

		if (!(tmp & (LAN8_RESET | LAN8_POWER_DOWN))) {
			i = -1;
		}
		else {
			i--;
		}
	}
	/* Timeout? */
	if (i == 0) {
		return ERROR;
	}

	/* Setup link */
	lpc_mii_write(LAN8_BCR_REG, LAN8_AUTONEG);

	/* The link is not set active at this point, but will be detected
	   later */

	return SUCCESS;
}

static inline void Chip_ENET_SetADDR(LPC_ENET_T *pENET, const uint8_t *macAddr)
{
	/* Save MAC address */
	pENET->MAC.SA[0] = ((uint32_t) macAddr[5] << 8) | ((uint32_t) macAddr[4]);
	pENET->MAC.SA[1] = ((uint32_t) macAddr[3] << 8) | ((uint32_t) macAddr[2]);
	pENET->MAC.SA[2] = ((uint32_t) macAddr[1] << 8) | ((uint32_t) macAddr[0]);
}

/* Configures the initial ethernet transmit descriptors */
void Chip_ENET_InitTxDescriptors(LPC_ENET_T *pENET,
								 ENET_TXDESC_T *pDescs,
								 ENET_TXSTAT_T *pStatus,
								 uint32_t descNum)
{
	/* Setup descriptor list base addresses */
	pENET->CONTROL.TX.DESCRIPTOR = (uint32_t) pDescs;
	pENET->CONTROL.TX.DESCRIPTORNUMBER = descNum - 1;
	pENET->CONTROL.TX.STATUS = (uint32_t) pStatus;
	pENET->CONTROL.TX.PRODUCEINDEX = 0;
}

static err_t lpc_tx_setup(lpc_enetdata_t *lpc_enetif)
{
	s32_t idx;

	/* Build TX descriptors for local buffers */
	for (idx = 0; idx < LPC_NUM_BUFF_TXDESCS; idx++) {
		lpc_enetif->ptxd[idx].Control = 0;
		lpc_enetif->ptxs[idx].StatusInfo = 0xFFFFFFFF;
	}

	/* Setup pointers to TX structures */
	Chip_ENET_InitTxDescriptors(LPC_ETHERNET, lpc_enetif->ptxd, lpc_enetif->ptxs, LPC_NUM_BUFF_TXDESCS);

	lpc_enetif->lpc_last_tx_idx = 0;

	return ERR_OK;
}


/* Configures the initial ethernet receive descriptors */
void Chip_ENET_InitRxDescriptors(LPC_ENET_T *pENET,
								 ENET_RXDESC_T *pDescs,
								 ENET_RXSTAT_T *pStatus,
								 uint32_t descNum)
{
	/* Setup descriptor list base addresses */
	pENET->CONTROL.RX.DESCRIPTOR = (uint32_t) pDescs;
	pENET->CONTROL.RX.DESCRIPTORNUMBER = descNum - 1;
	pENET->CONTROL.RX.STATUS = (uint32_t) pStatus;
	pENET->CONTROL.RX.CONSUMEINDEX = 0;
}

static void lpc_rxqueue_pbuf(lpc_enetdata_t *lpc_enetif, struct pbuf *p)
{
	u32_t idx;

	/* Get next free descriptor index */
	idx = lpc_enetif->rx_fill_desc_index;

	/* Setup descriptor and clear statuses */
	lpc_enetif->prxd[idx].Control = ENET_RCTRL_INT | ((u32_t) ENET_RCTRL_SIZE(p->len));
	lpc_enetif->prxd[idx].Packet = (u32_t) p->payload;
	lpc_enetif->prxs[idx].StatusInfo = 0xFFFFFFFF;
	lpc_enetif->prxs[idx].StatusHashCRC = 0xFFFFFFFF;

	/* Save pbuf pointer for push to network layer later */
	lpc_enetif->rxb[idx] = p;

	/* Wrap at end of descriptor list */
	idx++;
	if (idx >= LPC_NUM_BUFF_RXDESCS) {
		idx = 0;
	}

	/* Queue descriptor(s) */
	lpc_enetif->rx_free_descs -= 1;
	lpc_enetif->rx_fill_desc_index = idx;

	LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
				("lpc_rxqueue_pbuf: pbuf packet queued: %p (free desc=%d)\n", p,
				 lpc_enetif->rx_free_descs));
}

uint8_t lpc_rx_queue(struct netif *netif)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *p;

	s32_t queued = 0;

	/* Attempt to requeue as many packets as possible */
	while (lpc_enetif->rx_free_descs > 0) {
		/* Allocate a pbuf from the pool. We need to allocate at the
		   maximum size as we don't know the size of the yet to be
		   received packet. */
		p = pbuf_alloc(PBUF_RAW, (u16_t) ENET_ETH_MAX_FLEN, PBUF_RAM);
		if (p == NULL) {
			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_rx_queue: could not allocate RX pbuf (free desc=%d)\n",
						 lpc_enetif->rx_free_descs));
			return queued;
		}

		/* pbufs allocated from the RAM pool should be non-chained. */
		LWIP_ASSERT("lpc_rx_queue: pbuf is not contiguous (chained)",
					pbuf_clen(p) <= 1);

		/* Queue packet */
		lpc_rxqueue_pbuf(lpc_enetif, p);

		/* Update queued count */
		queued++;
	}

	return queued;
}

/* Sets up the RX descriptor ring buffers. */
static err_t lpc_rx_setup(lpc_enetdata_t *lpc_enetif)
{
	/* Setup pointers to RX structures */
	Chip_ENET_InitRxDescriptors(LPC_ETHERNET, lpc_enetif->prxd, lpc_enetif->prxs, LPC_NUM_BUFF_RXDESCS);

	lpc_enetif->rx_free_descs = LPC_NUM_BUFF_RXDESCS;
	lpc_enetif->rx_fill_desc_index = 0;

	/* Build RX buffer and descriptors */
	lpc_rx_queue(lpc_enetif->pnetif);

	return ERR_OK;
}

static inline void Chip_ENET_EnableRXFilter(LPC_ENET_T *pENET, uint32_t mask)
{
	pENET->CONTROL.COMMAND &= ~ENET_COMMAND_PASSRXFILTER;
	pENET->RXFILTER.CONTROL |=  mask;
}

static inline void Chip_ENET_EnableInt(LPC_ENET_T *pENET, uint32_t mask)
{
	pENET->MODULE_CONTROL.INTENABLE |= mask;
}


static inline void Chip_ENET_TXEnable(LPC_ENET_T *pENET)
{
	/* Descriptor list head pointers must be setup prior to enable */
	pENET->CONTROL.COMMAND |= ENET_COMMAND_TXENABLE;
}

static inline void Chip_ENET_RXEnable(LPC_ENET_T *pENET)
{
	/* Descriptor list head pointers must be setup prior to enable */
	pENET->CONTROL.COMMAND |= ENET_COMMAND_RXENABLE;
	pENET->MAC.MAC1 |= ENET_MAC1_RXENABLE;
}

static err_t low_level_init(struct netif *netif)
{
	lpc_enetdata_t *lpc_enetif = netif->state;
	err_t err = ERR_OK;

#if defined(USE_RMII)
	Chip_ENET_Init(LPC_ETHERNET, true);

#else
	Chip_ENET_Init(LPC_ETHERNET, false);
#endif

	/* Initialize the PHY */
	Chip_ENET_SetupMII(LPC_ETHERNET, Chip_ENET_FindMIIDiv(LPC_ETHERNET, 2500000), LPC_PHYDEF_PHYADDR);
#if defined(USE_RMII)
	if (lpc_phy_init(true, msDelay) != SUCCESS) {
		return ERROR;
	}
#else
	if (lpc_phy_init(false, msDelay) != SUCCESS) {
		return ERROR;
	}
#endif


	/* Save station address */
	Chip_ENET_SetADDR(LPC_ETHERNET, netif->hwaddr);

	/* Setup transmit and receive descriptors */
	if (lpc_tx_setup(lpc_enetif) != ERR_OK) {
		return ERR_BUF;
	}
	if (lpc_rx_setup(lpc_enetif) != ERR_OK) {
		return ERR_BUF;
	}

	/* Enable packet reception */
#if IP_SOF_BROADCAST_RECV
	Chip_ENET_EnableRXFilter(LPC_ETHERNET, ENET_RXFILTERCTRL_APE | ENET_RXFILTERCTRL_ABE);
#else
	Chip_ENET_EnableRXFilter(ENET_RXFILTERCTRL_APE);
#endif

	/* Clear and enable rx/tx interrupts */
	Chip_ENET_EnableInt(LPC_ETHERNET, RXINTGROUP | TXINTGROUP);

	/* Enable RX and TX */
	Chip_ENET_TXEnable(LPC_ETHERNET);
	Chip_ENET_RXEnable(LPC_ETHERNET);

	return err;
}

static inline uint32_t Chip_ENET_GetIntStatus(LPC_ENET_T *pENET)
{
	return pENET->MODULE_CONTROL.INTSTATUS;
}

static inline void Chip_ENET_RXDisable(LPC_ENET_T *pENET)
{
	pENET->CONTROL.COMMAND &= ~ENET_COMMAND_RXENABLE;
	pENET->MAC.MAC1 &= ~ENET_MAC1_RXENABLE;
}

static inline void Chip_ENET_ResetTXLogic(LPC_ENET_T *pENET)
{
	pENET->MAC.MAC1 |= ENET_MAC1_RESETTX;
}

static inline void Chip_ENET_ResetRXLogic(LPC_ENET_T *pENET)
{
	pENET->MAC.MAC1 |= ENET_MAC1_RESETRX;
}

static inline void Chip_ENET_ClearIntStatus(LPC_ENET_T *pENET, uint32_t mask)
{
	pENET->MODULE_CONTROL.INTCLEAR = mask;
}

static inline uint16_t Chip_ENET_GetRXConsumeIndex(LPC_ENET_T *pENET)
{
	return pENET->CONTROL.RX.CONSUMEINDEX;
}

static inline uint8_t Chip_ENET_IsRxEmpty(LPC_ENET_T *pENET)
{
	uint32_t tem = pENET->CONTROL.RX.PRODUCEINDEX;
	return (pENET->CONTROL.RX.CONSUMEINDEX != tem) ? false : true;
}

uint16_t Chip_ENET_IncRXConsumeIndex(LPC_ENET_T *pENET)
{
	/* Get current RX consume index */
	uint32_t idx = pENET->CONTROL.RX.CONSUMEINDEX;

	/* Consume descriptor */
	idx++;
	if (idx > pENET->CONTROL.RX.DESCRIPTORNUMBER) {
		idx = 0;
	}
	pENET->CONTROL.RX.CONSUMEINDEX = idx;

	return idx;
}

static struct pbuf *lpc_low_level_input(struct netif *netif) {
	lpc_enetdata_t *lpc_enetif = netif->state;
	struct pbuf *p = NULL;
	u32_t idx, length;

#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
	/* Get exclusive access */
	sys_mutex_lock(&lpc_enetif->rx_lock_mutex);
#endif
#endif

	/* Monitor RX overrun status. This should never happen unless
	   (possibly) the internal bus is behing held up by something.
	   Unless your system is running at a very low clock speed or
	   there are possibilities that the internal buses may be held
	   up for a long time, this can probably safely be removed. */
	if (Chip_ENET_GetIntStatus(LPC_ETHERNET) & ENET_INT_RXOVERRUN) {
		LINK_STATS_INC(link.err);
		LINK_STATS_INC(link.drop);

		/* Temporarily disable RX */
		Chip_ENET_RXDisable(LPC_ETHERNET);

		/* Reset the RX side */
		Chip_ENET_ResetRXLogic(LPC_ETHERNET);
		Chip_ENET_ClearIntStatus(LPC_ETHERNET, ENET_INT_RXOVERRUN);

		/* De-allocate all queued RX pbufs */
		for (idx = 0; idx < LPC_NUM_BUFF_RXDESCS; idx++) {
			if (lpc_enetif->rxb[idx] != NULL) {
				pbuf_free(lpc_enetif->rxb[idx]);
				lpc_enetif->rxb[idx] = NULL;
			}
		}

		/* Start RX side again */
		lpc_rx_setup(lpc_enetif);

		/* Re-enable RX */
		Chip_ENET_RXEnable(LPC_ETHERNET);

#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
		sys_mutex_unlock(&lpc_enetif->rx_lock_mutex);
#endif
#endif

		return NULL;
	}

	/* Determine if a frame has been received */
	length = 0;
	idx = Chip_ENET_GetRXConsumeIndex(LPC_ETHERNET);
	if (!Chip_ENET_IsRxEmpty(LPC_ETHERNET)) {
		/* Handle errors */
		if (lpc_enetif->prxs[idx].StatusInfo & (ENET_RINFO_CRC_ERR |
												ENET_RINFO_SYM_ERR | ENET_RINFO_ALIGN_ERR | ENET_RINFO_LEN_ERR)) {
#if LINK_STATS
			if (lpc_enetif->prxs[idx].StatusInfo & (ENET_RINFO_CRC_ERR |
													ENET_RINFO_SYM_ERR | ENET_RINFO_ALIGN_ERR)) {
				LINK_STATS_INC(link.chkerr);
			}
			if (lpc_enetif->prxs[idx].StatusInfo & ENET_RINFO_LEN_ERR) {
				LINK_STATS_INC(link.lenerr);
			}
#endif

			/* Drop the frame */
			LINK_STATS_INC(link.drop);

			/* Re-queue the pbuf for receive */
			lpc_enetif->rx_free_descs++;
			p = lpc_enetif->rxb[idx];
			lpc_enetif->rxb[idx] = NULL;
			lpc_rxqueue_pbuf(lpc_enetif, p);

			LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
						("lpc_low_level_input: Packet dropped with errors (0x%x)\n",
						 lpc_enetif->prxs[idx].StatusInfo));

			p = NULL;
		}
		else {
			/* A packet is waiting, get length */
			length = ENET_RINFO_SIZE(lpc_enetif->prxs[idx].StatusInfo) - 4;	/* Remove FCS */

			/* Zero-copy */
			p = lpc_enetif->rxb[idx];
			p->len = (u16_t) length;

			/* Free pbuf from desriptor */
			lpc_enetif->rxb[idx] = NULL;
			lpc_enetif->rx_free_descs++;

			/* Queue new buffer(s) */
			if (lpc_rx_queue(lpc_enetif->pnetif) == 0) {

				/* Re-queue the pbuf for receive */
				lpc_rxqueue_pbuf(lpc_enetif, p);

				/* Drop the frame */
				LINK_STATS_INC(link.drop);

				LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
							("lpc_low_level_input: Packet dropped since it could not allocate Rx Buffer\n"));

				p = NULL;
			}
			else {

				LWIP_DEBUGF(EMAC_DEBUG | LWIP_DBG_TRACE,
							("lpc_low_level_input: Packet received: %p, size %d (index=%d)\n",
							 p, length, idx));

				/* Save size */
				p->tot_len = (u16_t) length;
				LINK_STATS_INC(link.recv);
			}
		}

		/* Update Consume index */
		Chip_ENET_IncRXConsumeIndex(LPC_ETHERNET);
	}

#ifdef LOCK_RX_THREAD
#if NO_SYS == 0
	sys_mutex_unlock(&lpc_enetif->rx_lock_mutex);
#endif
#endif

	return p;
}

/* Attempt to read a packet from the EMAC interface */
void lpc_enetif_input(struct netif *netif)
{
	struct eth_hdr *ethhdr;

	struct pbuf *p;

	/* move received packet into a new pbuf */
	p = lpc_low_level_input(netif);
	if (p == NULL) {
		return;
	}

	/* points to packet payload, which starts with an Ethernet header */
	ethhdr = p->payload;

	switch (htons(ethhdr->type)) {
	case ETHTYPE_IP:
	case ETHTYPE_ARP:
#if PPPOE_SUPPORT
	case ETHTYPE_PPPOEDISC:
	case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
		/* full packet send to tcpip_thread to process */
		if (netif->input(p, netif) != ERR_OK) {
			LWIP_DEBUGF(NETIF_DEBUG, ("lpc_enetif_input: IP input error\n"));
			/* Free buffer */
			pbuf_free(p);
		}
		break;

	default:
		/* Return buffer */
		pbuf_free(p);
		break;
	}
}


err_t lpc_enetif_init(struct netif *netif)
{
	err_t err;

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	lpc_enetdata.pnetif = netif;

	/* set MAC hardware address */
	Board_ENET_GetMacADDR(netif->hwaddr);
	netif->hwaddr_len = ETHARP_HWADDR_LEN;

	/* maximum transfer unit */
	netif->mtu = 1500;

	/* device capabilities */
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_UP |
				   NETIF_FLAG_ETHERNET;

	/* Initialize the hardware */
	netif->state = &lpc_enetdata;
	err = low_level_init(netif);
	if (err != ERR_OK) {
		return err;
	}

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "lwiplpc";
#endif /* LWIP_NETIF_HOSTNAME */

	netif->name[0] = 'e';
	netif->name[1] = 'n';

	netif->output = lpc_etharp_output;
	netif->linkoutput = lpc_low_level_output;

	/* For FreeRTOS, start tasks */
#if NO_SYS == 0
	lpc_enetdata.xtx_count_sem = xSemaphoreCreateCounting(LPC_NUM_BUFF_TXDESCS,
														  LPC_NUM_BUFF_TXDESCS);
	LWIP_ASSERT("xtx_count_sem creation error",
				(lpc_enetdata.xtx_count_sem != NULL));

	err = sys_mutex_new(&lpc_enetdata.tx_lock_mutex);
	LWIP_ASSERT("tx_lock_mutex creation error", (err == ERR_OK));

	err = sys_mutex_new(&lpc_enetdata.rx_lock_mutex);
	LWIP_ASSERT("rx_lock_mutex creation error", (err == ERR_OK));

	/* Packet receive task */
	err = sys_sem_new(&lpc_enetdata.rx_sem, 0);
	LWIP_ASSERT("rx_sem creation error", (err == ERR_OK));
	sys_thread_new("receive_thread", vPacketReceiveTask, netif->state,
				   DEFAULT_THREAD_STACKSIZE, tskRECPKT_PRIORITY);

	/* Transmit cleanup task */
	err = sys_sem_new(&lpc_enetdata.tx_clean_sem, 0);
	LWIP_ASSERT("tx_clean_sem creation error", (err == ERR_OK));
	sys_thread_new("txclean_thread", vTransmitCleanupTask, netif->state,
				   DEFAULT_THREAD_STACKSIZE, tskTXCLEAN_PRIORITY);
#endif

	return ERR_OK;
}

struct stats_ lwip_stats;


static err_t
etharp_update_arp_entry(struct netif *netif, ip_addr_t *ipaddr, struct eth_addr *ethaddr, u8_t flags)
{
  s8_t i;
  LWIP_ASSERT("netif->hwaddr_len == ETHARP_HWADDR_LEN", netif->hwaddr_len == ETHARP_HWADDR_LEN);
  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_update_arp_entry: %"U16_F".%"U16_F".%"U16_F".%"U16_F" - %02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F"\n",
    ip4_addr1_16(ipaddr), ip4_addr2_16(ipaddr), ip4_addr3_16(ipaddr), ip4_addr4_16(ipaddr),
    ethaddr->addr[0], ethaddr->addr[1], ethaddr->addr[2],
    ethaddr->addr[3], ethaddr->addr[4], ethaddr->addr[5]));
  /* non-unicast address? */
  if (ip_addr_isany(ipaddr) ||
      ip_addr_isbroadcast(ipaddr, netif) ||
      ip_addr_ismulticast(ipaddr)) {
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_update_arp_entry: will not add non-unicast IP address to ARP cache\n"));
    return ERR_ARG;
  }
  /* find or create ARP entry */
  i = etharp_find_entry(ipaddr, flags);
  /* bail out if no entry could be found */
  if (i < 0) {
    return (err_t)i;
  }

#if ETHARP_SUPPORT_STATIC_ENTRIES
  if (flags & ETHARP_FLAG_STATIC_ENTRY) {
    /* record static type */
    arp_table[i].state = ETHARP_STATE_STATIC;
  } else
#endif /* ETHARP_SUPPORT_STATIC_ENTRIES */
  {
    /* mark it stable */
    arp_table[i].state = ETHARP_STATE_STABLE;
  }

  /* record network interface */
  arp_table[i].netif = netif;
  /* insert in SNMP ARP index tree */
  snmp_insert_arpidx_tree(netif, &arp_table[i].ipaddr);

  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_update_arp_entry: updating stable entry %"S16_F"\n", (s16_t)i));
  /* update address */
  ETHADDR32_COPY(&arp_table[i].ethaddr, ethaddr);
  /* reset time stamp */
  arp_table[i].ctime = 0;
  /* this is where we will send out queued packets! */
#if ARP_QUEUEING
  while (arp_table[i].q != NULL) {
    struct pbuf *p;
    /* remember remainder of queue */
    struct etharp_q_entry *q = arp_table[i].q;
    /* pop first item off the queue */
    arp_table[i].q = q->next;
    /* get the packet pointer */
    p = q->p;
    /* now queue entry can be freed */
    memp_free(MEMP_ARP_QUEUE, q);
#else /* ARP_QUEUEING */
  if (arp_table[i].q != NULL) {
    struct pbuf *p = arp_table[i].q;
    arp_table[i].q = NULL;
#endif /* ARP_QUEUEING */
    /* send the queued IP packet */
    etharp_send_ip(netif, p, (struct eth_addr*)(netif->hwaddr), ethaddr);
    /* free the queued IP packet */
    pbuf_free(p);
  }
  return ERR_OK;
}


static void
etharp_arp_input(struct netif *netif, struct eth_addr *ethaddr, struct pbuf *p)
{
  struct etharp_hdr *hdr;
  struct eth_hdr *ethhdr;
  /* these are aligned properly, whereas the ARP header fields might not be */
  ip_addr_t sipaddr, dipaddr;
  u8_t for_us;
#if LWIP_AUTOIP
  const u8_t * ethdst_hwaddr;
#endif /* LWIP_AUTOIP */

  LWIP_ERROR("netif != NULL", (netif != NULL), return;);

  /* drop short ARP packets: we have to check for p->len instead of p->tot_len here
     since a struct etharp_hdr is pointed to p->payload, so it musn't be chained! */
  if (p->len < SIZEOF_ETHARP_PACKET) {
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
      ("etharp_arp_input: packet dropped, too short (%"S16_F"/%"S16_F")\n", p->tot_len,
      (s16_t)SIZEOF_ETHARP_PACKET));
    ETHARP_STATS_INC(etharp.lenerr);
    ETHARP_STATS_INC(etharp.drop);
    pbuf_free(p);
    return;
  }

  ethhdr = (struct eth_hdr *)p->payload;
  hdr = (struct etharp_hdr *)((u8_t*)ethhdr + SIZEOF_ETH_HDR);
#if ETHARP_SUPPORT_VLAN
  if (ethhdr->type == PP_HTONS(ETHTYPE_VLAN)) {
    hdr = (struct etharp_hdr *)(((u8_t*)ethhdr) + SIZEOF_ETH_HDR + SIZEOF_VLAN_HDR);
  }
#endif /* ETHARP_SUPPORT_VLAN */

  /* RFC 826 "Packet Reception": */
  if ((hdr->hwtype != PP_HTONS(HWTYPE_ETHERNET)) ||
      (hdr->hwlen != ETHARP_HWADDR_LEN) ||
      (hdr->protolen != sizeof(ip_addr_t)) ||
      (hdr->proto != PP_HTONS(ETHTYPE_IP)))  {
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
      ("etharp_arp_input: packet dropped, wrong hw type, hwlen, proto, protolen or ethernet type (%"U16_F"/%"U16_F"/%"U16_F"/%"U16_F")\n",
      hdr->hwtype, hdr->hwlen, hdr->proto, hdr->protolen));
    ETHARP_STATS_INC(etharp.proterr);
    ETHARP_STATS_INC(etharp.drop);
    pbuf_free(p);
    return;
  }
  ETHARP_STATS_INC(etharp.recv);

#if LWIP_AUTOIP
  /* We have to check if a host already has configured our random
   * created link local address and continously check if there is
   * a host with this IP-address so we can detect collisions */
  autoip_arp_reply(netif, hdr);
#endif /* LWIP_AUTOIP */

  /* Copy struct ip_addr2 to aligned ip_addr, to support compilers without
   * structure packing (not using structure copy which breaks strict-aliasing rules). */
  IPADDR2_COPY(&sipaddr, &hdr->sipaddr);
  IPADDR2_COPY(&dipaddr, &hdr->dipaddr);

  /* this interface is not configured? */
  if (ip_addr_isany(&netif->ip_addr)) {
    for_us = 0;
  } else {
    /* ARP packet directed to us? */
    for_us = (u8_t)ip_addr_cmp(&dipaddr, &(netif->ip_addr));
  }

  /* ARP message directed to us?
      -> add IP address in ARP cache; assume requester wants to talk to us,
         can result in directly sending the queued packets for this host.
     ARP message not directed to us?
      ->  update the source IP address in the cache, if present */
  etharp_update_arp_entry(netif, &sipaddr, &(hdr->shwaddr),
                   for_us ? ETHARP_FLAG_TRY_HARD : ETHARP_FLAG_FIND_ONLY);

  /* now act on the message itself */
  switch (hdr->opcode) {
  /* ARP request? */
  case PP_HTONS(ARP_REQUEST):
    /* ARP request. If it asked for our address, we send out a
     * reply. In any case, we time-stamp any existing ARP entry,
     * and possiby send out an IP packet that was queued on it. */

    LWIP_DEBUGF (ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_arp_input: incoming ARP request\n"));
    /* ARP request for our address? */
    if (for_us) {

      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_arp_input: replying to ARP request for our IP address\n"));
      /* Re-use pbuf to send ARP reply.
         Since we are re-using an existing pbuf, we can't call etharp_raw since
         that would allocate a new pbuf. */
      hdr->opcode = htons(ARP_REPLY);

      IPADDR2_COPY(&hdr->dipaddr, &hdr->sipaddr);
      IPADDR2_COPY(&hdr->sipaddr, &netif->ip_addr);

      LWIP_ASSERT("netif->hwaddr_len must be the same as ETHARP_HWADDR_LEN for etharp!",
                  (netif->hwaddr_len == ETHARP_HWADDR_LEN));
#if LWIP_AUTOIP
      /* If we are using Link-Local, all ARP packets that contain a Link-Local
       * 'sender IP address' MUST be sent using link-layer broadcast instead of
       * link-layer unicast. (See RFC3927 Section 2.5, last paragraph) */
      ethdst_hwaddr = ip_addr_islinklocal(&netif->ip_addr) ? (u8_t*)(ethbroadcast.addr) : hdr->shwaddr.addr;
#endif /* LWIP_AUTOIP */

      ETHADDR16_COPY(&hdr->dhwaddr, &hdr->shwaddr);
#if LWIP_AUTOIP
      ETHADDR16_COPY(&ethhdr->dest, ethdst_hwaddr);
#else  /* LWIP_AUTOIP */
      ETHADDR16_COPY(&ethhdr->dest, &hdr->shwaddr);
#endif /* LWIP_AUTOIP */
      ETHADDR16_COPY(&hdr->shwaddr, ethaddr);
      ETHADDR16_COPY(&ethhdr->src, ethaddr);

      /* hwtype, hwaddr_len, proto, protolen and the type in the ethernet header
         are already correct, we tested that before */

      /* return ARP reply */
      netif->linkoutput(netif, p);
    /* we are not configured? */
    } else if (ip_addr_isany(&netif->ip_addr)) {
      /* { for_us == 0 and netif->ip_addr.addr == 0 } */
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_arp_input: we are unconfigured, ARP request ignored.\n"));
    /* request was not directed to us */
    } else {
      /* { for_us == 0 and netif->ip_addr.addr != 0 } */
      LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_arp_input: ARP request was not for us.\n"));
    }
    break;
  case PP_HTONS(ARP_REPLY):
    /* ARP reply. We already updated the ARP cache earlier. */
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_arp_input: incoming ARP reply\n"));
#if (LWIP_DHCP && DHCP_DOES_ARP_CHECK)
    /* DHCP wants to know about ARP replies from any host with an
     * IP address also offered to us by the DHCP server. We do not
     * want to take a duplicate IP address on a single network.
     * @todo How should we handle redundant (fail-over) interfaces? */
    dhcp_arp_reply(netif, &sipaddr);
#endif /* (LWIP_DHCP && DHCP_DOES_ARP_CHECK) */
    break;
  default:
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("etharp_arp_input: ARP unknown opcode type %"S16_F"\n", htons(hdr->opcode)));
    ETHARP_STATS_INC(etharp.err);
    break;
  }
  /* free ARP packet */
  pbuf_free(p);
}


err_t
ethernet_input(struct pbuf *p, struct netif *netif)
{
  struct eth_hdr* ethhdr;
  u16_t type;
#if LWIP_ARP || ETHARP_SUPPORT_VLAN
  s16_t ip_hdr_offset = SIZEOF_ETH_HDR;
#endif /* LWIP_ARP || ETHARP_SUPPORT_VLAN */

  if (p->len <= SIZEOF_ETH_HDR) {
    /* a packet with only an ethernet header (or less) is not valid for us */
    ETHARP_STATS_INC(etharp.proterr);
    ETHARP_STATS_INC(etharp.drop);
    goto free_and_return;
  }

  /* points to packet payload, which starts with an Ethernet header */
  ethhdr = (struct eth_hdr *)p->payload;
  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE,
    ("ethernet_input: dest:%"X8_F":%"X8_F":%"X8_F":%"X8_F":%"X8_F":%"X8_F", src:%"X8_F":%"X8_F":%"X8_F":%"X8_F":%"X8_F":%"X8_F", type:%"X16_F"\n",
     (unsigned)ethhdr->dest.addr[0], (unsigned)ethhdr->dest.addr[1], (unsigned)ethhdr->dest.addr[2],
     (unsigned)ethhdr->dest.addr[3], (unsigned)ethhdr->dest.addr[4], (unsigned)ethhdr->dest.addr[5],
     (unsigned)ethhdr->src.addr[0], (unsigned)ethhdr->src.addr[1], (unsigned)ethhdr->src.addr[2],
     (unsigned)ethhdr->src.addr[3], (unsigned)ethhdr->src.addr[4], (unsigned)ethhdr->src.addr[5],
     (unsigned)htons(ethhdr->type)));

  type = ethhdr->type;
#if ETHARP_SUPPORT_VLAN
  if (type == PP_HTONS(ETHTYPE_VLAN)) {
    struct eth_vlan_hdr *vlan = (struct eth_vlan_hdr*)(((char*)ethhdr) + SIZEOF_ETH_HDR);
    if (p->len <= SIZEOF_ETH_HDR + SIZEOF_VLAN_HDR) {
      /* a packet with only an ethernet/vlan header (or less) is not valid for us */
      ETHARP_STATS_INC(etharp.proterr);
      ETHARP_STATS_INC(etharp.drop);
      goto free_and_return;
    }
#if defined(ETHARP_VLAN_CHECK) || defined(ETHARP_VLAN_CHECK_FN) /* if not, allow all VLANs */
#ifdef ETHARP_VLAN_CHECK_FN
    if (!ETHARP_VLAN_CHECK_FN(ethhdr, vlan)) {
#elif defined(ETHARP_VLAN_CHECK)
    if (VLAN_ID(vlan) != ETHARP_VLAN_CHECK) {
#endif
      /* silently ignore this packet: not for our VLAN */
      pbuf_free(p);
      return ERR_OK;
    }
#endif /* defined(ETHARP_VLAN_CHECK) || defined(ETHARP_VLAN_CHECK_FN) */
    type = vlan->tpid;
    ip_hdr_offset = SIZEOF_ETH_HDR + SIZEOF_VLAN_HDR;
  }
#endif /* ETHARP_SUPPORT_VLAN */

#if LWIP_ARP_FILTER_NETIF
  netif = LWIP_ARP_FILTER_NETIF_FN(p, netif, htons(type));
#endif /* LWIP_ARP_FILTER_NETIF*/

  if (ethhdr->dest.addr[0] & 1) {
    /* this might be a multicast or broadcast packet */
    if (ethhdr->dest.addr[0] == LL_MULTICAST_ADDR_0) {
      if ((ethhdr->dest.addr[1] == LL_MULTICAST_ADDR_1) &&
          (ethhdr->dest.addr[2] == LL_MULTICAST_ADDR_2)) {
        /* mark the pbuf as link-layer multicast */
        p->flags |= PBUF_FLAG_LLMCAST;
      }
    } else if (eth_addr_cmp(&ethhdr->dest, &ethbroadcast)) {
      /* mark the pbuf as link-layer broadcast */
      p->flags |= PBUF_FLAG_LLBCAST;
    }
  }

  switch (type) {
#if LWIP_ARP
    /* IP packet? */
    case PP_HTONS(ETHTYPE_IP):
      if (!(netif->flags & NETIF_FLAG_ETHARP)) {
        goto free_and_return;
      }
#if ETHARP_TRUST_IP_MAC
      /* update ARP table */
      etharp_ip_input(netif, p);
#endif /* ETHARP_TRUST_IP_MAC */
      /* skip Ethernet header */
      if(pbuf_header(p, -ip_hdr_offset)) {
        LWIP_ASSERT("Can't move over header in packet", 0);
        goto free_and_return;
      } else {
        /* pass to IP layer */
        ip_input(p, netif);
      }
      break;

    case PP_HTONS(ETHTYPE_ARP):
      if (!(netif->flags & NETIF_FLAG_ETHARP)) {
        goto free_and_return;
      }
      /* pass p to ARP module */
      etharp_arp_input(netif, (struct eth_addr*)(netif->hwaddr), p);
      break;
#endif /* LWIP_ARP */
#if PPPOE_SUPPORT
    case PP_HTONS(ETHTYPE_PPPOEDISC): /* PPP Over Ethernet Discovery Stage */
      pppoe_disc_input(netif, p);
      break;

    case PP_HTONS(ETHTYPE_PPPOE): /* PPP Over Ethernet Session Stage */
      pppoe_data_input(netif, p);
      break;
#endif /* PPPOE_SUPPORT */

    default:
      ETHARP_STATS_INC(etharp.proterr);
      ETHARP_STATS_INC(etharp.drop);
      goto free_and_return;
  }

  /* This means the pbuf is freed or consumed,
     so the caller doesn't have to free it again */
  return ERR_OK;

free_and_return:
  pbuf_free(p);
  return ERR_OK;
}

  static void smsc_update_phy_sts(uint16_t linksts, uint16_t sdsts)
  {
  	/* Update link active status */
  	if (linksts & LAN8_LINK_STATUS) {
  		physts |= PHY_LINK_CONNECTED;
  	}
  	else {
  		physts &= ~PHY_LINK_CONNECTED;
  	}

  	switch (sdsts & LAN8_SPEEDMASK) {
  	case LAN8_SPEED100F:
  	default:
  		physts |= PHY_LINK_SPEED100;
  		physts |= PHY_LINK_FULLDUPLX;
  		break;

  	case LAN8_SPEED10F:
  		physts &= ~PHY_LINK_SPEED100;
  		physts |= PHY_LINK_FULLDUPLX;
  		break;

  	case LAN8_SPEED100H:
  		physts |= PHY_LINK_SPEED100;
  		physts &= ~PHY_LINK_FULLDUPLX;
  		break;

  	case LAN8_SPEED10H:
  		physts &= ~PHY_LINK_SPEED100;
  		physts &= ~PHY_LINK_FULLDUPLX;
  		break;
  	}

  	/* If the status has changed, indicate via change flag */
  	if ((physts & (PHY_LINK_SPEED100 | PHY_LINK_FULLDUPLX | PHY_LINK_CONNECTED)) !=
  		(olddphysts & (PHY_LINK_SPEED100 | PHY_LINK_FULLDUPLX | PHY_LINK_CONNECTED))) {
  		olddphysts = physts;
  		physts |= PHY_LINK_CHANGED;
  	}
  }


uint32_t lpcPHYStsPoll(void)
{
static uint16_t sts;

switch (phyustate) {
default:
case 0:
	/* Read BMSR to clear faults */
	Chip_ENET_StartMIIRead(LPC_ETHERNET, LAN8_BSR_REG);
	physts &= ~PHY_LINK_CHANGED;
	physts = physts | PHY_LINK_BUSY;
	phyustate = 1;
	break;

case 1:
	/* Wait for read status state */
	if (!Chip_ENET_IsMIIBusy(LPC_ETHERNET)) {
		/* Get PHY status with link state */
		sts = Chip_ENET_ReadMIIData(LPC_ETHERNET);
		Chip_ENET_StartMIIRead(LPC_ETHERNET, LAN8_PHYSPLCTL_REG);
		phyustate = 2;
	}
	break;

case 2:
	/* Wait for read status state */
	if (!Chip_ENET_IsMIIBusy(LPC_ETHERNET)) {
		/* Update PHY status */
		physts &= ~PHY_LINK_BUSY;
		smsc_update_phy_sts(sts, Chip_ENET_ReadMIIData(LPC_ETHERNET));
		phyustate = 0;
	}
	break;
}

return physts;
}

