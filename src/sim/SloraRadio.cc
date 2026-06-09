// Implementation of SloraRadio.
// See SloraRadio.h for the class-level overview.

#include "SloraRadio.h"

#include "inet/common/packet/Packet.h"
#include "inet/common/Simsignals.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

#include "LoRa/LoRaTagInfo_m.h"
#include "LoRaPhy/LoRaPhyPreamble_m.h"

namespace slora {

using namespace inet;
using namespace inet::physicallayer;

Define_Module(SloraRadio);

void SloraRadio::handleUpperPacket(Packet *packet)
{
    emit(packetReceivedFromUpperSignal, packet);
    if (!isTransmitterMode(radioMode)) {
        EV_ERROR << "Radio is not in transmitter or transceiver mode, dropping frame." << endl;
        delete packet;
        return;
    }

    auto loraTag = packet->removeTag<flora::LoRaTag>();
    auto preamble = makeShared<flora::LoRaPhyPreamble>();

    preamble->setBandwidth(loraTag->getBandwidth());
    preamble->setCenterFrequency(loraTag->getCenterFrequency());
    preamble->setCodeRendundance(loraTag->getCodeRendundance());
    preamble->setPower(loraTag->getPower());
    preamble->setSpreadFactor(loraTag->getSpreadFactor());
    preamble->setUseHeader(loraTag->getUseHeader());

    if (auto destReq = packet->findTag<MacAddressReq>())
        preamble->setReceiverAddress(destReq->getDestAddress());
    else
        preamble->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);

    auto signalPowerReq = packet->addTagIfAbsent<SignalPowerReq>();
    signalPowerReq->setPower(loraTag->getPower());

    preamble->setChunkLength(b(16));
    packet->insertAtFront(preamble);

    if (transmissionTimer->isScheduled())
        throw cRuntimeError("Received frame from upper layer while already transmitting.");
    if (separateTransmissionParts)
        startTransmission(packet, IRadioSignal::SIGNAL_PART_PREAMBLE);
    else
        startTransmission(packet, IRadioSignal::SIGNAL_PART_WHOLE);
}

}
