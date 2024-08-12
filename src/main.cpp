#include "CallbackEvent.h"

RE::FormID GetFormID(const std::string& a_str)
{
	if (const auto splitID = string::split(a_str, "~"); splitID.size() == 2) {
		const auto  formID = string::to_num<RE::FormID>(splitID[0], true);
		const auto& modName = splitID[1];
		return RE::TESDataHandler::GetSingleton()->LookupFormID(formID, modName);
	}
	if (string::is_only_hex(a_str, true)) {
		const auto formID = string::to_num<RE::FormID>(a_str, true);
		if (const auto form = RE::TESForm::LookupByID(formID); !form) {
			logger::error("\t\tFilter [{}] INFO - unable to find form, treating filter as cell formID", a_str);
		}
		return formID;
	}
	if (const auto form = RE::TESForm::LookupByEditorID(a_str)) {
		return form->GetFormID();
	}
	return static_cast<RE::FormID>(0);
}

void LoadHook() {
	RE::BGSListForm* SunDamageExcep = (RE::BGSListForm*)RE::TESForm::LookupByEditorID("SunDamageExceptionWorldSpaces");
	if (!SunDamageExcep) {
		logger::error("Could not file SunDamageExceptionWorldSpaces form, aborting...");
		return;
	}

	std::vector<std::string> configs = distribution::get_configs(R"(Data\)", "_SDE"sv, ".ini"sv);
	if (configs.empty()) {
		logger::warn("No .ini files with _SDE suffix were found within the Data folder, aborting...");
	}

	logger::info("{} matching inis found", configs.size());
	std::ranges::sort(configs);

	for (auto& path : configs) {
		logger::info("\tINI : {}", path);
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();
		ini.SetAllowKeyOnly();

		if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
			logger::error("\t\tcouldn't read INI");
			continue;
		}
		CSimpleIniA::TNamesDepend sections;
		ini.GetAllSections(sections);
		sections.sort(CSimpleIniA::Entry::LoadOrder());
		for (auto& [_section, comment, keyOrder] : sections) {
			std::string section = _section;
			CSimpleIniA::TNamesDepend values;
			ini.GetAllKeys(section.c_str(), values);

			if (!values.empty()) {
				for (auto& key : values) {
					//Get and Check FormID
					RE::FormID baseformid = GetFormID(key.pItem);
					if (baseformid == 0) {
						logger::warn("\t{} is not a valid formID, skipping...", key.pItem);
						continue;
					}

					//Get Form and check is it's a valid world space
					RE::TESForm* baseform = RE::TESForm::LookupByID(baseformid);
					if (baseform->formType != RE::FormType::WorldSpace) {
						logger::warn("\t{} is not a World Space, skipping...", key.pItem);
						continue;
					}

					//Check if world space is already in list
					if (SunDamageExcep->HasForm(baseformid) || SunDamageExcep->HasForm(baseform)) {
						logger::info("\t{} is already present in formID list, skipping...", key.pItem);
						continue;
					}

					SunDamageExcep->AddForm(baseform);
					logger::info("\tAdded {} to world space exceptions", key.pItem);
				}
			}
		}
	}
}

void OnInit(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
		case SKSE::MessagingInterface::kInputLoaded:
		{
			logger::info("{:*^30}", "INPUT LOADED");
		}
		break;
	default:
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("SunlightDamageException");
	v.AuthorName("Moonling");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info) 
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "SunlightDamageException";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		SKSE::log::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		SKSE::log::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif  // SKYRIM_AE

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	SKSE::Init(a_skse);

	//const auto messaging = SKSE::GetMessagingInterface();
	//messaging->RegisterListener("SKSE", OnInit);

	/*
	On<RE::TESActivateEvent>([](const RE::TESActivateEvent* event) {
		auto activated = event->objectActivated->GetBaseObject()->GetName();
		auto activator = event->actionRef->GetBaseObject()->GetName();
		logger::info("{} activated {}", activator, activated);
	});
	*/

	SKSE::GetMessagingInterface()->RegisterListener(OnInit);

	On<RE::TESLoadGameEvent>([](const RE::TESLoadGameEvent* event) {
		LoadHook();
	});

	return true;
}
