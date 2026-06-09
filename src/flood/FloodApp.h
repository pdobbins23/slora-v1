// The OMNeT++ per-node application module for the three flood baselines.
// A `mode` parameter picks one of FloodNaive, FloodSNR, or FloodMPR.
// FloodNaive rebroadcasts every received packet exactly once within the hop
// limit.
// FloodSNR is the Meshtastic style. It uses an SNR-weighted contention window
// and suppresses its own rebroadcast if it hears the same packet from someone
// else during backoff.
// FloodMPR is the OLSR style. Only nodes elected as MPR by the sender
// rebroadcast, which cuts redundant forwards.
// None of the three use crypto or a routing layer, and they all run on a
// single broadcast radio.

#pragma once

#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <omnetpp.h>

#include "inet/common/packet/Packet.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"

namespace flora { class LoRaRadio; }

namespace flood {

using namespace omnetpp;

class FloodApp : public cSimpleModule
{
  protected:
    using Key = std::pair<uint32_t, uint32_t>;

    uint32_t myId = 0;
    uint32_t nextPacketId = 1;

    static constexpr size_t RECENT_CACHE_CAP = 32;
    std::deque<Key> recentList;
    std::set<Key> recentSet;

    std::map<Key, cMessage *> pendingRebroadcasts;

    cMessage *sendTimer = nullptr;
    cMessage *helloTimer = nullptr;

    simtime_t sendInterval;
    int initialHopLimit = 3;
    int payloadSizeBytes = 32;
    bool isSource = false;

    std::string mode = "snr";

    int cwMin = 2;
    int cwMax = 8;
    simtime_t slotTime;
    double snrGoodDb = -5.0;
    double snrBadDb = -15.0;

    simtime_t helloInterval;
    simtime_t helloJitter;
    simtime_t neighbourTimeout;

    struct NeighbourInfo {
        simtime_t lastSeen;
        std::vector<uint32_t> n1;
        std::vector<uint32_t> mprs;
    };
    std::map<uint32_t, NeighbourInfo> neighbours;
    std::set<uint32_t> myMprSet;

    long pktsOriginated = 0;
    long pktsForwarded  = 0;
    long pktsSuppressed = 0;
    long pktsDelivered  = 0;
    long pktsDeliveredToMe = 0;
    long pktsDropDup    = 0;
    long pktsLbtBackoff = 0;
    long helloSent      = 0;
    long helloRcvd      = 0;
    long pktsMprDrop    = 0;

    int trafficDestOffset = 1;
    bool trafficRandomDest = false;

    simsignal_t pktsOriginatedSig = -1;
    simsignal_t pktsForwardedSig  = -1;
    simsignal_t pktsSuppressedSig = -1;
    simsignal_t pktsDeliveredSig  = -1;

    flora::LoRaRadio *loRaRadio = nullptr;

  protected:
    int numInitStages() const override;
    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;
    void finish() override;

    void originate();
    void onPacketFromBelow(cMessage *msg);
    void onRebroadcastTimer(cMessage *self);

    bool isInRecentCache(const Key &k) const;
    void rememberPacket(const Key &k);
    simtime_t computeRebroadcastDelay(double snrDb);
    bool channelBusy() const;

    void scheduleRebroadcast(uint32_t src, uint32_t pktId, uint8_t hop,
                             const std::string &payload, simtime_t delay, uint32_t dst, uint64_t txTimeNs);
    void broadcast(uint32_t src, uint32_t pktId, uint8_t hop, const std::string &payload, uint32_t dst, uint64_t txTimeNs);

    void sendHello();
    void noteHeard(uint32_t neighbour);
    void pruneNeighbours();
    // Greedy OLSR-style MPR election.
    // Cost is O(|N1| * |N2|) per run, which is fine because the recompute only
    // fires once per HELLO period (5 s) and the practical neighbour count is small.
    void recomputeMprSet();
    bool senderElectedUs(uint32_t sender) const;

    cStdDev latencyStats;
    uint64_t firstDeliveryNs = 0;
    uint64_t lastDeliveryNs = 0;
};

}
