package sim;

// Top-level network for SLoRa simulations.
// Owns a single shared LoRaMedium and an array of nodes whose count is set per-config.
network SloraMesh
{
    parameters:
        int numNodes = default(3);
        @display("bgb=600,600");
    submodules:
        LoRaMedium: SloraMedium {
            @display("p=50,50");
        }
        nodes[numNodes]: SloraNode {
            @display("p=300,300,m");
        }
}
