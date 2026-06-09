// Implementation of SloraReceiver.
// See SloraReceiver.h for the class-level overview.

#include "SloraReceiver.h"

#include "LoRa/LoRaRadio.h"
#include "LoRaPhy/LoRaBandListening.h"
#include "LoRaPhy/LoRaReception.h"

namespace slora {

using namespace inet::physicallayer;

Define_Module(SloraReceiver);

bool SloraReceiver::computeIsReceptionAttempted(
    const IListening *listening,
    const IReception *reception,
    IRadioSignal::SignalPart part,
    const IInterference *interference) const
{
    return !isPacketCollided(reception, part, interference);
}

bool SloraReceiver::computeIsReceptionPossible(
    const IListening *listening,
    const IReception *reception,
    IRadioSignal::SignalPart part) const
{
    // FLoRa's LoRaMedium::addTransmission caches a listening band built from the
    // transmission's PHY, not the receiver's. The listening-vs-reception CF check
    // in stock LoRaReceiver always matches because of that. Stock FLoRa gets away
    // with this because every node listens on the same frequency.
    // SLoRa's FDMA needs a real per-radio filter, so the override compares the
    // reception PHY against this radio's own loRaCF, loRaBW, and loRaSF directly.
    auto loRaRadio = check_and_cast<flora::LoRaRadio *>(getParentModule());
    auto loRaReception = check_and_cast<const flora::LoRaReception *>(reception);
    if (loRaRadio->loRaCF != loRaReception->getLoRaCF()
            || loRaRadio->loRaBW != loRaReception->getLoRaBW()
            || loRaRadio->loRaSF != loRaReception->getLoRaSF())
        return false;
    return LoRaReceiver::computeIsReceptionPossible(listening, reception, part);
}

const IListening *SloraReceiver::createListening(
    const IRadio *radio,
    const omnetpp::simtime_t startTime,
    const omnetpp::simtime_t endTime,
    const inet::Coord &startPosition,
    const inet::Coord &endPosition) const
{
    // SLoRa has no gateway role, so the override reads the PHY parameters from
    // this receiver's own radio (the parent module) rather than the hardcoded
    // "LoRaNic" submodule lookup that stock LoRaReceiver does.
    auto loRaRadio = check_and_cast<flora::LoRaRadio *>(getParentModule());
    return new flora::LoRaBandListening(radio, startTime, endTime, startPosition,
                                        endPosition, loRaRadio->loRaCF,
                                        loRaRadio->loRaBW, loRaRadio->loRaSF);
}

}
