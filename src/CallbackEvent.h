#pragma once

#include <RE/Skyrim.h>

template <class Event>
class CallbackEventSink : public RE::BSTEventSink<Event>
{
	std::function<void(const Event*)> _callback;

public:
	CallbackEventSink(std::function<void(const Event*)> callback) :
		_callback(callback) {}
	RE::BSEventNotifyControl ProcessEvent(const Event* event, RE::BSTEventSource<Event>*) override
	{
		_callback(event);
		return RE::BSEventNotifyControl::kContinue;
	}
};

template <typename EventType>
void On(std::function<void(const EventType*)> callback)
{
	RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<EventType>(
		new CallbackEventSink<EventType>(callback));
}
