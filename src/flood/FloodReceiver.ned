package flood;

import flora.LoRaPhy.LoRaReceiver;

// LoRaReceiver subclass that drops the MAC-collision lookup since flood has no MAC.
module FloodReceiver extends LoRaReceiver
{
    parameters:
        @class(FloodReceiver);
}
