#include "menuhook.hpp"
#include "logger.hpp"
#include "utility.hpp"

RE::UI_MESSAGE_RESULTS JournalMenu::thunk(RE::UIMessage& a_message)
{
	switch (a_message.type.get())
	{
	case RE::UI_MESSAGE_TYPE::kShow:
	
		logger::debug("Journal menu hook fired menu OPEN");
		break;

	case RE::UI_MESSAGE_TYPE::kHide:
	
		logger::debug("Journal menu hook fired menu CLOSED");

		SyncDodgeKey();

		break;
	}

	return func(this, a_message);
}

void JournalMenu::Install()
{
	func = REL::Relocation<std::uintptr_t>(RE::VTABLE_JournalMenu[0])
		.write_vfunc(idx, &JournalMenu::thunk);
	logger::info("Hooked Journal Menu");
}