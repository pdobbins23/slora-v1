// Cosmetic subclass of flora::LoRaMedium.
// The override below tags each delivered signal as broadcast or unicast based
// on the LoRa preamble bandwidth (500 kHz means the broadcast radio, anything
// narrower means the data radio). That tag is what colours the per-receiver
// delivery animation in Qtenv. The FDMA filtering itself lives in
// SloraReceiver, not here.

#pragma once

#include "LoRaPhy/LoRaMedium.h"

namespace slora {

class SloraMedium : public flora::LoRaMedium
{
  protected:
    void sendToRadio(inet::physicallayer::IRadio *transmitter,
                     const inet::physicallayer::IRadio *receiver,
                     const inet::physicallayer::IWirelessSignal *transmittedSignal) override;
};

}
