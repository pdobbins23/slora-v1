package flood;

import inet.mobility.static.StationaryMobility;
import inet.networklayer.common.InterfaceTable;
import inet.applications.contract.IApp;

// One flood-baseline node.
// Has one LoRa NIC (no separate broadcast/data split since flood has only one radio role).
module FloodNode
{
    parameters:
        int numApps = default(0);
        @networkNode();
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        @display("bgb=297,313;i=device/accesspoint;is=vs");
    submodules:
        interfaceTable: InterfaceTable {
            @display("p=30,26");
        }
        mobility: StationaryMobility {
            @display("p=24,88");
        }
        // Submodule name kept as `LoRaNic` because LoRaReceiver::createListening
        // looks for that name on the containing node when reading PHY params.
        LoRaNic: FloodNic {
            @display("p=137,239");
        }
        app[numApps]: <> like IApp {
            @display("p=375,76,row,150");
        }
    connections allowunconnected:
        for i=0..numApps-1 {
            app[i].socketOut --> LoRaNic.upperLayerIn;
            LoRaNic.upperLayerOut --> app[i].socketIn;
        }
}
