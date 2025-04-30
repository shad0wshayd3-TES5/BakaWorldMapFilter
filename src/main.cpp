#include <nlohmann/json.hpp>

namespace REX::JSON
{
	using vec_t = std::vector<std::string>;

	namespace Impl
	{
		template <>
		void SettingLoad<vec_t>(
			void* a_data,
			path_t a_path,
			vec_t& a_value,
			vec_t& a_valueDefault)
		{
			const auto& json = *static_cast<nlohmann::json*>(a_data);
			if (a_path[0] == '/') {
				a_value = json.value<vec_t>(nlohmann::json::json_pointer(a_path.data()), a_valueDefault);
			}
			else {
				a_value = json.value<vec_t>(a_path, a_valueDefault);
			}
		}

		template <>
		void SettingSave<vec_t>(void*, path_t, vec_t&) { return; }
	}

	template <class Store = SettingStore>
	using StrA = Setting<vec_t, Store>;
}

namespace Config
{
	static REX::JSON::Bool Debug{ "debug", false };
	static REX::JSON::StrA Blocklist{ "blocklist", {} };

	static void Init()
	{
		const auto json = REX::JSON::SettingStore::GetSingleton();
		json->Init(
			"Data/SKSE/plugins/BakaWorldMapFilter.json",
			"Data/SKSE/plugins/BakaWorldMapFilterCustom.json");
		json->Load();

		SKSE::log::info("Loaded blocklist: {}", Blocklist.GetValue());
	}
}

namespace Hooks
{
	static bool ListHasWorldSpace(const std::string a_name)
	{
		const auto& vec = Config::Blocklist.GetValue();
		const auto it = std::find_if(vec.begin(), vec.end(),
			[&a_name](const std::string& a_iter) {
				if (a_name.size() != a_iter.size()) {
					return false;
				}
				return std::equal(a_name.cbegin(), a_name.cend(), a_iter.cbegin(), a_iter.cend(),
					[](auto a_name, auto a_iter) {
						return std::toupper(a_name) == std::toupper(a_iter);
					});
			});
		return (it != vec.end());
	}

	static REL::Relocation<RE::UI_MESSAGE_RESULTS(*)(RE::MapMenu*, RE::UIMessage&)> _ProcessMessage;
	static RE::UI_MESSAGE_RESULTS ProcessMessage(RE::MapMenu* a_this, RE::UIMessage& a_message)
	{
		if (a_message.type.all(RE::UI_MESSAGE_TYPE::kShow)) {
			if (a_this && a_this->worldSpace) {
				if (ListHasWorldSpace(a_this->worldSpace->editorID.c_str())) {
					if (auto UI = RE::UI::GetSingleton(); UI && UI->IsMenuOpen("TweenMenu"sv)) {
						auto UIMessageQueue = RE::UIMessageQueue::GetSingleton();
						if (UIMessageQueue) {
							UIMessageQueue->AddMessage("TweenMenu"sv, RE::UI_MESSAGE_TYPE::kShow, nullptr);
						}
					}
					return RE::UI_MESSAGE_RESULTS::kIgnore;
				}
				else if (Config::Debug.GetValue())
				{
					SKSE::log::info("debug: map opened for worldspace: {} [{:08X}]", a_this->worldSpace->editorID.c_str(), a_this->worldSpace->formID);
				}
			}
		}
		return _ProcessMessage(a_this, a_message);
	}

	static void Install()
	{
		static REL::Relocation target{ RE::VTABLE_MapMenu[0] };
		_ProcessMessage = target.write_vfunc(0x04, ProcessMessage);
	}
}

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
			Config::Init();
			Hooks::Install();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
	return true;
}
