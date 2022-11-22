#include "utils.h"
#include "logging.h"
#include "kernel_exports.h"
#include "kernel_structures.h"

namespace Utils
{
    PVOID ModuleFromAddress(IN PEPROCESS pProcess, uintptr_t address, wchar_t* out_name)
    {
#define LDR_IMAGESIZE 0x40

        PPEB peb = PsGetProcessPeb(pProcess);

        if (!peb)
        {
            return NULL;
        }

        // Still no loader
        if (!peb->Ldr)
        {
            return NULL;
        }

        // Search in InLoadOrderModuleList
        for (PLIST_ENTRY list_entry = peb->Ldr->InLoadOrderModuleList.Flink; 
            list_entry != &peb->Ldr->InLoadOrderModuleList;
            list_entry = list_entry->Flink)
        {
            LDR_DATA_TABLE_ENTRY* mod = CONTAINING_RECORD(list_entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if ((uintptr_t)mod->DllBase <= address && address <= ((uintptr_t)mod->DllBase + *(uintptr_t*)((uintptr_t)mod + LDR_IMAGESIZE)))
            {
                wcscpy(out_name, mod->BaseDllName.Buffer);
                return mod->DllBase;
            }
        }

        return NULL;
    }

    int ForEachCore(void(*callback)(void* params), void* params)
    {
        auto core_count = KeQueryActiveProcessorCount(0);

        for (int idx = 0; idx < core_count; ++idx)
        {
            KAFFINITY affinity = Exponent(2, idx);

            KeSetSystemAffinityThread(affinity);

            callback(params);
        }

        return 0;
    }

    int Diff(uintptr_t a, uintptr_t b)
    {
        int diff = 0;

        if (a > b)
        {
            diff = a - b;
        }
        else
        {
            diff = b - a;
        }

        return diff; 
    }

    uintptr_t FindPattern(uintptr_t region_base, size_t region_size, const char* pattern, size_t pattern_size, char wildcard)
    {
        for (auto byte = (char*)region_base; byte < (char*)region_base + region_size;
            ++byte)
        {
            bool found = true;

            for (char* pattern_byte = (char*)pattern, *begin = byte; pattern_byte < pattern + pattern_size; ++pattern_byte, ++begin)
            {
                if (*pattern_byte != *begin && *pattern_byte != wildcard)
                {
                    found = false;
                }
            }

            if (found)
            {
                return (uintptr_t)byte;
            }
        }

        return 0;
    }

    bool IsInsideRange(uintptr_t address, uintptr_t range_base, uintptr_t range_size)
    {
        if ((range_base > address) &&
            ((range_base + range_size) < address))
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    KIRQL DisableWP()
    {
        KIRQL	tempirql = KeRaiseIrqlToDpcLevel();

        ULONG64  cr0 = __readcr0();

        cr0 &= 0xfffffffffffeffff;

        __writecr0(cr0);

        _disable();

        return tempirql;
    }

    void EnableWP(KIRQL	tempirql)
    {
        ULONG64	cr0 = __readcr0();

        cr0 |= 0x10000;

        _enable();

        __writecr0(cr0);

        KeLowerIrql(tempirql);
    }

    PVOID GetKernelModule(OUT PULONG pSize, UNICODE_STRING driver_name)
    {
        PLIST_ENTRY moduleList = (PLIST_ENTRY)PsLoadedModuleList;

        for (PLIST_ENTRY link = moduleList;
            link != moduleList->Blink;
            link = link->Flink)
        {
            LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (RtlCompareUnicodeString(&driver_name, &entry->BaseDllName, false) == 0)
            {
                DbgPrint("found module! %wZ at %p \n", &entry->BaseDllName, entry->DllBase);
                if (pSize && MmIsAddressValid(pSize))
                {
                    *pSize = entry->SizeOfImage;
                }

                return entry->DllBase;
            }
        }

        return 0;
    }

	HANDLE GetProcessId(const char* process_name)
    {
        auto list_entry = (LIST_ENTRY*)(((uintptr_t)PsInitialSystemProcess) + OFFSET::ProcessLinksOffset);

        auto current_entry = list_entry->Flink;

        while (current_entry != list_entry && current_entry != NULL)
        {
            auto process = (PEPROCESS)((uintptr_t)current_entry - OFFSET::ProcessLinksOffset);

            if (!strcmp(PsGetProcessImageFileName(process), process_name))
            {
                Logger::Get()->Log("found process!! PEPROCESS value %p \n", process);

                return process;
            }

            current_entry = current_entry->Flink;
        }
    }
    
    int Exponent(int base, int power)
    {
        int start = 1;
        for (int i = 0; i < power; ++i)
        {
            start *= base;
        }

        return start;
    }
}