#include "PrecompiledHeader.hpp"
#include "MaterialSystem.hpp"
#include "CreateInterface.hpp"
#include "Interface\Application\Application.hpp"

namespace
{
	namespace ModuleMaterialSystem
	{		
		void* Ptr;

		namespace Entries
		{
			SDR::Hooking::ModuleShared::Variant::Entry GetBackBufferDimensions;
		}

		namespace Variant0
		{
			using GetBackBufferDimensionsType = void(__fastcall*)(void* thisptr, void* edx, int& width, int& height);
			SDR::Hooking::ModuleShared::Variant::Function<GetBackBufferDimensionsType> GetBackBufferDimensions(Entries::GetBackBufferDimensions);
		}

		struct IWhatever
		{
			virtual void Test1(int value)
			{

			}
		};

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"MaterialsPtr",
				[](const rapidjson::Value& value)
				{
					auto module = value.FindMember("Module");
					auto version = value.FindMember("InterfaceVersion");
					auto address = SDR::Interface::CreateInterface(module->value.GetString(), version->value.GetString());

					Ptr = address;
					SDR::Error::ThrowIfNull(Ptr);

					SDR::Hooking::ModuleShared::Registry::SetKeyValue("MaterialsPtr", Ptr);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_GetBackBufferDimensions",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericVariantInit(Entries::GetBackBufferDimensions, value);
				}
			)
		);
	}
}

void* SDR::MaterialSystem::GetPtr()
{
	return ModuleMaterialSystem::Ptr;
}

void SDR::MaterialSystem::GetBackBufferDimensions(int& width, int& height)
{
	if (ModuleMaterialSystem::Entries::GetBackBufferDimensions == 0)
	{
		ModuleMaterialSystem::Variant0::GetBackBufferDimensions()(GetPtr(), nullptr, width, height);
	}
}
