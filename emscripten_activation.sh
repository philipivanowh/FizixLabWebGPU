cd /Users/lansmachine/Documents/Tools/emsdk
./emsdk activate latest
source ./emsdk_env.sh

cd //Users/lansmachine/Documents/Github/FizixLabWebGPU
cmake --build build-web -j8
python3 -m http.server 8123 --directory build-web