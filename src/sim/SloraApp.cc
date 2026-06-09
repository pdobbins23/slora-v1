// Implementation of SloraApp.
// See SloraApp.h for the class-level overview.

#include "SloraApp.h"

#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <inet/common/InitStages.h>
#include <inet/common/Protocol.h>
#include <inet/common/ProtocolTag_m.h>
#include <inet/common/packet/chunk/BytesChunk.h>
#include <inet/linklayer/common/MacAddressTag_m.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <LoRa/LoRaRadio.h>
#include <LoRa/LoRaTagInfo_m.h>

using namespace inet;
using namespace inet::physicallayer;

namespace slora {

Define_Module(SloraApp);

SloraApp::~SloraApp()
{
    for (auto &kv : wakeups) {
        cancelAndDelete(kv.second);
    }
    wakeups.clear();
    if (trafficTimer) {
        cancelAndDelete(trafficTimer);
        trafficTimer = nullptr;
    }
    if (bcastBackoffTimer) {
        cancelAndDelete(bcastBackoffTimer);
        bcastBackoffTimer = nullptr;
    }
    if (node) {
        slora_node_deinit(node);
        node = nullptr;
    }
    bcastTxQueue.clear();
    while (!dataTxQueue.empty()) {
        delete dataTxQueue.front();
        dataTxQueue.pop_front();
    }
}

uint64_t SloraApp::nowNs() const
{
    return static_cast<uint64_t>(simTime().inUnit(SIMTIME_NS));
}

void SloraApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        broadcastFreqHz   = par("broadcastFreqHz").doubleValue();
        broadcastBwHz     = par("broadcastBwHz").doubleValue();
        broadcastSF       = par("broadcastSF");
        broadcastPowerDbm = par("broadcastTxPowerDbm").doubleValue();
        broadcastCR       = par("broadcastCR");

        dataSF       = par("dataSF");
        dataPowerDbm = par("dataTxPowerDbm").doubleValue();
        dataCR       = par("dataCR");

        trafficEnabled = par("trafficEnabled");
        trafficInterval = par("trafficInterval").doubleValue();
        trafficPayloadBytes = par("trafficPayloadBytes");
        trafficDestOffset = par("trafficDestOffset");
        trafficRandomDest = par("trafficRandomDest");
        trafficMode = par("trafficMode").stdstringValue();

        bcastCwMin = par("broadcastCwMin");
        bcastCwMax = par("broadcastCwMax");
        bcastSlotTime = par("broadcastSlotTime").doubleValue();
        bcastBackoffTimer = new cMessage("slora_bcast_backoff");

        uint8_t seed[32] = {0};
        int nodeIndex = getParentModule()->getIndex();
        std::memcpy(seed, &nodeIndex, sizeof(nodeIndex));

        node = slora_node_init(seed);
        if (!node) throw cRuntimeError("slora_node_init failed");

        double announceInterval = par("announceInterval").doubleValue();
        double announceJitter = par("announceJitter").doubleValue();
        slora_node_set_announce_interval(node,
            static_cast<uint64_t>(announceInterval * 1e9),
            static_cast<uint64_t>(announceJitter * 1e9));

        if (trafficEnabled) {
            trafficTimer = new cMessage("slora_traffic");
        }
    }
    else if (stage == INITSTAGE_PHYSICAL_LAYER) {
        broadcastRadio = check_and_cast<flora::LoRaRadio *>(
            getParentModule()->getSubmodule("broadcastNic")->getSubmodule("radio"));

        broadcastRadio->loRaCF = units::values::Hz(broadcastFreqHz);
        broadcastRadio->loRaBW = units::values::Hz(broadcastBwHz);
        broadcastRadio->loRaSF = broadcastSF;
        broadcastRadio->loRaCR = broadcastCR;
        broadcastRadio->loRaTP = broadcastPowerDbm;
        broadcastRadio->setRadioMode(IRadio::RADIO_MODE_TRANSCEIVER);
        broadcastRadio->subscribe(IRadio::transmissionStateChangedSignal, this);

        dataRadio = check_and_cast<flora::LoRaRadio *>(
            getParentModule()->getSubmodule("dataNic")->getSubmodule("radio"));

        dataRadio->loRaSF = dataSF;
        dataRadio->loRaCR = dataCR;
        dataRadio->loRaBW = units::values::Hz(500000);
        dataRadio->loRaCF = units::values::Hz(910000000);
        dataRadio->loRaTP = dataPowerDbm;
        dataRadio->setRadioMode(IRadio::RADIO_MODE_TRANSCEIVER);
        dataRadio->subscribe(IRadio::transmissionStateChangedSignal, this);
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        slora_node_start(node, nowNs());
        drainActions();
        std::string offsets = par("targetPeerOffsets").stdstringValue();
        if (!offsets.empty()) {
            cModule *parent = getParentModule()->getParentModule();
            int self = getParentModule()->getIndex();
            int n = parent->par("numNodes");
            std::stringstream ss(offsets);
            std::string token;
            while (std::getline(ss, token, ',')) {
                if (token.empty()) continue;
                int offset = std::stoi(token);
                int peerIdx = self + offset;
                if (peerIdx < 0 || peerIdx >= n) continue;
                if (peerIdx == self) continue;
                cModule *peerNode = parent->getSubmodule("nodes", peerIdx);
                if (!peerNode) continue;
                SloraApp *peerSlora = check_and_cast<SloraApp *>(peerNode->getSubmodule("app"));
                if (!peerSlora || !peerSlora->node) continue;
                uint8_t peerAddr[16];
                slora_node_addr(peerSlora->node, peerAddr);
                slora_node_add_target_peer(node, peerAddr);
            }
            drainActions();
        }
        if (trafficEnabled && trafficTimer) {
            scheduleAt(simTime() + 10.0, trafficTimer);
        }
    }
}

void SloraApp::handleMessage(cMessage *msg)
{
    if (auto *w = dynamic_cast<WakeupMsg *>(msg)) {
        uint32_t tok = w->token;
        wakeups.erase(tok);
        slora_node_on_wakeup(node, tok, nowNs());
        delete msg;
        drainActions();
        return;
    }

    if (msg == bcastBackoffTimer) {
        if (broadcastChannelBusy()) {
            scheduleAt(simTime() + uniform(bcastSlotTime, 2.0 * bcastSlotTime), bcastBackoffTimer);
            return;
        }
        if (!bcastTxQueue.empty()) {
            auto bytes = std::move(bcastTxQueue.front());
            bcastTxQueue.pop_front();
            if (bytes.size() == 105 && bytes[0] == 0x00) {
                uint64_t asn = nowNs() / 100000000ULL;
                for (int i = 0; i < 8; i++) bytes[1 + i] = (uint8_t)((asn >> (i * 8)) & 0xff);
            }
            auto pkt = new Packet("SloraBcast");
            auto chunk = makeShared<BytesChunk>(bytes);
            pkt->insertAtBack(chunk);
            tagForBroadcast(pkt);
            bcastTxBusy = true;
            send(pkt, "broadcastOut");
        }
        if (!bcastTxQueue.empty()) scheduleBcastBackoff();
        return;
    }

    if (msg == trafficTimer) {
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
        cModule *peerNode = parent->getSubmodule("nodes", peer);
        cModule *peerApp = peerNode->getSubmodule("app");
        SloraApp *peerSlora = check_and_cast<SloraApp *>(peerApp);
        uint8_t peerAddr[16];
        slora_node_addr(peerSlora->node, peerAddr);
        std::vector<uint8_t> payload(trafficPayloadBytes, 0);
        uint64_t now_ns = nowNs();
        if (trafficPayloadBytes >= 8) {
            for (int i = 0; i < 8; i++) payload[i] = (uint8_t)((now_ns >> (i * 8)) & 0xff);
        }
        if (trafficMode == "unicast") {
            slora_node_send_payload(node, peerAddr, payload.data(), payload.size(), now_ns);
        } else {
            slora_node_send_echo(node, peerAddr, payload.data(), payload.size(), now_ns);
        }
        sentCount++;
        drainActions();
        scheduleAt(simTime() + trafficInterval, trafficTimer);
        return;
    }

    if (msg->isSelfMessage()) {
        delete msg;
        return;
    }

    cGate *arrival = msg->getArrivalGate();
    auto *pkt = check_and_cast<Packet *>(msg);

    if (arrival == gate("broadcastIn")) {
        auto bytesChunk = pkt->peekDataAsBytes();
        const auto &v = bytesChunk->getBytes();
        slora_node_on_broadcast_rx(node, v.data(), v.size(), nowNs());
        drainActions();
    }
    else if (arrival == gate("dataIn")) {
        auto bytesChunk = pkt->peekDataAsBytes();
        const auto &v = bytesChunk->getBytes();
        slora_node_on_data_rx(node, v.data(), v.size(), curDataFreqHz, curDataBwHz, nowNs());
        drainActions();
    }

    delete pkt;
}

void SloraApp::finish()
{
    recordScalar("sent", sentCount);
    recordScalar("delivered", deliveredCount);
    recordScalar("duplicate", duplicateCount);
    recordScalar("links", node ? slora_node_link_count(node) : 0);
    recordScalar("neighbors", node ? slora_node_neighbor_count(node) : 0);
    recordScalar("routes", node ? slora_node_route_count(node) : 0);
    recordScalar("pendingTx", node ? slora_node_pending_tx_total(node) : 0);
    recordScalar("dataFreqHzFinal", static_cast<double>(curDataFreqHz));
    recordScalar("tuneCount", tuneCount);
    recordScalar("forwarded", node ? slora_node_forwarded_count(node) : 0);
    recordScalar("dropNoRoute", node ? slora_node_drop_no_route(node) : 0);
    recordScalar("dropNoLink", node ? slora_node_drop_no_link(node) : 0);
    recordScalar("dropLinkDown", node ? slora_node_drop_link_down(node) : 0);
    recordScalar("dropQueueFull", node ? slora_node_drop_queue_full(node) : 0);
    recordScalar("latencyMean", latencyStats.getCount() > 0 ? latencyStats.getMean() : 0);
    recordScalar("latencyMin", latencyStats.getCount() > 0 ? latencyStats.getMin() : 0);
    recordScalar("latencyMax", latencyStats.getCount() > 0 ? latencyStats.getMax() : 0);
    if (firstDeliveryNs > 0 && lastDeliveryNs > firstDeliveryNs && deliveredCount > 1) {
        double window_s = (double)(lastDeliveryNs - firstDeliveryNs) / 1e9;
        recordScalar("throughputPps", (double)(deliveredCount - 1) / window_s);
    } else {
        recordScalar("throughputPps", 0);
    }
}

void SloraApp::receiveSignal(cComponent *source, simsignal_t signal, intval_t value, cObject *details)
{
    if (signal != IRadio::transmissionStateChangedSignal) return;
    if (source == check_and_cast<cComponent *>(broadcastRadio)) {
        bcastTxBusy = (value == IRadio::TRANSMISSION_STATE_TRANSMITTING);
        pumpBcastQueue();
    }
    else if (dataRadio && source == check_and_cast<cComponent *>(dataRadio)) {
        dataTxBusy = (value == IRadio::TRANSMISSION_STATE_TRANSMITTING);
        pumpDataQueue();
    }
}

void SloraApp::drainActions()
{
    slora_action_t act;
    while (slora_node_next_action(node, &act)) {
        switch (act.tag) {
            case SLORA_ACTION_TX_BROADCAST:
                deliverTxBroadcast(act.payload.tx_broadcast.bytes, act.payload.tx_broadcast.bytes_len);
                break;
            case SLORA_ACTION_TX_DATA:
                deliverTxData(act.payload.tx_data.bytes, act.payload.tx_data.bytes_len,
                              act.payload.tx_data.freq_hz, act.payload.tx_data.bw_hz);
                break;
            case SLORA_ACTION_TUNE_DATA:
                tuneDataRadio(act.payload.tune_data.freq_hz, act.payload.tune_data.bw_hz);
                break;
            case SLORA_ACTION_SCHEDULE:
                scheduleWakeup(act.payload.schedule.token, act.payload.schedule.delay_ns);
                break;
            case SLORA_ACTION_CANCEL:
                cancelWakeup(act.payload.cancel.token);
                break;
            case SLORA_ACTION_DELIVER:
                if (act.payload.deliver.payload_len >= 8 && act.payload.deliver.payload) {
                    uint64_t tx_ns = 0;
                    for (int i = 0; i < 8; i++) tx_ns |= (uint64_t)act.payload.deliver.payload[i] << (i * 8);
                    uint64_t src_lo = 0, src_hi = 0;
                    for (int i = 0; i < 8; i++) src_lo |= (uint64_t)act.payload.deliver.from[i] << (i * 8);
                    for (int i = 0; i < 8; i++) src_hi |= (uint64_t)act.payload.deliver.from[8 + i] << (i * 8);
                    uint64_t dedup_key = tx_ns ^ src_lo ^ (src_hi * 0x9E3779B97F4A7C15ULL);
                    if (deliveredDedup.insert(dedup_key).second) {
                        deliveredCount++;
                        uint64_t now_ns = nowNs();
                        if (now_ns >= tx_ns) {
                            double latency_s = (double)(now_ns - tx_ns) / 1e9;
                            latencyStats.collect(latency_s);
                            if (firstDeliveryNs == 0) firstDeliveryNs = now_ns;
                            lastDeliveryNs = now_ns;
                        }
                    } else {
                        duplicateCount++;
                    }
                } else {
                    deliveredCount++;
                }
                break;
            default:
                throw cRuntimeError("unknown slora action tag %d", (int)act.tag);
        }
    }
}

void SloraApp::deliverTxBroadcast(const uint8_t *bytes, size_t len)
{
    bcastTxQueue.emplace_back(bytes, bytes + len);
    pumpBcastQueue();
}

void SloraApp::deliverTxData(const uint8_t *bytes, size_t len, uint32_t freq_hz, uint32_t bw_hz)
{
    if (curDataFreqHz != freq_hz || curDataBwHz != bw_hz) {
        tuneDataRadio(freq_hz, bw_hz);
    }
    auto pkt = new Packet("SloraUnicast");
    std::vector<uint8_t> copy(bytes, bytes + len);
    auto chunk = makeShared<BytesChunk>(copy);
    pkt->insertAtBack(chunk);
    tagForData(pkt, freq_hz, bw_hz);
    dataTxQueue.push_back(pkt);
    pumpDataQueue();
}

void SloraApp::tuneDataRadio(uint32_t freq_hz, uint32_t bw_hz)
{
    if (!dataRadio) return;
    if (curDataFreqHz == freq_hz && curDataBwHz == bw_hz) return;
    dataRadio->loRaCF = units::values::Hz(static_cast<double>(freq_hz));
    dataRadio->loRaBW = units::values::Hz(static_cast<double>(bw_hz));
    curDataFreqHz = freq_hz;
    curDataBwHz = bw_hz;
    tuneCount++;
}

void SloraApp::scheduleWakeup(uint32_t token, uint64_t delay_ns)
{
    cancelWakeup(token);
    auto *msg = new WakeupMsg(token);
    wakeups[token] = msg;
    scheduleAt(simTime() + SimTime(static_cast<int64_t>(delay_ns), SIMTIME_NS), msg);
}

void SloraApp::cancelWakeup(uint32_t token)
{
    auto it = wakeups.find(token);
    if (it == wakeups.end()) return;
    cancelAndDelete(it->second);
    wakeups.erase(it);
}

void SloraApp::tagForBroadcast(Packet *pkt)
{
    auto tag = pkt->addTagIfAbsent<flora::LoRaTag>();
    tag->setBandwidth(units::values::Hz(broadcastBwHz));
    tag->setCenterFrequency(units::values::Hz(broadcastFreqHz));
    tag->setSpreadFactor(broadcastSF);
    tag->setCodeRendundance(broadcastCR);
    tag->setPower(mW(math::dBmW2mW(broadcastPowerDbm)));
    tag->setUseHeader(true);
    pkt->addTagIfAbsent<MacAddressReq>()->setDestAddress(MacAddress::BROADCAST_ADDRESS);
    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);
}

void SloraApp::tagForData(Packet *pkt, uint32_t freq_hz, uint32_t bw_hz)
{
    auto tag = pkt->addTagIfAbsent<flora::LoRaTag>();
    tag->setBandwidth(units::values::Hz(static_cast<double>(bw_hz)));
    tag->setCenterFrequency(units::values::Hz(static_cast<double>(freq_hz)));
    tag->setSpreadFactor(dataSF);
    tag->setCodeRendundance(dataCR);
    tag->setPower(mW(math::dBmW2mW(dataPowerDbm)));
    tag->setUseHeader(true);
    pkt->addTagIfAbsent<MacAddressReq>()->setDestAddress(MacAddress::BROADCAST_ADDRESS);
    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);
}

void SloraApp::pumpBcastQueue()
{
    if (bcastTxBusy || bcastTxQueue.empty()) return;
    if (bcastBackoffTimer && bcastBackoffTimer->isScheduled()) return;
    scheduleBcastBackoff();
}

void SloraApp::scheduleBcastBackoff()
{
    if (!bcastBackoffTimer) return;
    if (bcastBackoffTimer->isScheduled()) return;
    int cw = bcastCwMin + (bcastBackoffCount < (bcastCwMax - bcastCwMin) ? bcastBackoffCount : (bcastCwMax - bcastCwMin));
    double delay = uniform(0.0, cw * bcastSlotTime);
    scheduleAt(simTime() + delay, bcastBackoffTimer);
}

bool SloraApp::broadcastChannelBusy() const
{
    if (!broadcastRadio) return false;
    return broadcastRadio->getReceptionState() == IRadio::RECEPTION_STATE_RECEIVING ||
           broadcastRadio->getTransmissionState() == IRadio::TRANSMISSION_STATE_TRANSMITTING;
}

void SloraApp::pumpDataQueue()
{
    if (dataTxBusy || dataTxQueue.empty()) return;
    Packet *pkt = dataTxQueue.front();
    dataTxQueue.pop_front();
    dataTxBusy = true;
    send(pkt, "dataOut");
}

}
