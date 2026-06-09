package flood;

import inet.linklayer.contract.IWirelessInterface;
import inet.networklayer.common.NetworkInterface;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadio;

// Wireless interface with no MAC. The app talks straight to FloodRadio.
module FloodNic extends NetworkInterface like IWirelessInterface
{
    parameters:
        string interfaceTableModule;
        string energySourceModule = default("");
        *.interfaceTableModule = default(absPath(this.interfaceTableModule));
        *.energySourceModule = default(absPath(this.energySourceModule));

        radio.antenna.mobilityModule = "^.^.^.mobility";
        radio.transmitter.headerLength = 0B;
    gates:
        input upperLayerIn;
        output upperLayerOut;
        input radioIn @labels(IWirelessSignal);
    submodules:
        radio: <default("flood.FloodRadio")> like IRadio {
            @display("p=200,200");
        }
    connections:
        upperLayerIn --> { @display("m=n"); } --> radio.upperLayerIn;
        radio.upperLayerOut --> { @display("m=n"); } --> upperLayerOut;
        radioIn --> { @display("m=s"); } --> radio.radioIn;
}
