#pragma once

#include "LoRaPhy/LoRaReceiver.h"

namespace slora {

// LoRaReceiver subclass with two overrides.
//
// computeIsReceptionAttempted drops the check_and_cast<LoRaMac> lookup that
// stock LoRaReceiver does on every collision. SLoRa has no MAC, so that
// lookup would fail. FloodReceiver does the same thing.
//
// createListening reads the listening band from this receiver's own radio
// instead of the hardcoded "LoRaNic" submodule. Stock LoRaReceiver looks up
//   node->getSubmodule("LoRaNic")->getSubmodule("radio")
// to find the radio whose loRaCF, loRaBW, and loRaSF set the listening band.
// SLoRa nodes have two radios (broadcastNic on the broadcast frequency,
// dataNic on the node's home frequency), and the hardcoded lookup would
// resolve both receivers to the same fixed submodule. Reading from the
// parent module instead gives each receiver its own listening band.
class SloraReceiver : public flora::LoRaReceiver
{
  protected:
    bool computeIsReceptionAttempted(
        const inet::physicallayer::IListening *listening,
        const inet::physicallayer::IReception *reception,
        inet::physicallayer::IRadioSignal::SignalPart part,
        const inet::physicallayer::IInterference *interference) const override;

    bool computeIsReceptionPossible(
        const inet::physicallayer::IListening *listening,
        const inet::physicallayer::IReception *reception,
        inet::physicallayer::IRadioSignal::SignalPart part) const override;

    const inet::physicallayer::IListening *createListening(
        const inet::physicallayer::IRadio *radio,
        const omnetpp::simtime_t startTime,
        const omnetpp::simtime_t endTime,
        const inet::Coord &startPosition,
        const inet::Coord &endPosition) const override;
};

}
