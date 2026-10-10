// U2_ThaiSupport - Thai language support for the international NFS: Underground 2
// speed2.exe (v1.2 US). Pure data patching, no code hooks.
//
// Background: the international exe already maps the registry value "Thai" to
// language id 11, but its LanguageInfoTable (0x7F7020) only holds entries with
// ids 0-10, so selecting Thai makes the resource loader fail on a null entry.
// This plugin rebuilds the table with a THAILAND entry appended and repoints
// every reference to it, letting the international exe load Thai resource files
// while keeping all other languages fully usable.
//
// Patch list:
//   1. 13-entry LanguageInfoTable copy (original 11 + THAILAND + terminator)
//      kept inside this DLL.
//   2. The 12 immediate operands of &LanguageInfoTable in .text rebased.
//   3. The 7 table-walk bounds (imm32 = &CurrentLanguage used as end marker)
//      moved past the new last entry.
//   4. InitLocalization western sizing compare widened to id 11 so the
//      language memory pool is sized for the Thai font set (the largest).
//
// Every patch site is byte-verified first; on any mismatch the plugin stays
// inert, so it is safe to keep loaded with other exe builds (it disables
// itself on the SafeDisc-protected Thai release exe, for example).

#include <windows.h>
#include <cstring>

namespace {

    // Virtual addresses, ImageBase 0x400000 (module rebasing is handled).
    constexpr DWORD IMAGE_BASE          = 0x400000;
    constexpr DWORD TEXT_VA             = 0x401000;   // .text
    constexpr DWORD TEXT_SIZE           = 0x381AF1;
    constexpr DWORD LANGUAGE_INFO_TABLE = 0x7F7020;   // LanguageInfoTable[11]
    constexpr DWORD CURRENT_LANGUAGE    = 0x7F70D0;   // int CurrentLanguage (stays in place)

    // Table-walk bound compares (imm32 = &CurrentLanguage), one per walker.
    constexpr DWORD BOUND_SITES[] = {
        0x4FF65A, // language internal-name lookup
        0x4FF69E, // InitLocalization, Asian sizing walk
        0x4FF6D1, // InitLocalization, western sizing walk
        0x4FF79E, // LoadLanguageResources
        0x512395, // SetCurrentLanguage, unload walk
        0x51BE21, // FEngFont ctor, language-suffix strip
        0x52756A, // LanguageChangerCallback
    };

    // InitLocalization western sizing compare: cmp dword ptr [ecx], 0 -> 0x0B.
    constexpr DWORD INIT_SIZING_SITE    = 0x4FF6C6;
    constexpr BYTE  INIT_SIZING_OLD[3]  = { 0x83, 0x39, 0x00 };
    constexpr BYTE  INIT_SIZING_NEW[3]  = { 0x83, 0x39, 0x0B }; // match THAILAND (id 11)

    // One LanguageInfoTable entry, as used by the game (16 bytes).
    struct LangEntry
    {
        DWORD        id;
        const char*  internalName;
        const char*  binFile;
        const DWORD* fontInfo;
    };

    // Thai font set, mirrors the stock KOREAN/CHINESE layout: three null-
    // terminated arrays of bStringHash'd texture names that the game resolves
    // inside LANGUAGES\THAILAND.BIN via eLoadStreamingTexture.
    // Hashes verified against Thailand.bin of the Thai release:
    //   0x4815619D CONDUITMDITC_TT21I_THAILAND, 0xA73823FF CONDUITMDITC_TT14I_THAILAND,
    //   0xA87927BE FONT_ARIAL, 0xBBBA71C2 shared small texture,
    //   0xAB6215C1 FONT_ARIAL12, 0x0920075C FONT_IMPACT36.
    DWORD g_ThaiFontInfo[28] = {
        0x4815619D, 0xA87927BE, 0xA73823FF, 0xBBBA71C2, 0, 0, 0, 0, // primary pack
        0, 0, 0, 0, 0, 0, 0, 0,                                     // secondary pack (unused, like KOREAN)
        0xAB6215C1, 0x0920075C, 0, 0, 0, 0, 0, 0,                   // shared pack
        0, 0, 0, 0,                                                 // loaded flags, game-managed
    };

    const char g_ThaiName[] = "THAILAND";
    const char g_ThaiBin[]  = "LANGUAGES\\THAILAND.BIN";

    // Replacement table: original 11 entries + THAILAND + terminator.
    LangEntry g_NewTable[13];

    bool WriteMemory(void* dst, const void* src, size_t size)
    {
        DWORD oldProtect = 0;
        if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        memcpy(dst, src, size);
        DWORD tmp = 0;
        VirtualProtect(dst, size, oldProtect, &tmp);
        return true;
    }

    // Replaces imm32 oldImm with newImm anywhere in [site, site+range).
    bool PatchImm32Near(BYTE* site, int range, DWORD oldImm, DWORD newImm)
    {
        for (int i = 0; i + 4 <= range; ++i)
        {
            DWORD v;
            memcpy(&v, site + i, 4);
            if (v == oldImm)
                return WriteMemory(site + i, &newImm, 4);
        }
        return false;
    }

    int ScanImm32(const BYTE* data, DWORD size, DWORD value, DWORD* hits, int maxHits)
    {
        int count = 0;
        for (DWORD i = 0; i + 4 <= size; ++i)
        {
            DWORD v;
            memcpy(&v, data + i, 4);
            if (v == value && count < maxHits)
                hits[count++] = i;
        }
        return count;
    }

    bool InstallThaiSupport()
    {
        BYTE* base = reinterpret_cast<BYTE*>(GetModuleHandleA(nullptr));
        auto va = [base](DWORD x) { return base + (x - IMAGE_BASE); };
        const DWORD tableImm   = reinterpret_cast<DWORD>(base) + (LANGUAGE_INFO_TABLE - IMAGE_BASE);
        const DWORD curLangImm = reinterpret_cast<DWORD>(base) + (CURRENT_LANGUAGE - IMAGE_BASE);

        // Identity check: the intl v1.2 table reads ENGLISH..JAPANESE.
        const auto* oldTable = reinterpret_cast<const LangEntry*>(va(LANGUAGE_INFO_TABLE));
        __try
        {
            if (oldTable[0].id != 0 || strcmp(oldTable[0].internalName, "ENGLISH") != 0 ||
                strcmp(oldTable[10].internalName, "JAPANESE") != 0 ||
                strcmp(oldTable[10].binFile, "LANGUAGES\\JAPANESE.BIN") != 0)
                return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }

        memcpy(g_NewTable, oldTable, 11 * sizeof(LangEntry));
        g_NewTable[11] = { 11, g_ThaiName, g_ThaiBin, g_ThaiFontInfo };
        memset(&g_NewTable[12], 0, sizeof(LangEntry));

        const DWORD newTableVa = reinterpret_cast<DWORD>(g_NewTable);
        const DWORD newBound   = newTableVa + 12 * sizeof(LangEntry);
        BYTE* text = va(TEXT_VA);
        DWORD hits[32];

        // Already patched (references point into this DLL): nothing to do.
        if (ScanImm32(text, TEXT_SIZE, newTableVa, hits, 32) > 0)
            return true;

        // Rebase all &LanguageInfoTable immediates (6 walkers x 2 sites each).
        if (ScanImm32(text, TEXT_SIZE, tableImm, hits, 32) != 12)
            return false;
        for (int i = 0; i < 12; ++i)
        {
            if (!WriteMemory(text + hits[i], &newTableVa, 4))
                return false;
        }

        // Extend every walker's end-of-table bound past the Thai entry.
        for (DWORD site : BOUND_SITES)
        {
            if (!PatchImm32Near(va(site), 8, curLangImm, newBound))
                return false;
        }

        // Size the language pool for the Thai font set (western walk).
        if (memcmp(va(INIT_SIZING_SITE), INIT_SIZING_OLD, 3) != 0)
            return false;
        if (!WriteMemory(va(INIT_SIZING_SITE), INIT_SIZING_NEW, 3))
            return false;

        return true;
    }

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        InstallThaiSupport();
    }
    return TRUE;
}
