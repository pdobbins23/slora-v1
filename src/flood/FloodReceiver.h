#pragma once

#include "LoRaPhy/LoRaReceiver.h"

namespace flood {

// LoRaReceiver subclass that drops the MAC dependency.
// Stock LoRaReceiver runs check_and_cast<LoRaMac>(parent->parent->mac) on
// every collision to count "collisions addressed to me". The flood baselines
// have no MAC, so this override skips that lookup.
class FloodReceiver : public flora::LoRaReceiver
{
  public:
    bool computeIsReceptionAttempted(
        const inet::physicallayer::IListening *listening,
        const inet::physicallayer::IReception *reception,
        inet::physicallayer::IRadioSignal::SignalPart part,
        const inet::physicallayer::IInterference *interference) const override;
};

}
