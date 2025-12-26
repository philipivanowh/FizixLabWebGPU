## Setup

```sh


## Requirements

<i>Instructions are for macOS; they will need to be adapted to work on Linux and Windows.</i>

```sh
# Make sure CMake and Emscripten are installed.
brew install cmake emscripten
```

## Specific platform build

```sh
# Build the app with CMake.
cmake -B build && cmake --build build -j4

# Run the app.
./build/app
```

## Web build

```sh
# Build the app with Emscripten.
emcmake cmake -B build-web && cmake --build build-web -j4

# Run a server.
npx http-server
```

```sh
# Open the web app.
open http://127.0.0.1:8080/build-web/app.html
```