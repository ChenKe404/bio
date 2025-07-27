#ifndef CK_BIO_HPP
#define CK_BIO_HPP

#include <cstdint>
#include <iostream>
#include <vector>

namespace ck
{

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
template<Mode MO>
struct basic_reader
{};

template<>
struct basic_reader<Buffer> {
    basic_reader(const basic_reader&) = delete;
    inline basic_reader(const uint8_t* data,uint64_t size)
        : _data(data),_size(size),_cur(data)
    {}
    inline basic_reader(const buffer_t* buf)
        : _data(buf->data()),_size(buf->size()),_cur(buf->data())
    {}

    inline uint64_t read(uint8_t* buf,uint64_t size) {
        if(!_data || !buf || size < 1)
            return 0;
        const auto remain = _size - pos();
        if(remain < 1)
            return 0;
        if(remain < size)
            size = remain;
        memcpy(buf,_cur,size);
        _cur += size;
        return size;
    }

    inline uint64_t remain() const {
        return _cur - _data;
    }

    inline uint64_t pos() const {
        return _cur - _data;
    }

    // pos < 0: to the end; others: to the pos
    inline void seek(int64_t pos) {
        if(!_data) return;
        const auto end = _data + _size;
        _cur = end;
        if(pos > 0)
        {
            _cur = _data + pos;
            if(_cur > end) _cur = end;
        }
    }

    // end of binary
    inline bool eob() const {
        return _cur >= _data + _size;
    }
private:
    const uint8_t* _data;    // source data pointer
    const uint64_t _size;      // source data size
    const uint8_t* _cur;
};

template<>
struct basic_reader<Stream> {
    basic_reader(const basic_reader&) = delete;
    inline basic_reader(std::istream& si)
        : _si(si),_beg(si.tellg())
    {
        si.seekg(0,std::ios::end);
        _remain = pos();
        si.seekg(_beg,std::ios::beg);
    }

    inline uint64_t read(uint8_t* buf,uint64_t size) {
        if(_si.eof() || _si.fail() || _remain < 1)
            return 0;
        if(_remain < size)
            size = _remain;
        _si.read((char*)buf,size);
        size = _si.gcount();
        _remain -= size;
        return size;
    }

    inline uint64_t remain() const {
        return _remain;
    }

    inline uint64_t pos() const {
        return (uint64_t)_si.tellg() - _beg;
    }

    inline void seek(int64_t pos) {
        _si.seekg(0,std::ios::end);
        const auto size = _si.tellg();
        if(pos > 0)
        {
            pos += _beg;
            if(pos > size)
                _remain = 0;
            else {
                _si.seekg(pos, std::ios::beg);
                _remain = size - pos;
            }
        }
    }

    inline bool eob() const {
        return _si.eof() || _remain < 1;
    }
private:
    std::istream& _si;
    const uint64_t _beg;  // the position of begin
    uint64_t _remain;
};

template<Mode MO>
struct reader : public basic_reader<MO> {
    using super = basic_reader<MO>;
    using super::basic_reader;

    template<typename T>
    inline uint64_t read(T* out, uint64_t size) {
        return super::read((uint8_t*)out,size);
    }

    template<typename T>
    inline uint64_t read(T& out, uint64_t size) {
        return read(&out,size);
    }

    template<typename T>
    uint64_t read(T* out) {
        return read(out,sizeof(T));
    }

    template<typename T>
    uint64_t read(T& out) {
        return read(&out,sizeof(T));
    }
};

inline reader<Buffer> make_reader(uint8_t* in,uint64_t in_size)
{ return reader<Buffer>{ in,in_size }; }

inline reader<Buffer> make_reader(const buffer_t* in)
{ return reader<Buffer>{ in }; }

reader<Stream> make_reader(std::istream& si)
{ return reader<Stream>{ si }; }


/////////////////////////////////////////////////////////////////////
/// writer
template<Mode MO>
struct basic_writer
{};

template<>
struct basic_writer<Buffer>
{
    basic_writer(const basic_writer&) = delete;
    inline basic_writer(uint8_t* buf,uint64_t capacity)
        : _buf(buf),_capacity(capacity),_cur(buf)
    {}
    inline basic_writer(buffer_t& buf)
        : _buf(buf.data()),_capacity(buf.capacity()),_cur(buf.data())
    {}

    // @return: if less than "size",means output buffer capacity not enough
    inline uint64_t write(const uint8_t* buf,uint64_t size) {
        if(!_buf || !buf || size < 1)
            return 0;
        const auto remain = _capacity - pos();
        if(remain < 1)
            return 0;
        if(remain < size)
            size = remain;
        memcpy(_cur,buf,size);
        _cur += size;
        return size;
    }

    inline uint64_t pos() const {
        return _cur - _buf;
    }

    inline void seek(int64_t pos) {
        if(!_buf) return;
        const auto end = _buf + _capacity;
         _cur = end;
        if(pos > 0)
        {
            _cur = _buf + pos;
            if(_cur > end) _cur = end;
        }
    }

    inline bool eob() const {
        return _cur - _buf >= _capacity;
    }
private:
    uint8_t* _buf;
    uint64_t _capacity;
    uint8_t* _cur;
};

template<>
struct basic_writer<Stream>
{
    basic_writer(const basic_writer&) = delete;
    inline basic_writer(std::ostream& so)
        : _so(so),_beg(so.tellp())
    {}

    // @return: if less than "size",means output stream can't continue writing
    inline uint64_t write(const uint8_t* buf,uint64_t size) {
        if(!buf || _so.fail() || size < 1)
            return 0;
        uint64_t remain = size, remain_last = 0u;
        while(remain > 0) {
            if(_so.fail())
                break;
            auto pos = _so.tellp();
            _so.write((char*)buf,remain);
            remain -= _so.tellp() - pos;
            if(remain > 0)
            {
                if(remain == remain_last)   // no byte written at all
                    break;
                else
                    _so.flush();
            }
            remain_last = remain;
        }
        return size - remain;
    }

    inline uint64_t pos() const {
        return (uint64_t)_so.tellp() - _beg;
    }

    inline void seek(int64_t pos) {
        _so.seekp(0,std::ios::end);
        const auto tail = _so.tellp();
        if(pos > 0)
        {
            pos += _beg;
            if(pos < tail)
                _so.seekp(pos,std::ios::beg);
        }
    }

    inline bool eob() const {
        return _so.eof();
    }
private:
    std::ostream& _so;
    const uint64_t _beg;  // the position of begin
};

template<Mode MO>
struct writer : public basic_writer<MO> {
    using super = basic_writer<MO>;
    using super::basic_writer;

    template<typename T>
    inline uint64_t write(T* out, uint64_t size) {
        return super::write((uint8_t*)out,size);
    }

    template<typename T>
    inline uint64_t write(T& out, uint64_t size) {
        return write(&out,size);
    }

    template<typename T>
    uint64_t write(T* out) {
        return write(out,sizeof(T));
    }

    template<typename T>
    uint64_t write(T& out) {
        return write(&out,sizeof(T));
    }

    template<typename C>
    uint64_t write(const C* cstr) {
        return write(cstr,std::char_traits<C>::length(cstr));
    }
};

inline writer<Buffer> make_writer(uint8_t* out,uint64_t capacity)
{ return writer<Buffer>{ out,capacity }; }

inline writer<Buffer> make_writer(buffer_t& out)
{ return writer<Buffer>{ out }; }

writer<Stream> make_writer(std::ostream& si)
{ return writer<Stream>{ si }; }

}

}

#endif // CK_BIO_HPP
