#pragma once
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <istream>
#include <mutex>

namespace libbndl
{
	class Bundle
	{
	public:
		enum Version
		{
			CC		= 0,
			BFME	= 1,
		};

		struct Entry
		{
			uint32_t	Size;
			uint32_t	Offset;
		};
	public:
		bool Load(const std::string& name);
		bool Write(const std::string& name);

		//get entry info
		Entry GetInfo(const std::string& entry);
		//get entry binary
		uint8_t* GetBinary(const std::string& entry, uint32_t& size);
		//get entry as string
		std::string GetText(const std::string& entry);
		//get entry stream
		std::istream GetStream(const std::string& entry);
		//add entry 
		void AddEntry(const std::string& entry, const std::string& text,bool overwrite = true);
		void AddEntry(const std::string& entry, const uint8_t* data, uint32_t size, bool overwrite = true);

		std::vector<std::string> ListEntries();
	private:
		std::mutex					m_mutex;
		std::map<std::string,Entry>	m_entries;
		std::fstream				m_stream;
		uint32_t					m_size;
		uint32_t					m_numEntries;
		Version						m_version;
	};
}