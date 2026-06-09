// Packet formats for the flood baselines.
// FloodPacket carries application data with a hop budget.
// FloodHello is the OLSR-style 1-hop neighbour advertisement used by FloodMPR.

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

cplusplus {{
using namespace inet;
}}

namespace flood;

class FloodPacket extends inet::FieldsChunk
{
    uint32_t src;
    uint32_t dst;
    uint32_t lastHop;
    uint32_t packetId;
    uint8_t  hopLimit;
    uint8_t  hopStart;
    bool     wantAck;
    uint64_t txTimeNs;
    string   payload;
}

class FloodHello extends inet::FieldsChunk
{
    uint32_t src;
    uint32_t neighbours[];
    uint32_t mprs[];
}
