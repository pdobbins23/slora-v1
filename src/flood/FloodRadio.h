#pragma once

#include "LoRa/LoRaRadio.h"

namespace flood {

// LoRaRadio subclass that drops the MAC dependency.
// Stock LoRaRadio peeks a LoRaMacFrame at the front of every outgoing packet to
// copy the receiver address into the PHY preamble. The flood baselines have no
// MAC, so this radio reads the receiver address from a packet tag instead.
class FloodRadio : public flora::LoRaRadio
{
  protected:
    void handleUpperPacket(inet::Packet *packet) override;
};

}
