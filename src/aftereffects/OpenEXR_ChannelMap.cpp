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

#include "OpenEXR_ChannelMap.h"

#ifndef __MACH__
#include <string>
#endif

#include <fstream>


using namespace std;

// translates to 'FLT4' or whatever
static inline long four_char_code_2_long(const char *code)
{
	return ( code[0] << 24 | code[1] << 16 | code[2] << 8 | code[3] );
}

// init
ChannelEntry::ChannelEntry(const char *channel_name, long type_code, long data_code)
{
	chan_name = channel_name;
	
	chan_type_code = type_code;
	chan_data_code = data_code;
}

// init with strings
ChannelEntry::ChannelEntry(const char *channel_name, const char *type_code, const char *data_code)
{
	chan_name = channel_name;
	
	chan_type_code = four_char_code_2_long(type_code);
	chan_data_code = four_char_code_2_long(data_code);
}

// returns an arraw with index positions of '|' pipes
vector<int> ChannelEntry::bar_positions(void)
{
	vector<int> positions;
	
	int index = 0;
	
	for(string::iterator i = chan_name.begin() ; i != chan_name.end(); ++i)
	{
		if(*i == '|')
			positions.push_back( index );
		
		index++;
	}
	
	return positions;
}

// sees how many channels are listed in the name (seperated by '|')
int ChannelEntry::dimensions(void)
{
	vector<int> dividers = bar_positions();
	
	return dividers.size() + 1;
}

// hands back one of the channel names
string ChannelEntry::chan_part(int index)
{
	vector<int> dividers = bar_positions();

	if(dividers.size() == 0)
		return chan_name;
	else if(index == 0)
		return chan_name.substr(0, dividers[0]);
	else if(index == dividers.size())
		return chan_name.substr(dividers[index-1] + 1);
	else
		return chan_name.substr(dividers[index-1] + 1, dividers[index] - (dividers[index-1] + 1));
}

#define WHITESPACE	" \t\r\n"

// fills out this ChannelMap structure, given a path to a config text file (see sample below)
ChannelMap::ChannelMap(const char *file_path)
{
	if(file_path)
	{
		// open file for reading
		ifstream input_stream(file_path, ios_base::in);
		
		if( !input_stream.fail() )
		{
			string s;
			
			while( getline(input_stream, s) )
			{
				if(s.size() > 5 && s[0] != '#')
				{
					// parse the line into a ChannelEntry
					const int name_begin	= s.find_first_not_of(WHITESPACE, 0);
					const int name_end	= s.find_first_of(WHITESPACE, name_begin) - 1;
					const int type_begin	= s.find_first_not_of(WHITESPACE, name_end+1);
					const int type_end	= s.find_first_of(WHITESPACE, type_begin) - 1;
					const int data_begin	= s.find_first_not_of(WHITESPACE, type_end+1);
					const int data_end	= s.find_first_of(WHITESPACE, data_begin) - 1;
					
					const string name = s.substr(name_begin, 1 + name_end - name_begin);
					const string type = s.substr(type_begin, 1 + type_end - type_begin);
					const string data = s.substr(data_begin, 1 + data_end - data_begin);
					
					ChannelEntry entry(name.c_str(), type.c_str(), data.c_str());
					
					map.push_back( entry );
				}
			}
		}
	}
}

// given a channel name, returns the entry
// only looks for the first channel in the name
bool ChannelMap::findEntry(const char *channel_name, ChannelEntry &entry, bool search_all)
{
	for(vector<ChannelEntry>::iterator j = map.begin(); j != map.end(); ++j)
	{
		if(search_all)
		{
			for(int i=0; i < j->dimensions(); i++)
			{
				if(string(channel_name) == j->chan_part(i))
				{
					entry = *j;
					return true;
				}
			}
		}
		else
		{
			if(string(channel_name) == j->key_name())
			{
				entry = *j;
				return true;
			}
		}
	}
	
	return false;
}

// just see if the entry is there, don't supply it
bool ChannelMap::findEntry(const char *channel_name, bool search_all)
{
	ChannelEntry entry;
	return findEntry(channel_name, entry, search_all);
}


/*	This is the default OpenEXR_channel_map.txt file that should be placed next to OpenEXR.plugin (Mac)
	or OpenEXR.aex (Win).

# OpenEXR_channel_map.txt
#
# Place this file beside the OpenEXR reader plug-in.  It was downloaded from
# http://www.fnordware.com/OpenEXR
#
# The purpose of this file is to identify channel types in an EXR file based on name.
# Other than "R", "G", "B", and "A", OpenEXR doesn't use any standard names for
# channels, including common ones like Z-depth.
#
# On the other hand, many After Effects plug-ins retrieve these channels based on them
# being tagged properly.  This file lets you map channel names to their type and
# data format.
#
# The supported channel types in AE are:
#
# Z-Depth				DPTH
# Normals				NRML
# Object ID				OBID
# Motion Vectors		MTVR
# Background Color		BKCR
# Texture UVs			TEXR
# Coverage				COVR
# Node					NODE
# Material ID			MATR
# Unclamped RGB			UNCP
#
#
# The AE data types are:
#
# float					FLT4
# double				DBL8
# long					LON4
# short					SHT2
# Fixed					FIX4
# char					CHR1
# unsigned char			UBT1
# unsigned short		UST2
# unsigned Fixed		UFX4
# RGB color				RGB
#
# 
# EXR channels are only FLOAT, HALF (16-bit float), or UINT.  Therefore the only AE data
# types supported by the reader are float, long, short, unsigned short, char, and unsigned char.
#
# a typical entry in this file goes like this (examples should be down below):
#
# <Channel Name>		<Channel Type>		<Data Type>
#
#
# For some properties to make sense (like Velocity), you actually need multiple channels
# working together.  Combine them in one entry, with a pipe '|' between the channel names.
# AE will see that property as a multi-dimensional channel.
#
# Hopefully nobody decides to use '|' in their channel names.
#
# Channel names are case-sensitive, BTW.
#
#
# Un-comment this line for Floating Point Gray support
# Y	UNKN	FLT4
#
# 

Z			DPTH		FLT4
depth.Z		DPTH		FLT4

materialID	MATR		UBT1
objectID	OBID		UST2

velX|velY				MTVR		FLT4
Velocity.X|Velocity.Y	MTVR		FLT4



-end of file- */