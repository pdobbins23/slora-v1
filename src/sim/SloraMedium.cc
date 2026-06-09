// Implementation of SloraMedium.
// See SloraMedium.h for the why behind the kind-tagging override.

#include "SloraMedium.h"

#include "inet/common/packet/Packet.h"
#include "inet/physicallayer/wireless/common/signal/WirelessSignal.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IArrival.h"
#include "LoRaPhy/LoRaPhyPreamble_m.h"

namespace slora {

using namespace omnetpp;
using namespace inet;
using namespace inet::physicallayer;

Define_Module(SloraMedium);

static constexpr short KIND_BROADCAST = 4;
static constexpr short KIND_UNICAST   = 2;

void SloraMedium::sendToRadio(IRadio *transmitter, const IRadio *receiver,
                              const IWirelessSignal *transmittedSignal)
{
    const ITransmission *transmission = transmittedSignal->getTransmission();
    if (receiver == transmitter || receiver->getReceiver() == nullptr
            || !isPotentialReceiver(receiver, transmission))
        return;

    auto transmitterRadio = const_cast<cSimpleModule *>(
            check_and_cast<const cSimpleModule *>(transmitter));
    cMethodCallContextSwitcher contextSwitcher(transmitterRadio);
    contextSwitcher.methodCall("sendToRadio");

    const IArrival *arrival = getArrival(receiver, transmission);
    simtime_t propagationTime = arrival->getStartPropagationTime();

    auto receivedSignal = static_cast<WirelessSignal *>(createReceiverSignal(transmission));

    if (auto packet = transmission->getPacket()) {
        auto preamble = packet->peekAtFront<flora::LoRaPhyPreamble>(b(-1), Chunk::PF_ALLOW_NULLPTR);
        if (preamble != nullptr)
            receivedSignal->setKind(preamble->getBandwidth() >= Hz(500e3) ? KIND_BROADCAST
                                                                          : KIND_UNICAST);
    }

    cGate *gate = receiver->getRadioGate()->getPathStartGate();
    transmitterRadio->sendDirect(receivedSignal, propagationTime,
                                 transmission->getDuration(), gate);
    communicationCache->setCachedSignal(receiver, transmission, receivedSignal);
    signalSendCount++;
}

}
