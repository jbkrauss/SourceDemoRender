#include "PrecompiledHeader.hpp"
#include "Interface\LibraryInterface.hpp"
#include <SDR Shared\Json.hpp>
#include <SDR Shared\Hooking.hpp>
#include <SDR Library API\LibraryAPI.hpp>
#include <SDR LauncherCLI API\LauncherCLIAPI.hpp>
#include "Application.hpp"
#include "ConfigSystem.hpp"
#include "Interface\Application\Extensions\ExtensionManager.hpp"

namespace
{
	struct Application
	{
		std::vector<SDR::ModuleHandlerData> ModuleHandlers;
		std::vector<SDR::StartupFuncData> StartupFunctions;
		std::vector<SDR::ShutdownFuncType> ShutdownFunctions;
	};

	Application MainApplication;

	namespace Config
	{
		void PrintModuleState(bool value, const char* name)
		{
			if (!value)
			{
				SDR::Log::Warning("SDR: No handler found for \"%s\"\n", name);
			}

			else
			{
				SDR::Log::Message("{dark}SDR: {white}Enabled module {string}\"%s\"\n", name);
			}
		}

		std::vector<SDR::ConfigSystem::ObjectData> GameConfigs;
		std::vector<SDR::ConfigSystem::ObjectData> ExtensionConfigs;

		void CallGameHandlers(SDR::ConfigSystem::ObjectData* game)
		{
			SDR::Log::Message("{dark}SDR: {white}Creating {number}%d {white}game modules\n", MainApplication.ModuleHandlers.size());

			for (auto& prop : game->Properties)
			{
				/*
					Ignore these, they are only used by the launcher.
				*/
				if (prop.first == "DisplayName")
				{
					continue;
				}

				else if (prop.first == "ExecutableName")
				{
					continue;
				}

				bool found = false;

				for (auto& handler : MainApplication.ModuleHandlers)
				{
					if (prop.first == handler.Name)
					{
						found = true;

						try
						{
							SDR::Error::ScopedContext e1(handler.Name);

							handler.Function(prop.second);
						}

						catch (const SDR::Error::Exception& error)
						{
							SDR::Error::Make("Could not enable module \"%s\"", handler.Name);
							throw;
						}

						break;
					}
				}

				PrintModuleState(found, prop.first.c_str());
			}

			MainApplication.ModuleHandlers.clear();
		}

		void CallExtensionHandlers(SDR::ConfigSystem::ObjectData* object)
		{
			if (object->Properties.empty())
			{
				return;
			}

			std::vector<std::pair<std::string, rapidjson::Value>*> temp;

			for (auto& prop : object->Properties)
			{
				if (SDR::ExtensionManager::IsNamespaceLoaded(prop.first.c_str()))
				{
					temp.emplace_back(&prop);
				}
			}

			if (temp.empty())
			{
				return;
			}

			SDR::Log::Message("{dark}SDR: {white}Creating {number}%d {white}extension modules\n", temp.size());

			for (auto& prop : temp)
			{
				auto found = SDR::ExtensionManager::Events::CallHandlers(prop->first.c_str(), prop->second);
				PrintModuleState(found, prop->first.c_str());
			}
		}

		void SetupGame()
		{
			rapidjson::Document document;

			try
			{
				document = SDR::Json::FromFile(SDR::Library::BuildResourcePath("GameConfig.json"));
			}

			catch (SDR::File::ScopedFile::ExceptionType status)
			{
				SDR::Error::Make("Could not find game config"s);
			}

			auto searcher = SDR::Library::GetGamePath();
			auto object = SDR::ConfigSystem::FindAndPopulateObject(document, searcher, GameConfigs);

			if (!object)
			{
				SDR::Error::Make("Could not find current game in game config"s);
			}

			ResolveInherit(object, GameConfigs, document.GetAllocator());
			ResolveSort(object);
			CallGameHandlers(object);

			GameConfigs.clear();
		}

		void SetupExtensions()
		{
			rapidjson::Document document;

			try
			{
				document = SDR::Json::FromFile(SDR::Library::BuildResourcePath("ExtensionConfig.json"));
			}

			catch (SDR::File::ScopedFile::ExceptionType status)
			{
				SDR::Error::Make("Could not find extension config"s);
			}

			auto searcher = SDR::Library::GetGamePath();
			auto object = FindAndPopulateObject(document, searcher, ExtensionConfigs);

			if (!object)
			{
				SDR::Error::Make("Could not find current game in extension config"s);
			}

			ResolveInherit(object, ExtensionConfigs, document.GetAllocator());
			CallExtensionHandlers(object);

			ExtensionConfigs.clear();
		}
	}

	namespace LoadLibraryIntercept
	{
		namespace Common
		{
			template <typename T>
			using TableType = std::initializer_list<std::pair<const T*, std::function<void()>>>;

			template <typename T>
			void CheckTable(const TableType<T>& table, const T* name)
			{
				for (auto& entry : table)
				{
					if (SDR::String::EndsWith(name, entry.first))
					{
						entry.second();
						break;
					}
				}
			}

			void Load(HMODULE module, const char* name)
			{
				TableType<char> table =
				{
					std::make_pair("server.dll", []()
					{
						/*
							This should be changed in the future.
						*/
						SDR::Library::Load();
					})
				};

				CheckTable(table, name);
			}

			void Load(HMODULE module, const wchar_t* name)
			{
				TableType<wchar_t> table =
				{
					
				};

				CheckTable(table, name);
			}
		}

		namespace A
		{
			SDR::Hooking::HookModule<decltype(LoadLibraryA)*> ThisHook;

			HMODULE WINAPI Override(LPCSTR name)
			{
				auto ret = ThisHook.GetOriginal()(name);

				if (ret)
				{
					Common::Load(ret, name);
				}

				return ret;
			}
		}

		namespace ExA
		{
			SDR::Hooking::HookModule<decltype(LoadLibraryExA)*> ThisHook;

			HMODULE WINAPI Override(LPCSTR name, HANDLE file, DWORD flags)
			{
				auto ret = ThisHook.GetOriginal()(name, file, flags);

				if (ret)
				{
					Common::Load(ret, name);
				}

				return ret;
			}
		}

		namespace W
		{
			SDR::Hooking::HookModule<decltype(LoadLibraryW)*> ThisHook;

			HMODULE WINAPI Override(LPCWSTR name)
			{
				auto ret = ThisHook.GetOriginal()(name);

				if (ret)
				{
					Common::Load(ret, name);
				}

				return ret;
			}
		}

		namespace ExW
		{
			SDR::Hooking::HookModule<decltype(LoadLibraryExW)*> ThisHook;

			HMODULE WINAPI Override(LPCWSTR name, HANDLE file, DWORD flags)
			{
				auto ret = ThisHook.GetOriginal()(name, file, flags);

				if (ret)
				{
					Common::Load(ret, name);
				}

				return ret;
			}
		}

		void Start()
		{
			SDR::Hooking::CreateHookAPI(L"kernel32.dll", "LoadLibraryA", A::ThisHook, A::Override);
			SDR::Hooking::CreateHookAPI(L"kernel32.dll", "LoadLibraryExA", ExA::ThisHook, ExA::Override);
			SDR::Hooking::CreateHookAPI(L"kernel32.dll", "LoadLibraryW", W::ThisHook, W::Override);
			SDR::Hooking::CreateHookAPI(L"kernel32.dll", "LoadLibraryExW", ExW::ThisHook, ExW::Override);

			SDR::Error::MH::ThrowIfFailed(MH_EnableHook(A::ThisHook.TargetFunction), "Could not enable library intercept A");
			SDR::Error::MH::ThrowIfFailed(MH_EnableHook(ExA::ThisHook.TargetFunction), "Could not enable library intercept ExA");
			SDR::Error::MH::ThrowIfFailed(MH_EnableHook(W::ThisHook.TargetFunction), "Could not enable library intercept W");
			SDR::Error::MH::ThrowIfFailed(MH_EnableHook(ExW::ThisHook.TargetFunction), "Could not enable library intercept ExW");
		}

		void End()
		{
			MH_DisableHook(A::ThisHook.TargetFunction);
			MH_DisableHook(ExA::ThisHook.TargetFunction);
			MH_DisableHook(W::ThisHook.TargetFunction);
			MH_DisableHook(ExW::ThisHook.TargetFunction);
		}
	}
}

void SDR::PreEngineSetup()
{
	SDR::Error::MH::ThrowIfFailed
	(
		SDR::Hooking::Initialize(),
		"Could not initialize hooks"
	);

	LoadLibraryIntercept::Start();
}

void SDR::Setup()
{
	LoadLibraryIntercept::End();
	
	Config::SetupGame();

	for (auto entry : MainApplication.StartupFunctions)
	{
		try
		{
			SDR::Error::ScopedContext e1(entry.Name);
			entry.Function();
		}

		catch (const SDR::Error::Exception& error)
		{
			SDR::Error::Make("Could not pass startup procedure \"%s\"", entry.Name);
			throw;
		}

		SDR::Log::Message("{dark}SDR: {white}Passed startup procedure: {string}\"%s\"\n", entry.Name);
	}

	MainApplication.StartupFunctions.clear();

	ExtensionManager::LoadExtensions();

	if (SDR::ExtensionManager::HasExtensions())
	{
		Config::SetupExtensions();
		ExtensionManager::Events::Ready();
	}
}

void SDR::Close()
{
	for (auto func : MainApplication.ShutdownFunctions)
	{
		func();
	}

	SDR::Hooking::Shutdown();
}

void SDR::AddStartupFunction(const StartupFuncData& data)
{
	MainApplication.StartupFunctions.emplace_back(data);
}

void SDR::AddShutdownFunction(ShutdownFuncType function)
{
	MainApplication.ShutdownFunctions.emplace_back(function);
}

void SDR::AddModuleHandler(const ModuleHandlerData& data)
{
	MainApplication.ModuleHandlers.emplace_back(data);
}
