#include <libbndl/bundle.hpp>
#include "util.hpp"
#include <iostream>
#include <algorithm>

using namespace libbndl;

bool Bundle::Load(const std::string& name)
{
	if(m_stream.is_open())
		m_stream.close();

	m_stream.open(name,std::ios::in| std::ios::binary);
	//check if archive exists
	if (m_stream.fail())
		return false;

	m_mutex.lock();
	//check if it's a big archive
	std::string magic;
	for (int i = 0; i < 4; ++i)
		magic += m_stream.get();
	
	if (magic == std::string("BIGF"))
		m_version = CC;
	else if (magic == std::string("BIG4"))
		m_version = BFME;
	else
		return false;

	m_size = read<uint32_t>(m_stream);
	m_numEntries = reverse(read<uint32_t>(m_stream));
	uint32_t first = reverse(read<uint32_t>(m_stream));

	m_entries.clear();
	for (auto i = 0U; i < m_numEntries; i++)
	{
		Entry e;
		e.Offset = reverse(read<uint32_t>(m_stream));
		e.Size = reverse(read<uint32_t>(m_stream));
		auto Name = readString(m_stream);
		std::replace(Name.begin(),Name.end(),'\\','/');
		std::transform(Name.begin(),Name.end(),Name.begin(),::tolower);
		m_entries[Name] = e;
	}
	m_mutex.unlock();
	return true;
}

bool Bundle::Write(const std::string& name)
{
	return false;
}

uint8_t* Bundle::GetBinary(const std::string& entry, size_t& size)
{
	size = 0;
	std::string Name = entry;
	std::replace(Name.begin(),Name.end(),'\\','/');
	std::transform(Name.begin(),Name.end(),Name.begin(),::tolower);
	auto it = m_entries.find(Name);
	if (it == m_entries.end())
		return nullptr;

	m_mutex.lock();
	Entry e = it->second;
	uint8_t* buffer = new uint8_t[e.Size];
	m_stream.seekg(e.Offset, std::ios::beg);
	m_stream.read(reinterpret_cast<char*>(buffer), e.Size);
	size = e.Size;
	m_mutex.unlock();
	return buffer;
}

std::string Bundle::GetText(const std::string& entry)
{
	std::string Name = entry;
	std::replace(Name.begin(),Name.end(),'\\','/');
	std::transform(Name.begin(),Name.end(),Name.begin(),::tolower);
	auto it = m_entries.find(Name);
	if (it == m_entries.end())
		return std::string();

	m_mutex.lock();
	Entry e = it->second;
	std::string buffer;
	buffer.resize(e.Size);
	m_stream.seekg(e.Offset, std::ios::beg);
	m_stream.read(const_cast<char*>(buffer.data()), e.Size);
	m_mutex.unlock();
	return buffer;
}

Bundle::Entry Bundle::GetInfo(const std::string& entry)
{
	std::string Name = entry;
	std::replace(Name.begin(),Name.end(),'\\','/');
	std::transform(Name.begin(),Name.end(),Name.begin(),::tolower);
	auto it = m_entries.find(Name);
	if (it == m_entries.end())
		return Entry();
	
	return it->second;
}

void Bundle::AddEntry(const std::string & entry, const std::string & text, bool overwrite)
{
}

void Bundle::AddEntry(const std::string & entry, const uint8_t * data, size_t size, bool overwrite)
{
}


std::vector<std::string> Bundle::ListEntries()
{
	std::vector<std::string> entries;
	for (const auto& e : m_entries)
	{
		entries.push_back(e.first);
	}
	return entries;
}