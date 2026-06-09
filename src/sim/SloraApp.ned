package sim;

// Per-node application that wraps the Zig SLoRa stack.
// Owns one libslora Node, drives the broadcast and data NICs, and pumps Zig-emitted Actions into OMNeT++ events.
simple SloraApp
{
    parameters:
        double broadcastFreqHz @unit(Hz) = default(902.3MHz);
        double broadcastBwHz   @unit(Hz) = default(500kHz);
        int    broadcastSF              = default(12);
        double broadcastTxPowerDbm @unit(dBm) = default(14dBm);
        int    broadcastCR              = default(5);

        int    dataSF                    = default(7);
        double dataTxPowerDbm  @unit(dBm) = default(14dBm);
        int    dataCR                    = default(5);

        bool   trafficEnabled            = default(false);
        double trafficInterval @unit(s)  = default(5s);
        int    trafficPayloadBytes       = default(4);
        int    trafficDestOffset         = default(1);
        bool   trafficRandomDest         = default(false);
        string trafficMode               = default("echo");
        string targetPeerOffsets         = default("");

        double announceInterval @unit(s) = default(5s);
        double announceJitter   @unit(s) = default(2s);

        int    broadcastCwMin            = default(2);
        int    broadcastCwMax            = default(8);
        double broadcastSlotTime @unit(s) = default(100ms);

        @class(SloraApp);
        @display("i=block/app");
    gates:
        input  broadcastIn;
        output broadcastOut;
        input  dataIn;
        output dataOut;
}
