#include "bytemsg233.h"
#include <cassert>
#include <cstdio>

using namespace bytemsg233;

int main() {
    // Test varint roundtrip
    {
        Writer w;
        w.writeVarint(0);
        w.writeVarint(1);
        w.writeVarint(127);
        w.writeVarint(128);
        w.writeVarint(16383);
        w.writeVarint(16384);
        w.writeVarint(0xFFFFFFFFFFFFFFFF);
        auto data = w.finish();
        Reader r(data);
        assert(r.readVarint() == 0);
        assert(r.readVarint() == 1);
        assert(r.readVarint() == 127);
        assert(r.readVarint() == 128);
        assert(r.readVarint() == 16383);
        assert(r.readVarint() == 16384);
        assert(r.readVarint() == 0xFFFFFFFFFFFFFFFF);
    }

    // Test field header roundtrip
    {
        Writer w;
        w.writeHeader(1, WireType::Varint);
        w.writeVarint(42);
        w.writeHeader(2, WireType::Bytes);
        w.writeStringValue("hello");
        auto data = w.finish();
        Reader r(data);
        auto h1 = r.readHeader();
        assert(h1.tag == 1 && h1.wireType == WireType::Varint);
        assert(r.readVarint() == 42);
        auto h2 = r.readHeader();
        assert(h2.tag == 2 && h2.wireType == WireType::Bytes);
        assert(r.readString() == "hello");
    }

    // Test zigzag
    assert(zigzagEncode(0) == 0);
    assert(zigzagEncode(-1) == 1);
    assert(zigzagEncode(1) == 2);
    assert(zigzagEncode(-2) == 3);
    assert(zigzagDecode(0) == 0);
    assert(zigzagDecode(1) == -1);
    assert(zigzagDecode(2) == 1);
    assert(zigzagDecode(3) == -2);

    // Test protocol hello
    {
        ProtocolHello local{7, 6};
        auto data = appendProtocolHello({}, local);
        auto remote = readProtocolHello(data.data(), data.size());
        assert(remote.version == 7);
        assert(remote.minCompatible == 6);
        assert(checkProtocolHello(local, remote));
    }

    printf("All tests passed.\n");
    return 0;
}
