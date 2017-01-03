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

#include "ProEXR_UTF.h"


#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>


bool UTF8toUTF16(const std::string &str, utf16_char *buf, unsigned int max_len)
{
	bool result = false;

	CFStringRef input = CFStringCreateWithCString(kCFAllocatorDefault, 
													str.c_str(),
													kCFStringEncodingUTF8);
	
	if(input)
	{
		CFIndex len = CFStringGetLength(input);
		
		if(len < max_len)
		{
			CFRange range = {0, len};
			
			CFIndex chars = CFStringGetBytes(input, range,
												kCFStringEncodingUTF16, '?', FALSE,
												(UInt8 *)buf, max_len * sizeof(utf16_char), NULL);
			
			result = (chars == len);
			
			buf[len] = '\0';
		}
		
		
		CFRelease(input);
	}
	
	
	return result;	
}


std::string UTF16toUTF8(const utf16_char *str)
{
	std::string output;

	unsigned int len = 0;
	
	while(str[len] != '\0')
		len++;

	CFStringRef input = CFStringCreateWithBytes(kCFAllocatorDefault, 
												(const UInt8 *)str, len * sizeof(utf16_char),
												kCFStringEncodingUTF16, FALSE);
												
	if(input)
	{
		CFIndex len = CFStringGetLength(input);
		
		CFRange range = {0, len};
		
		CFIndex size = CFStringGetMaximumSizeForEncoding(len + 1, kCFStringEncodingUTF8);
		
		char buf[size];
		
		CFIndex usedBufLen = 0;
		
		CFIndex chars = CFStringGetBytes(input, range,
											kCFStringEncodingUTF8, '?', FALSE,
											(UInt8 *)buf, size, &usedBufLen);
											
		buf[usedBufLen] = '\0';
		
		output = buf;
	
		CFRelease(input);
	}
	
	return output;
}

#endif // __APPLE__


#ifdef WIN32

#include <Windows.h>

bool UTF8toUTF16(const std::string &str, utf16_char *buf, unsigned int max_len)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)buf, max_len);

	return (len != 0);
}


std::string UTF16toUTF8(const utf16_char *str)
{
	std::string output;

	// first one just returns the required length
	int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)str, -1, NULL, 0, NULL, NULL);

	char *buf = new char[len];

	int len2 = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)str, -1, buf, len, NULL, NULL);

	output = buf;

	delete [] buf;

	return output;
}


#endif // WIN32