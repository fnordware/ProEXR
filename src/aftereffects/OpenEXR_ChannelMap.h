/* ---------------------------------------------------------------------
// 
// ProEXR - OpenEXR plug-ins for Photoshop and After Effects
// Copyright (c) 2007-2017,  Brendan Bolles, http://www.fnordware.com
// 
// This file is part of ProEXR.
//
// ProEXR is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// -------------------------------------------------------------------*/

#ifndef OPENEXR_CHANNEL_MAP_H
#define OPENEXR_CHANNEL_MAP_H


#include <string>
#include <vector>


class ChannelEntry {
	public:
		ChannelEntry() {}
		ChannelEntry(const char *channel_name, long type_code, long data_code);
		ChannelEntry(const char *channel_name, const char *type_code, const char *data_code);
		
		std::string name(void) { return chan_name; }
		long type(void) { return chan_type_code; }
		long data(void) { return chan_data_code; }
		
		int dimensions(void);
		std::string chan_part(int index);
		std::string key_name(void) { return chan_part(0); }
		
	private:
		std::vector<int> bar_positions(void);
		
		std::string chan_name;
		long chan_type_code;
		long chan_data_code;
};

class ChannelMap {
	public:
		ChannelMap(const char *file_path);
				
		bool exists(void) { return map.size() > 0; }
		
		bool findEntry(const char *channel_name, ChannelEntry &entry, bool search_all=false);
		bool findEntry(const char *channel_name, bool search_all=false);
		void addEntry(ChannelEntry entry) { map.push_back(entry); }
	
	private:
		std::vector<ChannelEntry> map;
};


#endif // OPENEXR_CHANNEL_MAP_H