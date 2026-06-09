package flood;

import flora.LoRa.LoRaRadio;

// Thin LoRaRadio subclass that swaps in FloodReceiver and reads its receiver address from a tag.
module FloodRadio extends LoRaRadio
{
    parameters:
        @class(FloodRadio);
        receiver.typename = default("FloodReceiver");
}
