#include <libbndl/bundle.hpp>
#include "util.hpp"
#include "lock.hpp"
#include <algorithm>
#include <cassert>
#include <zlib.h>

using namespace libbndl;

bool Bundle::Load(const std::string &name)
{
	if (m_stream.is_open())
		m_stream.close();

	m_stream.open(name, std::ios::in | std::ios::binary);

	// Check if archive exists
	if (m_stream.fail())
		return false;

	Lock mutexLock(m_mutex);

	// Check if it's a BNDL archive
	std::string magic;
	for (auto i = 0; i < 4; i++)
		magic += m_stream.get();
	if (magic == std::string("bndl"))
		m_version = BNDL;
	else if (magic == std::string("bnd2"))
		m_version = BND2;
	else
		return false;

	// Only supporting BND2 atm.
	if (m_version == BNDL)
		return false;

	auto bundleVersion = read<uint32_t>(m_stream);

	m_platform = read<Platform>(m_stream);
	const auto isBigEndian = (m_platform != PC);

	if (isBigEndian)
		bundleVersion = endianSwap(bundleVersion);
	// Little sanity check.
	if (bundleVersion != 2)
		return false;

	const auto headerLength = read<uint32_t>(m_stream, isBigEndian);
	// Another sanity check.
	if (headerLength != 48)
		return false;

	m_numEntries = read<uint32_t>(m_stream, isBigEndian);

	m_idBlockOffset = read<uint32_t>(m_stream, isBigEndian);
	m_fileBlockOffsets[0] = read<uint32_t>(m_stream, isBigEndian);
	m_fileBlockOffsets[1] = read<uint32_t>(m_stream, isBigEndian);
	m_fileBlockOffsets[2] = read<uint32_t>(m_stream, isBigEndian);

	const auto flags = read<Flags>(m_stream, isBigEndian);
	m_compressed = (flags & Compressed);

	// Last 8 bytes are padding.


	m_stream.seekg(m_idBlockOffset);

	m_entries.clear();
	for (auto i = 0U; i < m_numEntries; i++)
	{
		Entry e;

		// These are stored in bundle as 64-bit (8-byte), but are really 32-bit.
		auto fileID = static_cast<uint32_t>(read<uint64_t>(m_stream, isBigEndian));
		e.checksum = static_cast<uint32_t>(read<uint64_t>(m_stream, isBigEndian));

		// The uncompressed sizes have a high nibble that varies depending on the file type for whatever reason.
		e.fileBlockDataInfo[0].uncompressedSize = read<uint32_t>(m_stream, isBigEndian) & ~(0xFU << 28);
		e.fileBlockDataInfo[1].uncompressedSize = read<uint32_t>(m_stream, isBigEndian) & ~(0xFU << 28);
		e.fileBlockDataInfo[2].uncompressedSize = read<uint32_t>(m_stream, isBigEndian) & ~(0xFU << 28);
		e.fileBlockDataInfo[0].compressedSize = read<uint32_t>(m_stream, isBigEndian);
		e.fileBlockDataInfo[1].compressedSize = read<uint32_t>(m_stream, isBigEndian);
		e.fileBlockDataInfo[2].compressedSize = read<uint32_t>(m_stream, isBigEndian);
		e.fileBlockDataInfo[0].offset = read<uint32_t>(m_stream, isBigEndian);
		e.fileBlockDataInfo[1].offset = read<uint32_t>(m_stream, isBigEndian);
		e.fileBlockDataInfo[2].offset = read<uint32_t>(m_stream, isBigEndian);

		e.pointersOffset = read<uint32_t>(m_stream, isBigEndian);
		e.fileType = read<FileType>(m_stream, isBigEndian);
		e.numberOfPointers = read<uint16_t>(m_stream, isBigEndian);

		m_stream.seekg(2, std::ios::cur); // Padding

		m_entries[fileID] = e;
	}

	return true;
}

bool Bundle::Write(const std::string& name)
{
	// Unimplemented for now.
	return false;
}

Bundle::EntryData* Bundle::GetBinary(uint32_t fileID)
{
	const auto it = m_entries.find(fileID);
	if (it == m_entries.end())
		return nullptr;

	const auto data = new EntryData;
	for (auto i = 0; i < 3; i++)
	{
		EntryDataBlock *dataBlock = GetBinary(fileID, i);
		data->fileBlockData[i] = *dataBlock;
		delete dataBlock;
	}

	return data;
}

Bundle::EntryDataBlock* Bundle::GetBinary(uint32_t fileID, uint32_t fileBlock)
{
	const auto it = m_entries.find(fileID);
	if (it == m_entries.end())
		return nullptr;

	Lock mutexLock(m_mutex);

	const Entry e = it->second;

	const auto dataBlock = new EntryDataBlock;
	EntryDataInfo dataInfo = e.fileBlockDataInfo[fileBlock];

	const size_t readSize = m_compressed ? dataInfo.compressedSize : dataInfo.uncompressedSize;
	if (readSize == 0)
	{
		dataBlock->data = nullptr;
		dataBlock->size = 0;
		return dataBlock;
	}

	auto *buffer = new uint8_t[readSize];
	m_stream.seekg(m_fileBlockOffsets[fileBlock] + dataInfo.offset);
	m_stream.read(reinterpret_cast<char *>(buffer), readSize);

	if (m_compressed)
	{
		uLongf uncompressedSize = dataInfo.uncompressedSize;
		auto *uncompressedBuffer = new uint8_t[uncompressedSize];
		const auto ret = uncompress(uncompressedBuffer, &uncompressedSize, buffer, static_cast<uLong>(readSize));

		assert(ret == Z_OK);
		assert(uncompressedSize == dataInfo.uncompressedSize);

		delete[] buffer;
		buffer = uncompressedBuffer;
	}

	dataBlock->data = buffer;
	dataBlock->size = dataInfo.uncompressedSize;

	return dataBlock;
}

Bundle::Entry Bundle::GetInfo(uint32_t fileID) const
{
	const auto it = m_entries.find(fileID);
	if (it == m_entries.end())
		return {};
	
	return it->second;
}

/*void Bundle::AddEntry(uint32_t fileID, const std::string & text, bool overwrite)
{
}

void Bundle::AddEntry(uint32_t fileID, const uint8_t * data, size_t size, bool overwrite)
{
}*/


std::vector<uint32_t> Bundle::ListEntries() const
{
	std::vector<uint32_t> entries;
	for (const auto &e : m_entries)
	{
		entries.push_back(e.first);
	}
	return entries;
}

std::map<Bundle::FileType, std::vector<uint32_t>> Bundle::ListEntriesByFileType() const
{
	std::map<FileType, std::vector<uint32_t>> entriesByFileType;
	for (const auto &e : m_entries)
	{
		entriesByFileType[e.second.fileType].push_back(e.first);
	}
	return entriesByFileType;
}
