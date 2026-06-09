package sim;

import flora.LoRaPhy.LoRaReceiver;

// LoRaReceiver subclass that filters reception by this radio's actual listening band.
// Stock LoRaReceiver assumes all nodes listen on the same channel, which breaks SLoRa's FDMA.
module SloraReceiver extends LoRaReceiver
{
    parameters:
        @class(SloraReceiver);
}
