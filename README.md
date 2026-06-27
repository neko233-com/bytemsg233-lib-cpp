# bytemsg233-lib-cpp

C++ runtime for `bytemsg233` generated code.

This repository provides the encode/decode helpers, arena allocator, object pool, and enum utilities used by generated C++ message classes.

## Features

- Zero-copy arena allocator for hot-path decode
- Single-threaded object pool (rent / return)
- Scoped enum (`enum class`) helpers
- Varint, zigzag, string, bytes, list, map, nested message support
- No external dependencies — header-only or minimal source copy
- Thread-safety by design: single-threaded, no locks, no atomics, no background workers

## Install

Copy-based install from the main repository:

```bash
bytemsg233 install-lib cpp --to ./third_party/bytemsg233
```

Or add as a git submodule:

```bash
git submodule add https://github.com/neko233-com/bytemsg233-lib-cpp.git third_party/bytemsg233
```

## Quick Start

```cpp
#include "bytemsg233.h"

enum class HeroState : int32_t {
    Idle   = 0,
    Moving = 1,
    Dead   = 2,
};

struct Hero {
    uint32_t    id = 0;
    std::string name;
    HeroState   state = HeroState::Idle;
    std::vector<std::string> tags;

    void Reset() {
        id = 0;
        name.clear();
        state = HeroState::Idle;
        tags.clear();
    }
};

// Encode
ByteMsgWriter writer;
writer.WriteUintField(1, hero.id);
writer.WriteStringField(2, hero.name);
writer.WriteEnumField(3, static_cast<int32_t>(hero.state));
writer.WriteListField(4, hero.tags, [](ByteMsgWriter& w, const std::string& v) {
    w.WriteString(v);
});
auto bytes = writer.Finish();

// Decode
ByteMsgReader reader(bytes);
while (!reader.IsEof()) {
    auto header = reader.ReadFieldHeader();
    switch (header.tag) {
        case 1: hero.id = reader.ReadVarintUint32(); break;
        case 2: hero.name = reader.ReadString(); break;
        case 3: hero.state = static_cast<HeroState>(reader.ReadVarintInt32()); break;
        case 4: hero.tags = reader.ReadList<std::string>([](ByteMsgReader& r) {
            return r.ReadString();
        }); break;
        default: reader.SkipField(header.wireType); break;
    }
}
```

## API

- `ByteMsgWriter`: field header, scalar, string, bytes, list, map, nested message writing
- `ByteMsgReader`: field header, scalar reading, field skipping with bounded length checks
- `ByteMsgArena`: bump allocator for zero-GC hot-path decode
- `ByteMsgPool<T>`: rent / return object pool for generated models
- Enum helpers for scoped enum restore and validation

## Development

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest
```
