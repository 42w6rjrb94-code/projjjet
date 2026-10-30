cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release



cmake -B build -DCMAKE_BUILD_TYPE=Release


cmake --build build --config Release




char sm[256]{};
WideCharToMultiByte(CP_ACP, 0, me.szModule, -1, sm, 255, nullptr, nullptr);
if (!_stricmp(sm, modName)) {
