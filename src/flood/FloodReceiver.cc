// Implementation of FloodReceiver.
// See FloodReceiver.h for the class-level overview.

#include "FloodReceiver.h"

namespace flood {

using namespace inet::physicallayer;

Define_Module(FloodReceiver);

bool FloodReceiver::computeIsReceptionAttempted(
    const IListening *listening,
    const IReception *reception,
    IRadioSignal::SignalPart part,
    const IInterference *interference) const
{
    return !isPacketCollided(reception, part, interference);
}

}
