#include "receiver.h"

#include "../config.h"
#include "../parse.h"

#include <libconfig.h>
#include <time.h>

#include <rte_log.h>
#include <rte_ether.h>


#define RTE_LOGTYPE_RECEIVER RTE_LOGTYPE_USER1

void
log_receiver(struct receiver_t *receiver) {
    RTE_LOG(INFO, RECEIVER, "------------- Receiver -------------\n");
    RTE_LOG(INFO, RECEIVER, "| Core ID:          %"PRIu32"\n", receiver->core_id);
    RTE_LOG(INFO, RECEIVER, "| In port:          %"PRIu32"\n", receiver->in_port);
    RTE_LOG(INFO, RECEIVER, "| MAC:              "FORMAT_MAC"\n", ARG_V_MAC(receiver->mac));
    RTE_LOG(INFO, RECEIVER, "| Packets received: %"PRIu64"\n", receiver->pkts_received);
    RTE_LOG(INFO, RECEIVER, "| Load:             %"PRIu64"\n", receiver->nb_rec);
    RTE_LOG(INFO, RECEIVER, "------------------------------------\n");
    receiver->nb_polls = 0;
    receiver->nb_rec = 0;
}

void
poll_receiver(struct receiver_t *receiver) {
    const uint16_t port = receiver->in_port;
    struct rte_mbuf **pkts_burst = receiver->burst_buffer;

    unsigned nb_rx = rte_eth_rx_burst((uint8_t) port, 0,
                    pkts_burst, BURST_SIZE);

    receiver->pkts_received += nb_rx;
    receiver->nb_polls++;
    if (nb_rx == BURST_SIZE)
        receiver->nb_rec += 1;

    for (unsigned h_index = 0; h_index < receiver->nb_handler; ++h_index) {
        /* handover packet to handler. */
        receiver->handler[h_index](receiver->args[h_index], pkts_burst, nb_rx);
    }
    for (unsigned p_index = 0; p_index < nb_rx; ++p_index) {
        rte_pktmbuf_free(pkts_burst[p_index]);
        // if (rte_mbuf_refcnt_read(pkts_burst[p_index]) > 1) {
        //     rte_mbuf_refcnt_update(pkts_burst[p_index], -1);
        // } else {
        //     rte_pktmbuf_free(pkts_burst[p_index]);
        // }
    }
}

void
init_receiver(unsigned core_id, unsigned in_port,
            struct receiver_t *receiver) {

    receiver->core_id = core_id;
    receiver->in_port = in_port;

    receiver->nb_handler = 0;
    receiver->nb_polls = 0;
    receiver->nb_rec = 0;
    receiver->pkts_received = 0;
    
    receiver->burst_buffer = malloc(BURST_SIZE * sizeof(void*));

    rte_eth_macaddr_get(receiver->in_port, &receiver->mac);
}
