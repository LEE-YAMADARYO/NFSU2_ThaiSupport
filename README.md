# U2 Thai Support
*I am not a native Thai speaker, so I apologize for not providing a Thai README.*<br>
*If you are a native Thai speaker, or know someone else who needs this, please recommend it to them.*<br><br>
Adds Thai language support to the international speed2.exe of NFS Underground 2 (v1.2 US build). Implemented as an ASI plugin.<br>

The international exe already support "Thai" as a language value, but ships without the internal table entry that loads Thai resources — selecting Thai would crash the game. This plugin restores that entry at runtime, so the international exe can load Thai resource files while keeping every other language fully selectable. No language is replaced.<br>

Thai resource files are NOT included — you must supply them yourself.<br><br>

## Install
1. Install an ASI loader (e.g. Ultimate ASI Loader's dinput8.dll) and drop `U2_ThaiSupport.asi` into the game `Scripts` folder.<br>
2. Prepare the Thai resource files from the Thai release of the game: `LANGUAGES\Thailand.bin` — Thai string table, including the Thai font textures — They are usually located in the `LANGUAGES`, `GLOBAL`, or `FRONTEND` folders.<br>
3. Modify the registry or use the latest Widescreen Fix to set the Thai language.<br><br>

## Important Notes:
Optional but recommended:<br>
If you are using the Western version of the game build and wish to play in Thai (or another Asian language), please replace the corresponding files in the Western version with the following files from the Asian version's `GLOBAL\` folder:<br>
`CarHeadersMemoryFile.bin`<br>
`FrontEndMemoryFile.bin`<br>
`GlobalMemoryFile.bin`<br>
`InGameMemoryFile.bin`<br><br>

## Recommended pairings:
You can use this plugin alongside `KyoriAsh`'s Multi 13 languages selector mod.<br>
https://nfsmods.xyz/mod/5749<br><br>
This mod includes all Asian language assets(Includes Thai language resources) and memory files.<br><br>

## Known issues:
None found so far, if any exist, please submit an issue.<br><br>