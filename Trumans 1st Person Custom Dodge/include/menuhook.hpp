#pragma once

struct JournalMenu : RE::JournalMenu

{
    RE::UI_MESSAGE_RESULTS thunk(RE::UIMessage& a_message);

    using ProcessMessage_t = decltype(&RE::JournalMenu::ProcessMessage);
    static inline REL::Relocation<ProcessMessage_t> func;

    static constexpr std::size_t idx{ 0x4 };

    static void Install();
};