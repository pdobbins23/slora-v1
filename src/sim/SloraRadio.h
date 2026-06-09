#pragma once

#include "LoRa/LoRaRadio.h"

namespace slora {

// LoRaRadio subclass that handles two SLoRa needs.
// Stock LoRaRadio peeks a LoRaMacFrame at the front of every outgoing packet
// to copy the receiver address into the PHY preamble. SLoRa has no MAC
// layer, so SloraRadio reads the receiver address from a packet tag instead.
// SloraRadio also lets the upper layer override the per-packet center
// frequency, bandwidth, and SF through LoRaTag. This is how FDMA retune-on-TX
// is modelled. The LoRaTag's center frequency can differ from the radio's
// own listening frequency, and the radio uses the tagged value when it
// transmits.
class SloraRadio : public flora::LoRaRadio
{
  protected:
    void handleUpperPacket(inet::Packet *packet) override;
};

}
