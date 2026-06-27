#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <algorithm>

namespace bytemsg233 {

enum class WireType : uint8_t {
    Varint  = 0,
    Fixed64 = 1,
    Bytes   = 2,
    Fixed32 = 5,
};

enum class BlockKind : uint8_t {
    PackedVarint = 1,
    PackedZigzag = 2,
    DeltaVarint  = 3,
    BoolBitset   = 4,
    StringList   = 5,
    ColumnList   = 6,
};

inline uint64_t zigzagEncode(int64_t value) {
    return static_cast<uint64_t>((value << 1) ^ (value >> 63));
}

inline int64_t zigzagDecode(uint64_t value) {
    return static_cast<int64_t>((value >> 1) ^ -(value & 1));
}

struct FieldHeader {
    uint32_t tag;
    WireType wireType;
};

class Writer {
public:
    explicit Writer(size_t reserve = 256) { buf_.reserve(reserve); }

    const std::vector<uint8_t>& bytes() const { return buf_; }
    std::vector<uint8_t> finish() { return std::move(buf_); }
    void reset() { buf_.clear(); }

    void writeHeader(uint32_t tag, WireType wt) {
        writeVarint((static_cast<uint64_t>(tag) << 3) | static_cast<uint32_t>(wt));
    }

    void writeVarint(uint64_t value) {
        while (value >= 0x80) {
            buf_.push_back(static_cast<uint8_t>(value) | 0x80);
            value >>= 7;
        }
        buf_.push_back(static_cast<uint8_t>(value));
    }

    void writeFixed32(uint32_t value) {
        buf_.push_back(static_cast<uint8_t>(value));
        buf_.push_back(static_cast<uint8_t>(value >> 8));
        buf_.push_back(static_cast<uint8_t>(value >> 16));
        buf_.push_back(static_cast<uint8_t>(value >> 24));
    }

    void writeFixed64(uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            buf_.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    }

    void writeStringValue(const std::string& value) {
        writeVarint(value.size());
        buf_.insert(buf_.end(), value.begin(), value.end());
    }

    void writeBytes(const uint8_t* data, size_t len) {
        writeVarint(len);
        buf_.insert(buf_.end(), data, data + len);
    }

    void writeString(uint32_t tag, const std::string& value) {
        writeHeader(tag, WireType::Bytes);
        writeStringValue(value);
    }

    void writeUint32(uint32_t tag, uint32_t value) {
        writeHeader(tag, WireType::Varint);
        writeVarint(value);
    }

    void writeInt32(uint32_t tag, int32_t value) {
        writeHeader(tag, WireType::Varint);
        writeVarint(static_cast<uint64_t>(static_cast<int64_t>(value)));
    }

    void writeUint64(uint32_t tag, uint64_t value) {
        writeHeader(tag, WireType::Varint);
        writeVarint(value);
    }

    void writeInt64(uint32_t tag, int64_t value) {
        writeHeader(tag, WireType::Varint);
        writeVarint(zigzagEncode(value));
    }

    void writeFloat(uint32_t tag, float value) {
        writeHeader(tag, WireType::Fixed32);
        uint32_t bits;
        std::memcpy(&bits, &value, 4);
        writeFixed32(bits);
    }

    void writeDouble(uint32_t tag, double value) {
        writeHeader(tag, WireType::Fixed64);
        uint64_t bits;
        std::memcpy(&bits, &value, 8);
        writeFixed64(bits);
    }

    void writeBool(uint32_t tag, bool value) {
        writeHeader(tag, WireType::Varint);
        writeVarint(value ? 1 : 0);
    }

    void writeEnum(uint32_t tag, int32_t value) {
        writeHeader(tag, WireType::Varint);
        writeVarint(static_cast<uint64_t>(value));
    }

    void writeMessage(uint32_t tag, const std::vector<uint8_t>& nested) {
        writeHeader(tag, WireType::Bytes);
        writeVarint(nested.size());
        buf_.insert(buf_.end(), nested.begin(), nested.end());
    }

    template<typename T>
    void writeList(uint32_t tag, const std::vector<T>& items, std::function<void(Writer&, const T&)> writeFn) {
        writeHeader(tag, WireType::Bytes);
        Writer nested;
        nested.writeVarint(items.size());
        for (const auto& item : items) {
            writeFn(nested, item);
        }
        const auto& nb = nested.bytes();
        writeVarint(nb.size());
        buf_.insert(buf_.end(), nb.begin(), nb.end());
    }

    void writePackedVarints(uint32_t tag, const std::vector<uint64_t>& values) {
        writeHeader(tag, WireType::Bytes);
        Writer nested;
        nested.writeVarint(values.size());
        for (auto v : values) nested.writeVarint(v);
        const auto& nb = nested.bytes();
        writeVarint(nb.size());
        buf_.insert(buf_.end(), nb.begin(), nb.end());
    }

    void writeDeltaVarints(uint32_t tag, const std::vector<uint64_t>& values) {
        writeHeader(tag, WireType::Bytes);
        Writer nested;
        nested.writeVarint(values.size());
        if (!values.empty()) {
            uint64_t prev = values[0];
            nested.writeVarint(prev);
            for (size_t i = 1; i < values.size(); ++i) {
                nested.writeVarint(zigzagEncode(static_cast<int64_t>(values[i]) - static_cast<int64_t>(prev)));
                prev = values[i];
            }
        }
        const auto& nb = nested.bytes();
        writeVarint(nb.size());
        buf_.insert(buf_.end(), nb.begin(), nb.end());
    }

    void writeBoolBitset(uint32_t tag, const std::vector<bool>& values) {
        writeHeader(tag, WireType::Bytes);
        Writer nested;
        nested.writeVarint(values.size());
        uint8_t current = 0;
        for (size_t i = 0; i < values.size(); ++i) {
            if (values[i]) current |= (1 << (i & 7));
            if ((i & 7) == 7) { nested.buf_.push_back(current); current = 0; }
        }
        if (values.size() & 7) nested.buf_.push_back(current);
        const auto& nb = nested.bytes();
        writeVarint(nb.size());
        buf_.insert(buf_.end(), nb.begin(), nb.end());
    }

    void writeStringList(uint32_t tag, const std::vector<std::string>& values) {
        writeHeader(tag, WireType::Bytes);
        Writer nested;
        nested.writeVarint(values.size());
        for (const auto& v : values) nested.writeStringValue(v);
        const auto& nb = nested.bytes();
        writeVarint(nb.size());
        buf_.insert(buf_.end(), nb.begin(), nb.end());
    }

private:
    std::vector<uint8_t> buf_;
    friend class Reader;
};

class Reader {
public:
    Reader(const uint8_t* data, size_t len) : data_(data), len_(len), pos_(0) {}
    Reader(const std::vector<uint8_t>& data) : data_(data.data()), len_(data.size()), pos_(0) {}

    bool eof() const { return pos_ >= len_; }
    size_t remaining() const { return len_ - pos_; }
    void reset(const uint8_t* data, size_t len) { data_ = data; len_ = len; pos_ = 0; }

    FieldHeader readHeader() {
        uint64_t raw = readVarint();
        uint32_t tag = static_cast<uint32_t>(raw >> 3);
        WireType wt = static_cast<WireType>(raw & 0x7);
        return {tag, wt};
    }

    uint64_t readVarint() {
        uint64_t value = 0;
        for (unsigned shift = 0; shift < 64; shift += 7) {
            if (pos_ >= len_) throw std::runtime_error("bytemsg233: unexpected eof");
            uint8_t b = data_[pos_++];
            value |= static_cast<uint64_t>(b & 0x7f) << shift;
            if (b < 0x80) {
                if (shift == 63 && b > 1) throw std::runtime_error("bytemsg233: varint overflow");
                return value;
            }
        }
        throw std::runtime_error("bytemsg233: varint overflow");
    }

    int32_t readVarintInt32() { return static_cast<int32_t>(readVarint()); }
    uint32_t readVarintUint32() { return static_cast<uint32_t>(readVarint()); }
    int64_t readVarintInt64() { return zigzagDecode(readVarint()); }
    uint64_t readVarintUint64() { return readVarint(); }

    uint32_t readFixed32() {
        if (len_ - pos_ < 4) throw std::runtime_error("bytemsg233: unexpected eof");
        uint32_t v = static_cast<uint32_t>(data_[pos_])
                   | (static_cast<uint32_t>(data_[pos_ + 1]) << 8)
                   | (static_cast<uint32_t>(data_[pos_ + 2]) << 16)
                   | (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return v;
    }

    uint64_t readFixed64() {
        if (len_ - pos_ < 8) throw std::runtime_error("bytemsg233: unexpected eof");
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<uint64_t>(data_[pos_ + i]) << (i * 8);
        }
        pos_ += 8;
        return v;
    }

    float readFloat() {
        uint32_t bits = readFixed32();
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    }

    double readDouble() {
        uint64_t bits = readFixed64();
        double v;
        std::memcpy(&v, &bits, 8);
        return v;
    }

    bool readBool() { return readVarint() != 0; }
    int32_t readEnum() { return static_cast<int32_t>(readVarint()); }

    std::string readString() {
        uint64_t len = readVarint();
        if (len > len_ - pos_) throw std::runtime_error("bytemsg233: unexpected eof");
        std::string s(reinterpret_cast<const char*>(data_ + pos_), static_cast<size_t>(len));
        pos_ += static_cast<size_t>(len);
        return s;
    }

    std::vector<uint8_t> readBytes() {
        uint64_t len = readVarint();
        if (len > len_ - pos_) throw std::runtime_error("bytemsg233: unexpected eof");
        std::vector<uint8_t> v(data_ + pos_, data_ + pos_ + static_cast<size_t>(len));
        pos_ += static_cast<size_t>(len);
        return v;
    }

    std::vector<uint8_t> readMessage() { return readBytes(); }

    void skipField(WireType wt) {
        switch (wt) {
            case WireType::Varint:  readVarint(); break;
            case WireType::Fixed64: pos_ += 8; break;
            case WireType::Bytes:   { uint64_t n = readVarint(); pos_ += static_cast<size_t>(n); } break;
            case WireType::Fixed32: pos_ += 4; break;
        }
    }

    template<typename T>
    std::vector<T> readList(std::function<T(Reader&)> readFn) {
        uint64_t count = readVarint();
        uint64_t len = readVarint();
        size_t end = pos_ + static_cast<size_t>(len);
        std::vector<T> items;
        items.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) {
            items.push_back(readFn(*this));
        }
        pos_ = end;
        return items;
    }

    std::vector<uint64_t> readPackedVarints() {
        uint64_t count = readVarint();
        uint64_t len = readVarint();
        size_t end = pos_ + static_cast<size_t>(len);
        std::vector<uint64_t> v;
        v.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) v.push_back(readVarint());
        pos_ = end;
        return v;
    }

    std::vector<uint64_t> readDeltaVarints() {
        uint64_t count = readVarint();
        uint64_t len = readVarint();
        size_t end = pos_ + static_cast<size_t>(len);
        std::vector<uint64_t> v;
        v.reserve(static_cast<size_t>(count));
        if (count > 0) {
            uint64_t val = readVarint();
            v.push_back(val);
            for (uint64_t i = 1; i < count; ++i) {
                val = static_cast<uint64_t>(static_cast<int64_t>(val) + zigzagDecode(readVarint()));
                v.push_back(val);
            }
        }
        pos_ = end;
        return v;
    }

    std::vector<bool> readBoolBitset() {
        uint64_t count = readVarint();
        uint64_t len = readVarint();
        size_t end = pos_ + static_cast<size_t>(len);
        std::vector<bool> v(static_cast<size_t>(count), false);
        for (size_t i = 0; i < v.size(); i += 8) {
            uint8_t current = data_[pos_++];
            size_t limit = std::min(size_t(8), v.size() - i);
            for (size_t b = 0; b < limit; ++b) {
                v[i + b] = (current & (1 << b)) != 0;
            }
        }
        pos_ = end;
        return v;
    }

    std::vector<std::string> readStringList() {
        uint64_t count = readVarint();
        uint64_t len = readVarint();
        size_t end = pos_ + static_cast<size_t>(len);
        std::vector<std::string> v;
        v.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) v.push_back(readString());
        pos_ = end;
        return v;
    }

private:
    const uint8_t* data_;
    size_t len_;
    size_t pos_;
};

template<typename T>
class Pool {
public:
    using Factory = std::function<T()>;

    explicit Pool(Factory factory) : factory_(std::move(factory)) {}

    T acquire() {
        if (!items_.empty()) {
            T item = std::move(items_.back());
            items_.pop_back();
            return item;
        }
        return factory_();
    }

    void release(T value) {
        reset_(value);
        items_.push_back(std::move(value));
    }

private:
    Factory factory_;
    std::vector<T> items_;
    std::function<void(T&)> reset_ = [](T& v) { v.reset(); };
};

struct ProtocolHello {
    uint64_t version = 0;
    uint64_t minCompatible = 0;
};

inline std::vector<uint8_t> appendProtocolHello(const std::vector<uint8_t>& dst, ProtocolHello hello) {
    Writer w(dst.size() + 24);
    // copy existing data
    w.buf_.insert(w.buf_.end(), dst.begin(), dst.end());
    w.writeHeader(1, WireType::Varint);
    w.writeVarint(hello.version);
    w.writeHeader(2, WireType::Varint);
    w.writeVarint(hello.minCompatible);
    return w.finish();
}

inline ProtocolHello readProtocolHello(const uint8_t* data, size_t len) {
    Reader r(data, len);
    ProtocolHello hello;
    while (!r.eof()) {
        auto h = r.readHeader();
        switch (h.tag) {
            case 1: hello.version = r.readVarint(); break;
            case 2: hello.minCompatible = r.readVarint(); break;
            default: r.skipField(h.wireType); break;
        }
    }
    return hello;
}

inline bool checkProtocolHello(ProtocolHello local, ProtocolHello remote) {
    return remote.version >= local.minCompatible && local.version >= remote.minCompatible;
}

} // namespace bytemsg233
