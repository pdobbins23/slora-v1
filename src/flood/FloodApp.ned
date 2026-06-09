package flood;

import inet.applications.contract.IApp;

simple FloodApp like IApp
{
    parameters:
        @class(FloodApp);

        bool isSource = default(false);
        double sendInterval @unit(s) = default(30s);
        int payloadSizeBytes @unit(B) = default(32B);
        int trafficDestOffset = default(1);
        bool trafficRandomDest = default(false);

        int initialHopLimit = default(3);

        // Flood variant. One of:
        //   "naive": every node rebroadcasts every packet once within the
        //            hop limit. No SNR weighting and no suppression. Uses
        //            uniform random jitter in [0, slotTime] to dodge
        //            synchronous collisions.
        //   "snr":   Meshtastic-style. SNR-weighted contention window plus
        //            suppression when the same packet is heard from someone
        //            else during backoff.
        //   "mpr":   OLSR-style. Only nodes elected as MPR by the sender
        //            rebroadcast. Requires periodic HELLO exchange of 1-hop
        //            neighbour lists.
        string mode = default("snr");

        int cwMin = default(2);
        int cwMax = default(8);
        double slotTime @unit(s) = default(100ms);
        double snrGoodDb = default(-5);
        double snrBadDb = default(-15);

        double helloInterval @unit(s) = default(5s);
        double helloJitter @unit(s) = default(1s);
        double neighbourTimeout @unit(s) = default(30s);

        double initialLoRaTP @unit(dBm) = default(22dBm);
        double initialLoRaCF @unit(Hz) = default(868MHz);
        int initialLoRaSF = default(11);
        double initialLoRaBW @unit(Hz) = default(250kHz);
        int initialLoRaCR = default(5);
        bool initialUseHeader = default(true);

        @display("i=block/app");
        @signal[pktsOriginated](type=long);
        @signal[pktsForwarded](type=long);
        @signal[pktsSuppressed](type=long);
        @signal[pktsDelivered](type=long);
        @signal[helloSent](type=long);
        @signal[helloRcvd](type=long);
        @signal[pktsMprDrop](type=long);
        @statistic[pktsOriginated](source=pktsOriginated; record=count,sum);
        @statistic[pktsForwarded](source=pktsForwarded; record=count,sum);
        @statistic[pktsSuppressed](source=pktsSuppressed; record=count,sum);
        @statistic[pktsDelivered](source=pktsDelivered; record=count,sum);
        @statistic[helloSent](source=helloSent; record=count,sum);
        @statistic[helloRcvd](source=helloRcvd; record=count,sum);
        @statistic[pktsMprDrop](source=pktsMprDrop; record=count,sum);
    gates:
        input socketIn;
        output socketOut;
}
