#include <zephyr/kernel.h>
#include <zephyr/net/ieee802154_radio.h>
#include <zephyr/sys/util.h>
#include <nrf_802154.h>
#include <nrf_802154_const.h>
#include <stdlib.h>
#include "sniffer.h"

static const struct device *radio_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_ieee802154));
static struct ieee802154_radio_api *radio_api;

enum net_verdict ieee802154_handle_ack(struct net_if *iface,
                       struct net_pkt *pkt)
{
    ARG_UNUSED(iface);
    ARG_UNUSED(pkt);

    return NET_DROP;
}

int net_recv_data(struct net_if *iface, struct net_pkt *pkt)
{
    if (!pkt) {
        return -EINVAL;
    }

    if (net_pkt_is_empty(pkt)) {
        return -ENODATA;
    }

    uint8_t *psdu = net_buf_frag_last(pkt->buffer)->data;
    size_t length = net_buf_frags_len(pkt->buffer);
    uint8_t lqi = net_pkt_ieee802154_lqi(pkt);
    int8_t rssi = net_pkt_ieee802154_rssi_dbm(pkt);
    struct net_ptp_time *pkt_time = net_pkt_timestamp(pkt);
    uint64_t timestamp =
        pkt_time->second * USEC_PER_SEC + pkt_time->nanosecond / NSEC_PER_USEC;

    log_packet(psdu, length, lqi, rssi, timestamp);

    net_pkt_unref(pkt);

    return 0;
}

void sniffer_zephyr_init() {
    struct ieee802154_config config = {
        .promiscuous = true
    };

    radio_api = (struct ieee802154_radio_api *)radio_dev->api;
    __ASSERT_NO_MSG(radio_api);

#if !IS_ENABLED(CONFIG_NRF_802154_SERIALIZATION)
    /* The serialization API does not support disabling the auto-ack. */
    nrf_802154_auto_ack_set(false);
#endif

    radio_api->configure(radio_dev, IEEE802154_CONFIG_PROMISCUOUS, &config);
}

void sniffer_zephyr_set_channel(uint32_t channel) {
    radio_api->set_channel(radio_dev, channel);
}

uint32_t sniffer_zephyr_get_channel() {
    return nrf_802154_channel_get();
}

uint32_t sniffer_zephyr_get_channel_max() {
    return 26;
}

uint32_t sniffer_zephyr_get_channel_min() {
    return 11;
}

void sniffer_zephyr_start() {
    radio_api->start(radio_dev);
}

void sniffer_zephyr_stop() {
    radio_api->stop(radio_dev);
}

sniffer_t sniffer_zephyr = {
    sniffer_zephyr_init,
    sniffer_zephyr_start,
    sniffer_zephyr_stop,
    sniffer_zephyr_get_channel,
    sniffer_zephyr_set_channel,
    sniffer_zephyr_get_channel_min,
    sniffer_zephyr_get_channel_max,
};
