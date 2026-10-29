cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release



cmake -B build -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"


cmake --build build --config Release