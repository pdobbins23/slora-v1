// Implementation of FloodApp.
// See FloodApp.h for the class-level overview and the three flood variants.

#include "FloodApp.h"
#include "FloodPacket_m.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "inet/common/ModuleAccess.h"
#include "inet/common/Protocol.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

#include "LoRa/LoRaRadio.h"
#include "LoRa/LoRaTagInfo_m.h"

namespace flood {

using namespace inet;
using namespace inet::physicallayer;

Define_Module(FloodApp);

static constexpr short MSG_KIND_SEND        = 1;
static constexpr short MSG_KIND_REBROADCAST = 2;
static constexpr short MSG_KIND_HELLO       = 3;

static constexpr uint32_t MESHTASTIC_BROADCAST = 0xffffffffU;

int FloodApp::numInitStages() const
{
    return inet::NUM_INIT_STAGES;
}

void FloodApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        myId = 0xc0000000U | (uint32_t) getParentModule()->getIndex();

        sendInterval = par("sendInterval");
        initialHopLimit = par("initialHopLimit");
        payloadSizeBytes = par("payloadSizeBytes");
        isSource = par("isSource");
        trafficDestOffset = par("trafficDestOffset");
        trafficRandomDest = par("trafficRandomDest");

        mode = par("mode").stdstringValue();
        cwMin = par("cwMin");
        cwMax = par("cwMax");
        slotTime = par("slotTime");
        snrGoodDb = par("snrGoodDb");
        snrBadDb = par("snrBadDb");

        helloInterval = par("helloInterval");
        helloJitter = par("helloJitter");
        neighbourTimeout = par("neighbourTimeout");

        pktsOriginatedSig = registerSignal("pktsOriginated");
        pktsForwardedSig  = registerSignal("pktsForwarded");
        pktsSuppressedSig = registerSignal("pktsSuppressed");
        pktsDeliveredSig  = registerSignal("pktsDelivered");
    }
    else if (stage == inet::INITSTAGE_LINK_LAYER) {
        loRaRadio = check_and_cast<flora::LoRaRadio *>(
                getParentModule()->getSubmodule("LoRaNic")->getSubmodule("radio"));

        loRaRadio->loRaTP = par("initialLoRaTP").doubleValue();
        loRaRadio->loRaCF = inet::units::values::Hz(par("initialLoRaCF").doubleValue());
        loRaRadio->loRaSF = par("initialLoRaSF");
        loRaRadio->loRaBW = inet::units::values::Hz(par("initialLoRaBW").doubleValue());
        loRaRadio->loRaCR = par("initialLoRaCR");
        loRaRadio->loRaUseHeader = par("initialUseHeader");

        loRaRadio->setRadioMode(IRadio::RADIO_MODE_TRANSCEIVER);
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        if (isSource) {
            sendTimer = new cMessage("sendTimer", MSG_KIND_SEND);
            double initDelay = sendInterval.dbl();
            scheduleAt(simTime() + uniform(initDelay * 0.5, initDelay), sendTimer);
        }
        if (mode == "mpr") {
            helloTimer = new cMessage("helloTimer", MSG_KIND_HELLO);
            scheduleAt(simTime() + uniform(0.0, helloInterval.dbl()), helloTimer);
        }
    }
}

void FloodApp::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
        case MSG_KIND_SEND:
            if (channelBusy()) {
                pktsLbtBackoff++;
                scheduleAt(simTime() + uniform(slotTime.dbl(), 2.0 * slotTime.dbl()), msg);
            }
            else {
                originate();
                scheduleAt(simTime() + sendInterval, msg);
            }
            break;
        case MSG_KIND_REBROADCAST:
            onRebroadcastTimer(msg);
            break;
        case MSG_KIND_HELLO:
            if (channelBusy()) {
                scheduleAt(simTime() + uniform(slotTime.dbl(), 2.0 * slotTime.dbl()), msg);
            }
            else {
                sendHello();
                double jit = helloJitter.dbl();
                scheduleAt(simTime() + helloInterval + uniform(-jit, jit), msg);
            }
            break;
        default:
            throw cRuntimeError("unknown self-message kind %d", msg->getKind());
        }
    } else {
        onPacketFromBelow(msg);
    }
}

void FloodApp::originate()
{
    uint32_t pktId = nextPacketId++;
    rememberPacket({myId, pktId});

    cModule *parent = getParentModule()->getParentModule();
    int n = parent->par("numNodes");
    int self = getParentModule()->getIndex();
    int peer;
    if (trafficRandomDest) {
        peer = intuniform(0, n - 2);
        if (peer >= self) peer++;
    } else {
        peer = (self + trafficDestOffset) % n;
    }
    uint32_t destId = 0xc0000000U | (uint32_t) peer;

    char buf[64];
    snprintf(buf, sizeof(buf), "hello from %08x id %u", myId, pktId);

    pktsOriginated++;
    emit(pktsOriginatedSig, 1L);

    broadcast(myId, pktId, (uint8_t) initialHopLimit, buf, destId, (uint64_t) simTime().inUnit(SIMTIME_NS));
}

void FloodApp::onPacketFromBelow(cMessage *msg)
{
    auto *pkt = check_and_cast<inet::Packet *>(msg);

    auto front = pkt->peekAtFront<Chunk>();
    auto hello = dynamicPtrCast<const FloodHello>(front);
    if (hello != nullptr) {
        uint32_t sender = hello->getSrc();
        if (sender != myId) {
            helloRcvd++;
            noteHeard(sender);
            auto &info = neighbours[sender];
            info.lastSeen = simTime();
            info.n1.clear();
            info.n1.reserve(hello->getNeighboursArraySize());
            for (size_t i = 0; i < hello->getNeighboursArraySize(); i++)
                info.n1.push_back(hello->getNeighbours(i));
            info.mprs.clear();
            info.mprs.reserve(hello->getMprsArraySize());
            for (size_t i = 0; i < hello->getMprsArraySize(); i++)
                info.mprs.push_back(hello->getMprs(i));
            pruneNeighbours();
            recomputeMprSet();
        }
        delete pkt;
        return;
    }

    auto chunk = pkt->peekAtFront<FloodPacket>();
    Key key{chunk->getSrc(), chunk->getPacketId()};

    uint32_t lastHop = chunk->getLastHop();
    if (mode == "mpr") {
        noteHeard(lastHop);
    }

    auto pendingIt = pendingRebroadcasts.find(key);
    if (pendingIt != pendingRebroadcasts.end()) {
        if (mode == "naive") {
            rememberPacket(key);
            delete pkt;
            return;
        }
        cancelAndDelete(pendingIt->second);
        pendingRebroadcasts.erase(pendingIt);
        pktsSuppressed++;
        emit(pktsSuppressedSig, 1L);
        rememberPacket(key);
        delete pkt;
        return;
    }

    if (isInRecentCache(key)) {
        pktsDropDup++;
        delete pkt;
        return;
    }

    rememberPacket(key);
    pktsDelivered++;
    emit(pktsDeliveredSig, 1L);
    if (chunk->getDst() == myId) {
        pktsDeliveredToMe++;
        uint64_t now_ns = (uint64_t) simTime().inUnit(SIMTIME_NS);
        uint64_t tx_ns = chunk->getTxTimeNs();
        if (now_ns >= tx_ns) {
            double latency_s = (double)(now_ns - tx_ns) / 1e9;
            latencyStats.collect(latency_s);
            if (firstDeliveryNs == 0) firstDeliveryNs = now_ns;
            lastDeliveryNs = now_ns;
        }
    }

    bool destinedOnlyForUs = (chunk->getDst() == myId);
    if (chunk->getHopLimit() > 0 && chunk->getSrc() != myId && !destinedOnlyForUs) {
        if (mode == "mpr" && !senderElectedUs(lastHop)) {
            pktsMprDrop++;
            delete pkt;
            return;
        }
        double snrDb = snrBadDb;
        if (auto snirInd = pkt->findTag<SnirInd>()) {
            double snir = snirInd->getMinimumSnir();
            if (snir > 0) snrDb = 10.0 * std::log10(snir);
        }
        simtime_t delay = computeRebroadcastDelay(snrDb);
        scheduleRebroadcast(chunk->getSrc(), chunk->getPacketId(),
                            chunk->getHopLimit() - 1, chunk->getPayload(), delay, chunk->getDst(), chunk->getTxTimeNs());
    }

    delete pkt;
}

simtime_t FloodApp::computeRebroadcastDelay(double snrDb)
{
    if (mode == "naive") {
        return uniform(0, cwMax) * slotTime;
    }
    if (mode == "mpr") {
        return uniform(0, cwMin) * slotTime;
    }
    double cwSize;
    if (snrDb >= snrGoodDb)      cwSize = cwMin;
    else if (snrDb <= snrBadDb)  cwSize = cwMax;
    else {
        double frac = (snrGoodDb - snrDb) / (snrGoodDb - snrBadDb);
        cwSize = cwMin + frac * (cwMax - cwMin);
    }
    double slots = uniform(0, cwSize);
    return slots * slotTime;
}

void FloodApp::scheduleRebroadcast(uint32_t src, uint32_t pktId, uint8_t hop,
                                   const std::string &payload, simtime_t delay, uint32_t dst, uint64_t txTimeNs)
{
    auto *self = new cMessage("rebroadcast", MSG_KIND_REBROADCAST);
    self->addPar("src");     self->par("src").setLongValue(src);
    self->addPar("pktId");   self->par("pktId").setLongValue(pktId);
    self->addPar("hop");     self->par("hop").setLongValue(hop);
    self->addPar("dst");     self->par("dst").setLongValue(dst);
    self->addPar("txTime");  self->par("txTime").setLongValue((long) txTimeNs);
    self->addPar("payload"); self->par("payload").setStringValue(payload.c_str());

    pendingRebroadcasts[{src, pktId}] = self;
    scheduleAt(simTime() + delay, self);
}

void FloodApp::onRebroadcastTimer(cMessage *self)
{
    uint32_t src   = (uint32_t) self->par("src").longValue();
    uint32_t pktId = (uint32_t) self->par("pktId").longValue();
    uint8_t  hop   = (uint8_t)  self->par("hop").longValue();
    uint32_t dst   = (uint32_t) self->par("dst").longValue();
    uint64_t txNs  = (uint64_t) self->par("txTime").longValue();
    std::string payload = self->par("payload").stringValue();

    if (channelBusy()) {
        pktsLbtBackoff++;
        scheduleAt(simTime() + uniform(slotTime.dbl(), 2.0 * slotTime.dbl()), self);
        return;
    }

    pendingRebroadcasts.erase({src, pktId});
    pktsForwarded++;
    emit(pktsForwardedSig, 1L);
    broadcast(src, pktId, hop, payload, dst, txNs);
    delete self;
}

bool FloodApp::channelBusy() const
{
    return loRaRadio->getReceptionState() == IRadio::RECEPTION_STATE_RECEIVING ||
           loRaRadio->getTransmissionState() == IRadio::TRANSMISSION_STATE_TRANSMITTING;
}

void FloodApp::broadcast(uint32_t src, uint32_t pktId, uint8_t hop, const std::string &payload, uint32_t dst, uint64_t txTimeNs)
{
    auto *pkt = new inet::Packet("FloodPacket");

    auto chunk = makeShared<FloodPacket>();
    chunk->setSrc(src);
    chunk->setDst(dst);
    chunk->setLastHop(myId);
    chunk->setPacketId(pktId);
    chunk->setHopLimit(hop);
    chunk->setHopStart((uint8_t) initialHopLimit);
    chunk->setWantAck(false);
    chunk->setTxTimeNs(txTimeNs);
    chunk->setPayload(payload.c_str());
    chunk->setChunkLength(B(4 + 4 + 4 + 4 + 1 + 1 + 1 + 8 + (int) payload.size()));
    pkt->insertAtBack(chunk);

    auto loraTag = pkt->addTagIfAbsent<flora::LoRaTag>();
    loraTag->setBandwidth(loRaRadio->loRaBW);
    loraTag->setCenterFrequency(loRaRadio->loRaCF);
    loraTag->setSpreadFactor(loRaRadio->loRaSF);
    loraTag->setCodeRendundance(loRaRadio->loRaCR);
    loraTag->setPower(mW(math::dBmW2mW(loRaRadio->loRaTP)));

    pkt->addTagIfAbsent<MacAddressReq>()->setDestAddress(MacAddress::BROADCAST_ADDRESS);
    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);

    send(pkt, "socketOut");
}

void FloodApp::sendHello()
{
    pruneNeighbours();
    recomputeMprSet();

    auto *pkt = new inet::Packet("FloodHello");
    auto chunk = makeShared<FloodHello>();
    chunk->setSrc(myId);

    std::vector<uint32_t> n1;
    n1.reserve(neighbours.size());
    for (auto &kv : neighbours) n1.push_back(kv.first);
    std::sort(n1.begin(), n1.end());

    chunk->setNeighboursArraySize(n1.size());
    for (size_t i = 0; i < n1.size(); i++) chunk->setNeighbours(i, n1[i]);

    std::vector<uint32_t> mprs(myMprSet.begin(), myMprSet.end());
    std::sort(mprs.begin(), mprs.end());
    chunk->setMprsArraySize(mprs.size());
    for (size_t i = 0; i < mprs.size(); i++) chunk->setMprs(i, mprs[i]);

    chunk->setChunkLength(B(4 + 4 * n1.size() + 4 * mprs.size()));
    pkt->insertAtBack(chunk);

    auto loraTag = pkt->addTagIfAbsent<flora::LoRaTag>();
    loraTag->setBandwidth(loRaRadio->loRaBW);
    loraTag->setCenterFrequency(loRaRadio->loRaCF);
    loraTag->setSpreadFactor(loRaRadio->loRaSF);
    loraTag->setCodeRendundance(loRaRadio->loRaCR);
    loraTag->setPower(mW(math::dBmW2mW(loRaRadio->loRaTP)));

    pkt->addTagIfAbsent<MacAddressReq>()->setDestAddress(MacAddress::BROADCAST_ADDRESS);
    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);

    helloSent++;
    send(pkt, "socketOut");
}

void FloodApp::noteHeard(uint32_t neighbour)
{
    if (neighbour == myId) return;
    auto &info = neighbours[neighbour];
    info.lastSeen = simTime();
}

void FloodApp::pruneNeighbours()
{
    simtime_t cutoff = simTime() - neighbourTimeout;
    for (auto it = neighbours.begin(); it != neighbours.end(); ) {
        if (it->second.lastSeen < cutoff) it = neighbours.erase(it);
        else ++it;
    }
}

void FloodApp::recomputeMprSet()
{
    myMprSet.clear();
    if (neighbours.empty()) return;

    std::set<uint32_t> n1;
    for (auto &kv : neighbours) n1.insert(kv.first);

    std::set<uint32_t> uncovered;
    for (auto &kv : neighbours) {
        for (uint32_t two : kv.second.n1) {
            if (two == myId) continue;
            if (n1.count(two)) continue;
            uncovered.insert(two);
        }
    }

    while (!uncovered.empty()) {
        uint32_t bestId = 0;
        size_t bestCover = 0;
        bool found = false;
        for (uint32_t candidate : n1) {
            if (myMprSet.count(candidate)) continue;
            auto it = neighbours.find(candidate);
            if (it == neighbours.end()) continue;
            size_t cover = 0;
            for (uint32_t two : it->second.n1) {
                if (uncovered.count(two)) cover++;
            }
            // Tie-break by smaller ID for determinism across nodes.
            if (cover > bestCover || (cover == bestCover && found && candidate < bestId)) {
                if (cover > 0) {
                    bestCover = cover;
                    bestId = candidate;
                    found = true;
                }
            }
        }
        if (!found) break;
        myMprSet.insert(bestId);
        auto it = neighbours.find(bestId);
        if (it != neighbours.end()) {
            for (uint32_t two : it->second.n1) uncovered.erase(two);
        }
    }
}

bool FloodApp::senderElectedUs(uint32_t sender) const
{
    auto it = neighbours.find(sender);
    if (it == neighbours.end()) return false;
    for (uint32_t m : it->second.mprs) {
        if (m == myId) return true;
    }
    return false;
}

bool FloodApp::isInRecentCache(const Key &k) const
{
    return recentSet.count(k) > 0;
}

void FloodApp::rememberPacket(const Key &k)
{
    if (recentSet.insert(k).second) {
        recentList.push_back(k);
        if (recentList.size() > RECENT_CACHE_CAP) {
            recentSet.erase(recentList.front());
            recentList.pop_front();
        }
    }
}

void FloodApp::finish()
{
    recordScalar("pktsOriginated", pktsOriginated);
    recordScalar("pktsForwarded",  pktsForwarded);
    recordScalar("pktsSuppressed", pktsSuppressed);
    recordScalar("pktsDelivered",  pktsDelivered);
    recordScalar("pktsDeliveredToMe", pktsDeliveredToMe);
    recordScalar("pktsDropDup",    pktsDropDup);
    recordScalar("pktsLbtBackoff", pktsLbtBackoff);
    recordScalar("helloSent",      helloSent);
    recordScalar("helloRcvd",      helloRcvd);
    recordScalar("pktsMprDrop",    pktsMprDrop);
    recordScalar("mprSetSize",     (long) myMprSet.size());
    recordScalar("neighbourCount", (long) neighbours.size());
    recordScalar("latencyMean", latencyStats.getCount() > 0 ? latencyStats.getMean() : 0);
    recordScalar("latencyMin", latencyStats.getCount() > 0 ? latencyStats.getMin() : 0);
    recordScalar("latencyMax", latencyStats.getCount() > 0 ? latencyStats.getMax() : 0);
    if (firstDeliveryNs > 0 && lastDeliveryNs > firstDeliveryNs && pktsDeliveredToMe > 1) {
        double window_s = (double)(lastDeliveryNs - firstDeliveryNs) / 1e9;
        recordScalar("throughputPps", (double)(pktsDeliveredToMe - 1) / window_s);
    } else {
        recordScalar("throughputPps", 0);
    }

    cancelAndDelete(sendTimer);
    sendTimer = nullptr;
    cancelAndDelete(helloTimer);
    helloTimer = nullptr;
    for (auto &kv : pendingRebroadcasts) cancelAndDelete(kv.second);
    pendingRebroadcasts.clear();
}

}
