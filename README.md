# Integra
P2P messenger

#### Build prototype:

```bash
git submodule update --init
rm -rf build && mkdir build && cd build
cmake ../prototypes/console_client
cmake --build . --target console_client -- -j
```
