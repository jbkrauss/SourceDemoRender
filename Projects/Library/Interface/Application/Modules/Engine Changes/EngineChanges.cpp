#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

namespace
{
	bool IsUnfocused = false;
}

namespace
{
	namespace ModuleEngineInfo
	{
		/*
			The offset of m_bActiveApp in CGame.
		*/
		int ActiveAppOffset;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"Engine_Info",
				[](const rapidjson::Value& value)
				{
					ActiveAppOffset = SDR::Json::GetInt(value, "ActiveAppOffset");
				}
			)
		);
	}

	namespace ModuleActivateMouse
	{
		/*
			This is needed because it's responsible for locking the mouse inside the window.
		*/

		namespace Variant0
		{
			void __cdecl NewFunction();

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction()
			{
				if (!IsUnfocused)
				{
					ThisHook.GetOriginal()();
				}
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"ActivateMouse",
				[](const rapidjson::Value& value)
				{
					SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleAppActivate
	{
		namespace Variant0
		{
			/*
				Structure from Source 2007.
			*/
			struct InputEvent
			{
				int Type;
				int Tick;
				int Data;
				int Data2;
				int Data3;
			};

			void __fastcall NewFunction(void* thisptr, void* edx, const InputEvent& event);

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

			void __fastcall NewFunction(void* thisptr, void* edx, const InputEvent& event)
			{
				IsUnfocused = event.Data == 0;

				auto& isactiveapp = *(bool*)((char*)(thisptr) + ModuleEngineInfo::ActiveAppOffset);

				ThisHook.GetOriginal()(thisptr, edx, event);

				/*
					Deep in the engine somewhere in CEngine::Frame, the logical
					FPS is lowered when the window is unfocused to save performance.
					That also makes the processing slower if you are alt tabbed.
				*/
				if (IsUnfocused)
				{
					isactiveapp = true;
				}
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"AppActivate",
				[](const rapidjson::Value& value)
				{
					SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}
}