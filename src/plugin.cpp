
#include "plugin.hpp"
#include "logger.hpp"
#include "utility.hpp"
#include "menuhook.hpp"
#include "SimpleDodge.h"

#include "hook.h"
#include "eventsink.hpp"

static void MessageHandler(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case SKSE::MessagingInterface::kPostLoad:
    {
        break;
    }
    case SKSE::MessagingInterface::kSaveGame: 
    {
		break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
    {
        break;
    }
    case SKSE::MessagingInterface::kPostLoadGame:
    {
        break;
    }
    case SKSE::MessagingInterface::kNewGame:
    {
        break;
    }  case SKSE::MessagingInterface::kDataLoaded:
    {
        SyncDodgeKey();
        SimpleDodge::InitGlobals();
        eventsink::DodgeInputSink::GetSingleton()->Register();
        eventsink::PlayerCellEvent::RegisterEventSink(); 
        break;
    }
    default:
        break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    setupLog(spdlog::level::info);
    logger::info("sync 1st and 3rd person dodge key plugin is Loaded");
    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
    JournalMenu::Install();
    Hooks::Install();

    //INI
    SimpleDodge::LoadSettings();
    return true;
}

