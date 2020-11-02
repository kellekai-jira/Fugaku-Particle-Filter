#include "TimingEvent.h"
void Timing::trigger_event(TimingEventType type, const int parameter)
{
    events.push_back(TimingEvent(type, parameter));
}
