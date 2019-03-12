#include "PrecompiledHeader.hpp"
#include "CreateInterface.hpp"

namespace
{
	using FuncType = void*(__cdecl*)(const char* name, int* code);
}

namespace SDR::Interface
{
	void* CreateInterface(const char* module, const char* version)
	{
		auto handle = GetModuleHandleA(module);
		auto proc = (FuncType)GetProcAddress(handle, "CreateInterface");

		int code = 0;
		auto address = proc(version, &code);
		
		return address;
	}
}