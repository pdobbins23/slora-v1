package sim;

import flora.LoRaPhy.LoRaMedium;

// Cosmetic subclass of flora.LoRaMedium.
// Overrides sendToRadio in C++ to tag each delivered signal as broadcast or
// unicast based on the LoRa preamble bandwidth, which colours the
// per-receiver delivery animation in Qtenv. See SloraMedium.cc for the why.
module SloraMedium extends LoRaMedium
{
    parameters:
        @class(SloraMedium);
}
