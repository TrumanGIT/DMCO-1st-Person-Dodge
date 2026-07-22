#include "eventsink.hpp"
#include "SimpleDodge.h"

RE::BSEventNotifyControl eventsink::PlayerCellEvent::ProcessEvent(const RE::BGSActorCellEvent* event,
	RE::BSTEventSource<RE::BGSActorCellEvent>*) {
	if (!event || event->flags == RE::BGSActorCellEvent::CellFlag::kLeave) {
		return RE::BSEventNotifyControl::kContinue;
	}

	static bool initialized = false;

		if (initialized) return RE::BSEventNotifyControl::kContinue; 

	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player) return RE::BSEventNotifyControl::kContinue;

	PlayerAnimEventSink::GetSingleton()->Register();
	//spdlog::info("ForwardDodgeOnly: PlayerAnimEventSink registered (PostLoad/NewGame)");
	SimpleDodge::EnsureIFramePerkOnPlayer(); // get player the dodge iframe perk
	SimpleDodge::EnsureDamagePerkOnPlayer(); // get player damage mult perk

	initialized = true; 

	return RE::BSEventNotifyControl::kContinue;
}
void eventsink::PlayerCellEvent::RegisterEventSink()
{
	if (auto* player = RE::PlayerCharacter::GetSingleton()) {
		player->AsBGSActorCellEventSource()->AddEventSink(PlayerCellEvent::GetSingleton());
		logger::info("BGSActorCellEvent sink registered");
	}
}