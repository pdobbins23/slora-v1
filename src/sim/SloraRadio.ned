package sim;

import flora.LoRa.LoRaRadio;

// Thin LoRaRadio subclass that swaps in SloraReceiver and lets the upper layer
// override per-packet center frequency and bandwidth via LoRaTag.
module SloraRadio extends LoRaRadio
{
    parameters:
        @class(SloraRadio);
        receiver.typename = default("SloraReceiver");
}
