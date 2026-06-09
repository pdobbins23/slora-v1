package sim;

import inet.mobility.static.StationaryMobility;
import inet.networklayer.common.InterfaceTable;

// One SLoRa node.
// Has two LoRa NICs (broadcast and data), a stationary mobility position, and the SloraApp glue module.
module SloraNode
{
    parameters:
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
        broadcastNic: SloraNic {
            @display("p=137,180");
        }
        dataNic: SloraNic {
            @display("p=137,260");
        }
        app: SloraApp {
            @display("p=375,76");
        }
    connections allowunconnected:
        app.broadcastOut --> broadcastNic.upperLayerIn;
        broadcastNic.upperLayerOut --> app.broadcastIn;
        app.dataOut --> dataNic.upperLayerIn;
        dataNic.upperLayerOut --> app.dataIn;
}
