#ifndef CK_BIO_HPP
#define CK_BIO_HPP

#include <cstdint>
#include <iostream>
#include <vector>

namespace ck
{

using pos_t = std::streamoff;

namespace bio
{

enum Mode
{
    Buffer,
    Stream
};

using buffer_t = std::vector<uint8_t>;

/////////////////////////////////////////////////////////////////////
/// reader
struct ireader{
    virtual size_t read(uint8_t* buf,size_t size) = 0;
    virtual size_t pos() const = 0;
    virtual size_t seek(pos_t pos) = 0;
    virtual size_t offset(pos_t ofs) = 0;
};

template<Mode MO>
struct basic_reader
{};

template<>
struct basic_reader<Buffer> : ireader {
    basic_reader(const basic_reader&) = delete;
    inline basic_reader(basic_reader&& o) :
        d(o.d), _cur(o._cur),_is_buffer(o._is_buffer)
    {
        o.d.p.data = nullptr;
        o.d.size = 0;
        o._cur = 0;
        o._is_buffer = false;
    }

    inline basic_reader(const uint8_t* data,size_t size) {
        d.p.data = data;
        d.size = size;
    }
    inline basic_reader(const buffer_t* buf)
        : _is_buffer(true)
    {
        d.p.buf = buf;
    }

    // @return: the number of bytes have been read, <= size; if equal 0, means eob
    inline size_t read(uint8_t* buf,size_t size) override {
        if(!d.p.data || !buf || size < 1)
            return 0;
        const auto remain = end() - _cur;
        if(remain < 1)
            return 0;
        if(remain < size)
            size = remain;
        memcpy(buf,ptr() + _cur,size);
        _cur += size;
        return size;
    }

    inline size_t pos() const override {
        return _cur;
    }

    // pos < 0: to the end; others: to the pos
    // @return: position after seek
    inline size_t seek(pos_t pos) override {
        if(!d.p.data) return 0;
        _cur = end();
        if(pos >= 0 && (size_t)pos < end())
            _cur = (size_t)pos;
        return _cur;
    }

    // offset from current pos
    // @return: position after offset
    inline size_t offset(pos_t ofs) override {
        if(!d.p.data) return 0;
        auto cur = (pos_t)_cur + ofs;
        if(cur < 0) _cur = 0;
        else if((size_t)cur > end()) _cur = end();
        return _cur;
    }
private:
    inline const uint8_t* ptr() const {
        return _is_buffer ? d.p.buf->data() : d.p.data;
    }
    inline const size_t end() const{
        return _is_buffer ? d.p.buf->size() : d.size;
    }

    struct {
        union {
            const uint8_t* data;    // source data pointer
            const buffer_t* buf;
        } p;
        size_t size = 0;         // source data size
    } d;

    size_t _cur = 0;          // curren position
    bool _is_buffer = false;
};

template<>
struct basic_reader<Stream> : ireader {
    basic_reader(const basic_reader&) = delete;
    inline basic_reader(basic_reader&& o) :
        _si(o._si),_beg(o._beg),_remain(o._remain)
    {}
    inline basic_reader(std::istream& si)
        : _si(si),_beg((size_t)si.tellg())
    {
        si.seekg(0,std::ios::end);
        _remain = basic_reader<Stream>::pos();
        si.seekg(_beg,std::ios::beg);
    }

    // fill the "size" as much as possible
    inline size_t read(uint8_t* buf,size_t size) override {
        if(_si.eof() || _si.fail() || _remain < 1)
            return 0;
        if(_remain < size)
            size = _remain;

        _si.read((char*)buf,size);
        auto sz_read = (size_t)_si.gcount();
        _remain -= sz_read;
        return sz_read;
    }

    inline size_t pos() const override {
        return (size_t)_si.tellg() - _beg;
    }

    inline size_t seek(pos_t pos) override {
        _si.seekg(0,std::ios::end);
        const auto size = _si.tellg();
        if(pos >= 0)
        {
            pos += _beg;
            if(pos > size)
                _remain = 0;
            else {
                _si.seekg(pos, std::ios::beg);
                _remain = size_t(size - pos);
            }
        }
        return this->pos();
    }

    inline size_t offset(pos_t ofs) override {
        _si.seekg(ofs,std::ios::cur);
        return this->pos();
    }
private:
    std::istream& _si;
    const size_t _beg;  // the position of begin
    size_t _remain;
};

template<Mode MO>
struct reader : public basic_reader<MO> {
    using super = basic_reader<MO>;
    using super::basic_reader;

    template<typename T>
    inline size_t read(T* out, size_t capacity) {
        return super::read((uint8_t*)out,capacity);
    }

    template<typename T>
    size_t read(T* out) {
        return read(out,sizeof(T));
    }
};

inline reader<Buffer> make_reader(const uint8_t* in,size_t in_size)
{ return { in,in_size }; }

inline reader<Buffer> make_reader(const buffer_t* in)
{ return { in }; }

inline reader<Stream> make_reader(std::istream& si)
{ return { si }; }

/////////////////////////////////////////////////////////////////////
/// writer
struct iwriter{
    virtual void reserve(size_t capacity) {};
    virtual size_t write(const uint8_t* buf,size_t size) = 0;
    virtual size_t pos() const = 0;
    virtual size_t seek(pos_t pos) = 0;
    virtual size_t offset(pos_t ofs) = 0;
};

template<Mode MO>
struct basic_writer
{};

template<>
struct basic_writer<Buffer> : iwriter
{
    basic_writer(const basic_writer&) = delete;
    inline basic_writer(basic_writer&& o) :
        _buf(o._buf),_cur(o._cur)
    {
        o._cur = 0;
    }
    inline basic_writer(buffer_t& buf)
        : _buf(buf),_cur(0)
    {}

    inline size_t write(const uint8_t* buf,size_t size) override {
        if(!buf || size < 1)
            return 0;
        auto sub = _cur + size - _buf.size();
        if(sub > 0) _buf.resize(_cur + size);
        memcpy(_buf.data() + _cur,buf,size);
        _cur += size;
        return size;
    }

    inline void reserve(size_t capacity) override {
        _buf.reserve(capacity);
    }

    inline size_t pos() const override {
        return _cur;
    }

    inline size_t seek(pos_t pos) override {
        const auto end = _buf.size();   // cannot move out size
        _cur = end;
        if(pos >= 0 && pos < end)
            _cur = (size_t)pos;
        return this->pos();
    }

    inline size_t offset(pos_t ofs) override {
        auto end = _buf.size();     // cannot move out size
        auto cur = (pos_t)_cur + ofs;
        if(cur < 0) _cur = 0;
        else if(cur > end) _cur = end;
        return this->pos();
    }
private:
    buffer_t& _buf;
    size_t _cur;
};

template<>
struct basic_writer<Stream> : iwriter
{
    basic_writer(const basic_writer&) = delete;
    inline basic_writer(basic_writer&& o) :
        _so(o._so),_beg(o._beg)
    {}
    inline basic_writer(std::ostream& so)
        : _so(so),_beg((size_t)so.tellp())
    {}

    inline size_t write(const uint8_t* buf,size_t size) override {
        if(!buf || _so.fail() || size < 1)
            return 0;

        const auto before = _so.tellp();
        _so.write((char*)buf,size);
        auto pos = _so.tellp();
        if(pos == (std::streamsize)-1)
            return 0;
        return size_t(pos - before);
    }

    inline size_t pos() const override {
        return (size_t)_so.tellp() - _beg;
    }

    inline size_t seek(pos_t pos) override {
        _so.seekp(0,std::ios::end);
        const auto tail = _so.tellp();
        if(pos >= 0)
        {
            pos += _beg;
            if(pos < tail)
                _so.seekp(pos,std::ios::beg);
        }
        return this->pos();
    }

    inline size_t offset(pos_t ofs) override {
        _so.seekp(ofs,std::ios::cur);
        return this->pos();
    }
private:
    std::ostream& _so;
    const size_t _beg;  // the position of begin
};

template<Mode MO>
struct writer : public basic_writer<MO> {
    using super = basic_writer<MO>;
    using super::basic_writer;

    template<typename T>
    inline size_t write(const T* const data, size_t size) {
        return super::write((uint8_t*)data,size);
    }
};

inline writer<Buffer> make_writer(buffer_t& out)
{ return { out }; }

inline writer<Stream> make_writer(std::ostream& si)
{ return { si }; }

}

}

#endif // CK_BIO_HPP
