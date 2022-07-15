#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <imagine/config/defs.hh>
#include <imagine/io/ioDefs.hh>
#include <imagine/util/bitset.hh>
#include <imagine/util/memory/Buffer.hh>
#include <imagine/util/concepts.hh>
#include <memory>
#include <utility>

namespace IG
{

class IOBuffer : public ByteBuffer
{
public:
	using ByteBuffer::ByteBuffer;
	using Flags = uint8_t;

	static constexpr Flags MAPPED_FILE_BIT = bit(0);

	constexpr IOBuffer(std::span<uint8_t> span, Flags flags, DeleterFunc deleter = [](const uint8_t*, size_t){}):
		ByteBuffer{span, deleter}, flags{flags} {}

	constexpr bool isMappedFile() const { return flags & MAPPED_FILE_BIT; }

protected:
	Flags flags{};
};

template <class IO>
class IOUtils
{
public:
	off_t seekS(off_t offset);
	off_t seekE(off_t offset);
	off_t seekC(off_t offset);
	bool rewind();
	off_t tell();
	ssize_t send(IO &output, off_t *srcOffset, size_t bytes);
	IOBuffer buffer(IODefs::BufferMode mode = IODefs::BufferMode::DIRECT);

	template <class T, bool useOffset = false>
	T getImpl(off_t offset = 0)
	{
		if constexpr(std::is_same_v<T, bool>)
		{
			// special case to convert value to a valid bool
			return getImpl<uint8_t, useOffset>(offset);
		}
		else
		{
			T obj;
			ssize_t size;
			if constexpr(useOffset)
				size = static_cast<IO*>(this)->readAtPos(&obj, sizeof(T), offset);
			else
				size = static_cast<IO*>(this)->read(&obj, sizeof(T));
			if(size < (ssize_t)sizeof(T)) [[unlikely]]
				return {};
			return obj;
		}
	}

	template <class T>
	T get()
	{
		return getImpl<T>();
	}

	template <class T>
	T get(off_t offset)
	{
		return getImpl<T, true>(offset);
	}

	ssize_t readSized(IG::ResizableContainer auto &c, size_t maxBytes)
	{
		if(c.max_size() < maxBytes)
			return -1;
		c.resize(maxBytes);
		auto bytesRead = static_cast<IO*>(this)->read(c.data(), maxBytes);
		if(bytesRead == -1) [[unlikely]]
			return -1;
		c.resize(bytesRead);
		return bytesRead;
	}

	ssize_t write(IG::NotPointerDecayable auto &&obj)
	{
		return static_cast<IO*>(this)->write(&obj, sizeof(decltype(obj)));
	}
};

class IO : public IOUtils<IO>
{
public:
	using IOUtilsBase = IOUtils<IO>;
	using IOUtilsBase::write;
	using IOUtilsBase::seekS;
	using IOUtilsBase::seekE;
	using IOUtilsBase::seekC;
	using IOUtilsBase::tell;
	using IOUtilsBase::send;
	using IOUtilsBase::buffer;
	using IOUtilsBase::get;
	using AccessHint = IODefs::AccessHint;
	using Advice = IODefs::Advice;
	using BufferMode = IODefs::BufferMode;
	using SeekMode = IODefs::SeekMode;
	using OpenFlags = IODefs::OpenFlags;

	// allow reading file
	static constexpr OpenFlags READ_BIT = IG::bit(0);
	// allow modifying file
	static constexpr OpenFlags WRITE_BIT = IG::bit(1);
	// create a new file if it doesn't already exist
	static constexpr OpenFlags CREATE_BIT = IG::bit(2);
	// if using WRITE_BIT/READ_WRITE_BIT, truncate any existing file to 0 bytes
	static constexpr OpenFlags TRUNCATE_BIT = IG::bit(3);
	// return from constructor without throwing exception if opening fails,
	// used to avoid redundant FS::exists() tests when searching for a file to open
	static constexpr OpenFlags TEST_BIT = IG::bit(4);
	static constexpr OpenFlags OPEN_FLAGS_BITS = 5;

	// common flag combinations
	static constexpr OpenFlags OPEN_NEW = WRITE_BIT | CREATE_BIT | TRUNCATE_BIT;
	static constexpr OpenFlags OPEN_RW = READ_BIT | WRITE_BIT | CREATE_BIT;

	constexpr IO() = default;
	virtual ~IO() = default;

	// reading
	virtual ssize_t read(void *buff, size_t bytes) = 0;
	virtual ssize_t readAtPos(void *buff, size_t bytes, off_t offset);

	// writing
	virtual ssize_t write(const void *buff, size_t bytes) = 0;
	virtual bool truncate(off_t offset);

	// seeking
	virtual off_t seek(off_t offset, SeekMode mode) = 0;

	// other functions
	virtual std::span<uint8_t> map();
	virtual void sync();
	virtual size_t size() = 0;
	virtual bool eof() = 0;
	virtual void advise(off_t offset, size_t bytes, Advice advice);
	virtual explicit operator bool() const = 0;
};

class GenericIO : public IOUtils<GenericIO>
{
public:
	using IOUtilsBase = IOUtils<GenericIO>;
	using IOUtilsBase::write;
	using IOUtilsBase::seekS;
	using IOUtilsBase::seekE;
	using IOUtilsBase::seekC;
	using IOUtilsBase::tell;
	using IOUtilsBase::send;
	using IOUtilsBase::buffer;
	using IOUtilsBase::get;

	constexpr GenericIO() = default;
	GenericIO(std::derived_from<IO> auto io): io{std::make_unique<decltype(io)>(std::move(io))} {}
	GenericIO(std::unique_ptr<IO> io);
	explicit operator IO*();
	operator IO&();
	IO *release();
	FILE *moveToFileStream(const char *opentype);
	ssize_t read(void *buff, size_t bytes);
	ssize_t readAtPos(void *buff, size_t bytes, off_t offset);
	std::span<uint8_t> map();
	ssize_t write(const void *buff, size_t bytes);
	bool truncate(off_t offset);
	off_t seek(off_t offset, IO::SeekMode mode);
	void sync();
	size_t size();
	bool eof();
	void advise(off_t offset, size_t bytes, IO::Advice advice);
	explicit operator bool() const;

protected:
	std::unique_ptr<IO> io{};
};

}
