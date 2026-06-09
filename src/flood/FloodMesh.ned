package flood;

import flora.LoRaPhy.LoRaMedium;

// Top-level network for the flood baselines.
// Uses stock LoRaMedium since flood nodes all listen on the same channel.
network FloodMesh
{
    parameters:
        int numNodes = default(5);
        @display("bgb=600,600");
    submodules:
        LoRaMedium: LoRaMedium {
            @display("p=50,50");
        }
        nodes[numNodes]: FloodNode {
            @display("p=300,300,m");
        }
}
