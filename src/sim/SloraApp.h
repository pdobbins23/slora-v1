// The OMNeT++ per-node application module for SLoRa.
// SloraApp wraps one libslora `Node` and drives two LoRa NICs, one for the
// broadcast radio and one for the data radio.
// When the Zig stack hands an `Action` value out (tx_broadcast, tx_data,
// tune_data, schedule, deliver), SloraApp translates it into the matching
// OMNeT++ gate send, radio reconfiguration, or self-message.

#ifndef __SLORA_APP_H
#define __SLORA_APP_H

#include <omnetpp.h>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <inet/common/packet/Packet.h>
#include <inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h>
#include <LoRa/LoRaRadio.h>

#include "slora.h"

namespace slora {

using namespace omnetpp;
using namespace inet;

class WakeupMsg : public cMessage {
  public:
    uint32_t token;
    WakeupMsg(uint32_t t) : cMessage("slora_wakeup"), token(t) {}
};

class SloraApp : public cSimpleModule, public cListener
{
  public:
    SloraApp() = default;
    ~SloraApp() override;

  protected:
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;
    void finish() override;
    void receiveSignal(cComponent *source, simsignal_t signal, intval_t value, cObject *details) override;

  private:
    slora_node_handle_t *node = nullptr;
    std::unordered_map<uint32_t, WakeupMsg *> wakeups;

    double broadcastFreqHz;
    double broadcastBwHz;
    int    broadcastSF;
    double broadcastPowerDbm;
    int    broadcastCR;

    int    dataSF;
    double dataPowerDbm;
    int    dataCR;

    flora::LoRaRadio *broadcastRadio = nullptr;
    std::deque<std::vector<uint8_t>> bcastTxQueue;
    bool bcastTxBusy = false;
    cMessage *bcastBackoffTimer = nullptr;
    int    bcastCwMin;
    int    bcastCwMax;
    double bcastSlotTime;
    int    bcastBackoffCount = 0;

    flora::LoRaRadio *dataRadio = nullptr;
    std::deque<Packet *> dataTxQueue;
    bool dataTxBusy = false;
    uint32_t curDataFreqHz = 0;
    uint32_t curDataBwHz = 0;
    uint32_t tuneCount = 0;

    cMessage *trafficTimer = nullptr;
    double trafficInterval;
    bool   trafficEnabled;
    int    trafficPayloadBytes;
    int    trafficDestOffset;
    bool   trafficRandomDest;
    std::string trafficMode;
    int    deliveredCount = 0;
    int    duplicateCount = 0;
    int    sentCount = 0;
    cStdDev latencyStats;
    uint64_t firstDeliveryNs = 0;
    uint64_t lastDeliveryNs = 0;
    std::unordered_set<uint64_t> deliveredDedup;

    void drainActions();
    void deliverTxBroadcast(const uint8_t *bytes, size_t len);
    void deliverTxData(const uint8_t *bytes, size_t len, uint32_t freq_hz, uint32_t bw_hz);
    void tuneDataRadio(uint32_t freq_hz, uint32_t bw_hz);
    void scheduleWakeup(uint32_t token, uint64_t delay_ns);
    void cancelWakeup(uint32_t token);
    void tagForBroadcast(Packet *pkt);
    void tagForData(Packet *pkt, uint32_t freq_hz, uint32_t bw_hz);
    void pumpBcastQueue();
    void pumpDataQueue();
    void scheduleBcastBackoff();
    bool broadcastChannelBusy() const;
    uint64_t nowNs() const;
};

}

#endif
