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

#include "OpenEXR_PlatformIO.h"

#include "Iex.h"

#include "ProEXR_UTF.h"

#include <time.h>
#include <assert.h>

using namespace Imf;
using namespace Iex;


extern AEGP_PluginID	S_mem_id;

static AEGP_MemHandle	file_cacheH = NULL;
static PathString		file_cache_path;
static DateTime			file_cache_date_time;
static Int64			file_cache_size = 0;
static time_t			file_cache_last_access;


static bool
MatchCacheDateTime(DateTime date_time)
{
#ifdef __APPLE__
	return (date_time.fraction == file_cache_date_time.fraction &&
			date_time.lowSeconds == file_cache_date_time.lowSeconds &&
			date_time.highSeconds == file_cache_date_time.highSeconds);
#else
	return (date_time.dwHighDateTime == file_cache_date_time.dwHighDateTime &&
			date_time.dwLowDateTime == file_cache_date_time.dwLowDateTime);
#endif
}


static void *
LockFileCache(const SPBasicSuite *pica_basicP)
{
	A_Err err = A_Err_NONE;
	
	if(pica_basicP == NULL)
		throw LogicExc("pica_basicP is NULL");
	
	AEGP_SuiteHandler suites(pica_basicP);

	void *cache = NULL;
	
	if(file_cacheH)
	{
		err = suites.MemorySuite()->AEGP_LockMemHandle(file_cacheH, (void**)&cache);
		
		file_cache_last_access = time(NULL);
	}
	
	return cache;
}


static void
UnlockFileCache(const SPBasicSuite *pica_basicP)
{
	A_Err err = A_Err_NONE;
	
	if(pica_basicP == NULL)
		throw LogicExc("pica_basicP is NULL");
	
	AEGP_SuiteHandler suites(pica_basicP);

	if(file_cacheH)
		err = suites.MemorySuite()->AEGP_UnlockMemHandle(file_cacheH);
}


void
DeleteFileCache(const SPBasicSuite *pica_basicP, int timeout)
{
	if(pica_basicP == NULL)
		throw LogicExc("pica_basicP is NULL");
	
	if(file_cacheH && (timeout == 0 || (difftime(time(NULL), file_cache_last_access) > timeout)) )
	{
		AEGP_SuiteHandler suites(pica_basicP);

		A_Err err = suites.MemorySuite()->AEGP_FreeMemHandle(file_cacheH);
		
		file_cacheH = NULL;
		file_cache_path = "";
		file_cache_size = 0;
	}
}


static void *
NewFileCache(const SPBasicSuite *pica_basicP, AEGP_MemSize size)
{
	A_Err err = A_Err_NONE;
	
	if(pica_basicP == NULL)
		throw LogicExc("pica_basicP is NULL");
	
	AEGP_SuiteHandler suites(pica_basicP);

	DeleteFileCache(pica_basicP);
	
	err = suites.MemorySuite()->AEGP_NewMemHandle(S_mem_id, "File Cache", size,
													AEGP_MemFlag_CLEAR, &file_cacheH);
													
	if(file_cacheH)
		file_cache_size = size;
																																					
	return LockFileCache(pica_basicP);
}

#pragma mark-


IStreamPlatform::IStreamPlatform(const char fileName[], const SPBasicSuite *pica_basicP) :
	IStream(fileName),
	_pica_basicP(pica_basicP),
	_vfile(NULL),
	_path(fileName)
{
	open_file(fileName);
	
	_modtime = file_modtime();
}


IStreamPlatform::IStreamPlatform(const uint16_t fileName[], const SPBasicSuite *pica_basicP) :
	IStream("Unicode Path"),
	_pica_basicP(pica_basicP),
	_vfile(NULL),
	_path(fileName)
{
	open_file(fileName);
	
	_modtime = file_modtime();
}


IStreamPlatform::~IStreamPlatform()
{
	try{
		close_file();
	} catch(...) {}

	release_cache();
}


bool
IStreamPlatform::isMemoryMapped() const
{
	return (_vfile != NULL);
}


bool
IStreamPlatform::read(char c[/*n*/], int n)
{
	if( isMemoryMapped() )
	{
		bool success = true;
		
		try{
			memcpy(c, readMemoryMapped(n), n);
		}catch(...) { success = false; } // readMemoryMapped() will throw an excpetion if there's anything dicey
		
		return success;
	}
	else
		return read_file(c, n);
}

char *
IStreamPlatform::readMemoryMapped(int n)
{
	if( !isMemoryMapped() )
		throw LogicExc("readMemoryMapped() called for non memory-mapped file.");
	
	if(n > (_vsize - _voffset))
		throw IoExc("Trying to read past the end of a memory-mapped file.");
	
	char *ptr = ((char *)_vfile + _voffset);
	
	_voffset += n;
	
	return ptr;
}


Int64
IStreamPlatform::tellg()
{
	if( isMemoryMapped() )
	{
		return _voffset;
	}
	else
		return tellg_file();
}


void
IStreamPlatform::seekg(Int64 pos)
{
	if( isMemoryMapped() )
	{
		if(pos > _vsize)
			throw IoExc("Trying to seek past the end of a memory-mapped file.");
		
		_voffset = pos;
	}
	else
		seekg_file(pos);
}


void
IStreamPlatform::memoryMap()
{
	assert(_pica_basicP != NULL);

	if(_pica_basicP && !isMemoryMapped() )
	{
		if(file_cacheH && MatchCacheDateTime(_modtime) && (_path == file_cache_path))
		{
			adopt_cache();
		}
		else
		{
			_vsize = file_size();
			
			_vfile = NewFileCache(_pica_basicP, _vsize);
			
			if(_vfile)
			{
				_voffset = tellg_file();
				
				seekg_file(0);
				
				read_file((char *)_vfile, _vsize);
				
				file_cache_path = _path;
				
				file_cache_date_time = _modtime;
				
				seekg_file(_voffset);
			}
		}
	}
}


void
IStreamPlatform::unMemoryMap()
{
	if( isMemoryMapped() )
	{
		//seekg_file(_voffset); // use if you want to still access the file
		
		UnlockFileCache(_pica_basicP);
		
		_vfile = NULL;
	}
}


void
IStreamPlatform::adopt_cache()
{
	if(file_cacheH == NULL)
		throw LogicExc("Can't adopt a NULL cache.");

	_vfile = LockFileCache(_pica_basicP);
	
	_voffset = tellg_file();
	
	_vsize = file_cache_size;
}


void
IStreamPlatform::release_cache()
{
	unMemoryMap();
}


#ifdef __APPLE__
void
IStreamPlatform::open_file(const char fileName[])
{
	OSErr result = noErr;
	
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)fileName, strlen(fileName) + 1, FALSE);
	if(url == NULL)
		throw IoExc ("Couldn't make CFURLRef.");
	
	Boolean success = CFURLGetFSRef(url, &_fsRef);
	CFRelease(url);
	if(!success)
		throw IoExc ("Couldn't make FSRef.");
	
	HFSUniStr255 dataForkName;
	result = FSGetDataForkName(&dataForkName);

	result = FSOpenFork(&_fsRef, dataForkName.length, dataForkName.unicode, fsRdPerm, &_refNum);

	if(result != noErr)
		throw IoExc ("Couldn't open file for reading.");
}


void
IStreamPlatform::open_file(const uint16_t fileName[])
{
	OSErr result = noErr;
	
	const int len = PathString::StrLen(fileName);
	
	CFStringRef inStr = CFStringCreateWithCharacters(kCFAllocatorDefault, fileName, len);
	if(inStr == NULL)
		throw IoExc("Couldn't make CFStringRef.");
		
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, inStr, kCFURLPOSIXPathStyle, 0);
	CFRelease(inStr);
	if(url == NULL)
		throw IoExc("Couldn't make CFURLRef.");
	
	Boolean success = CFURLGetFSRef(url, &_fsRef);
	CFRelease(url);
	if(!success)
		throw IoExc("Couldn't make FSRef.");
	
	HFSUniStr255 dataForkName;
	result = FSGetDataForkName(&dataForkName);

	result = FSOpenFork(&_fsRef, dataForkName.length, dataForkName.unicode, fsRdPerm, &_refNum);

	if(result != noErr)
		throw IoExc("Couldn't open file for reading.");
}


void
IStreamPlatform::close_file()
{
	if(_refNum == 0)
		throw LogicExc("_refNum is 0.");

	OSErr result = FSCloseFork(_refNum);

	if(result != noErr)
		throw IoExc("Error closing file.");
}


bool
IStreamPlatform::read_file(char c[/*n*/], int n)
{
	if(_refNum == 0)
		throw LogicExc("_refNum is 0.");

	ByteCount count = n;
	
	OSErr result = FSReadFork(_refNum, fsAtMark, 0, count, (void *)c, &count);
	
	return (result == noErr && count == n);
}


Int64
IStreamPlatform::tellg_file()
{
	if(_refNum == 0)
		throw LogicExc("_refNum is 0.");

	SInt64 lpos;

	OSErr result = FSGetForkPosition(_refNum, &lpos);
	
	if(result != noErr)
		throw IoExc("Error calling FSGetForkPosition().");

	return lpos;
}


void
IStreamPlatform::seekg_file(Int64 pos)
{
	if(_refNum == 0)
		throw LogicExc("_refNum is 0.");
	
	OSErr result = FSSetForkPosition(_refNum, fsFromStart, pos);

	if(result != noErr)
		throw IoExc("Error calling FSSetForkPosition().");
}


Int64
IStreamPlatform::file_size()
{
	if(_refNum == 0)
		throw LogicExc("_refNum is 0.");
	
	SInt64 fork_size = 0;
	
	OSErr result = FSGetForkSize(_refNum, &fork_size);
	
	if(result != noErr)
		throw IoExc("Error calling FSGetForkSize().");
	
	return fork_size;
}


DateTime
IStreamPlatform::file_modtime()
{
	if(_refNum == 0)
		throw LogicExc("_refNum is 0.");
	
	FSCatalogInfo cat;

	OSErr result = FSGetCatalogInfo(&_fsRef, kFSCatInfoContentMod, &cat, NULL, NULL, NULL);
	
	if(result != noErr)
		throw IoExc("Error calling FSGetCatalogInfo().");
	
	return cat.contentModDate;
}


OStreamPlatform::OStreamPlatform(const char fileName[]):
	OStream(fileName)
{
	OSErr result = noErr;
	
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)fileName, strlen(fileName) + 1, FALSE);
	if(url == NULL)
		throw IoExc ("Couldn't make CFURLRef.");
	
	Boolean file_exists = CFURLGetFSRef(url, &_fsRef);
	CFRelease(url);
	
	if(!file_exists)
	{
		// means the file doesn't exist and we have to create it
		
		// find the last slash, splitting the directory from the file name
		int dir_name_len = 0;
		const char *file_name = NULL;
		int file_name_len = 0;
		
		int len = strlen(fileName);
		
		for(int i = (len - 2); i >= 0 && file_name == NULL; i--)
		{
			if(fileName[i] == '/')
			{
				dir_name_len = i;
				file_name = &fileName[i + 1];
				file_name_len = (len - i) - 1;
			}
		}
		
		if(file_name == NULL)
			throw IoExc("Error parsing path.");
		
		CFURLRef dir_url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)fileName, dir_name_len, TRUE);
		
		FSRef parent_fsRef;
		Boolean dir_success = CFURLGetFSRef(dir_url, &parent_fsRef);
		CFRelease(dir_url);
		
		if(dir_success)
		{
			UniChar u_fileName[256];
			
			// poor-man's unicode copy
			UniChar *u = u_fileName;
			for(int i=0; i < file_name_len; i++)
				*u++ = file_name[i];
			
			FSSpec my_fsSpec;
			result =  FSCreateFileUnicode(&parent_fsRef, file_name_len, u_fileName, kFSCatInfoNone, NULL, &_fsRef, &my_fsSpec);
			
			if(result != noErr)
				throw IoExc("Couldn't create new file.");
		}
		else
			throw IoExc("Couldn't make FSRef.");
	}
		
	
	HFSUniStr255 dataForkName;
	result = FSGetDataForkName(&dataForkName);

	result = FSOpenFork(&_fsRef, dataForkName.length, dataForkName.unicode, fsWrPerm, &_refNum);

	if(result != noErr)
		throw IoExc("Couldn't open file for writing.");
}


OStreamPlatform::OStreamPlatform(const uint16_t fileName[]):
	OStream("Unicode Path")
{
	OSErr result = noErr;
	
	const int len = PathString::StrLen(fileName);
	
	CFStringRef inStr = CFStringCreateWithCharacters(kCFAllocatorDefault, fileName, len);
	if(inStr == NULL)
		throw IoExc("Couldn't make CFStringRef.");
		
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, inStr, kCFURLPOSIXPathStyle, 0);
	CFRelease(inStr);
	if(url == NULL)
		throw IoExc("Couldn't make CFURLRef.");
	
	Boolean file_exists = CFURLGetFSRef(url, &_fsRef);
	CFRelease(url);
	
	if(!file_exists)
	{
		// find the last slash, splitting the directory from the file name
		int dir_name_len = 0;
		const uint16_t *file_name = NULL;
		int file_name_len = 0;
		
		for(int i = (len - 2); i >= 0 && file_name == NULL; i--)
		{
			if(fileName[i] == '/')
			{
				dir_name_len = i;
				file_name = &fileName[i + 1];
				file_name_len = (len - i) - 1;
			}
		}
		
		if(file_name == NULL)
			throw IoExc("Error parsing path.");

		CFStringRef dirStr = CFStringCreateWithCharacters(kCFAllocatorDefault, fileName, dir_name_len);
		if(dirStr == NULL)
			throw IoExc("Couldn't make CFStringRef.");

		CFURLRef dir_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, dirStr, kCFURLPOSIXPathStyle, 0);
		CFRelease(dirStr);
		if(dir_url == NULL)
			throw IoExc("Couldn't make CFURLRef.");

		FSRef parent_fsRef;
		Boolean dir_success = CFURLGetFSRef(dir_url, &parent_fsRef);
		CFRelease(dir_url);
		
		if(dir_success)
		{
			FSSpec my_fsSpec;
			result =  FSCreateFileUnicode(&parent_fsRef, file_name_len, file_name, kFSCatInfoNone, NULL, &_fsRef, &my_fsSpec);
			
			if(result != noErr)
				throw IoExc("Couldn't create new file.");
		}
		else
			throw IoExc("Couldn't make FSRef.");
	}
	
	HFSUniStr255 dataForkName;
	result = FSGetDataForkName(&dataForkName);

	result = FSOpenFork(&_fsRef, dataForkName.length, dataForkName.unicode, fsWrPerm, &_refNum);

	if(result != noErr)
		throw IoExc("Couldn't open file for reading.");
}


OStreamPlatform::~OStreamPlatform()
{
	OSErr result = FSCloseFork(_refNum);

	assert(result == noErr);
}


void
OStreamPlatform::write (const char c[/*n*/], int n)
{
	ByteCount count = n;

	OSErr result = FSWriteFork(_refNum, fsAtMark, 0, count, (const void *)c, &count);

	if(count != n || result != noErr)
		throw IoExc("Not able to write.");
}


Int64
OStreamPlatform::tellp ()
{
	Int64 pos;
	SInt64 lpos;

	OSErr result = FSGetForkPosition(_refNum, &lpos);
	
	if(result != noErr)
		throw IoExc("Error calling FSGetForkPosition().");

	pos = lpos;
	
	return pos;
}


void
OStreamPlatform::seekp (Int64 pos)
{
	OSErr result = FSSetForkPosition(_refNum, fsFromStart, pos);

	if(result != noErr)
		throw IoExc("Error calling FSSetForkPosition().");
}
#endif // __APPLE__

#ifdef WIN32
void
IStreamPlatform::open_file(const char fileName[])
{
	_hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if(_hFile == INVALID_HANDLE_VALUE)
		throw IoExc("Couldn't open file.");
}

void
IStreamPlatform::open_file(const uint16_t fileName[])
{
	_hFile = CreateFileW((LPCWSTR)fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if(_hFile == INVALID_HANDLE_VALUE)
		throw IoExc("Couldn't open file.");
}


void
IStreamPlatform::close_file()
{
	BOOL result = CloseHandle(_hFile);

	if(!result)
		throw IoExc("Error closing file.");
}


bool
IStreamPlatform::read_file(char c[/*n*/], int n)
{
	DWORD count = n, out = 0;
	
	BOOL result = ReadFile(_hFile, (LPVOID)c, count, &out, NULL);

	return (result && count == out);
}


Int64
IStreamPlatform::tellg_file()
{
	Int64 pos;
	LARGE_INTEGER lpos, zero;

	zero.QuadPart = 0;

	BOOL result = SetFilePointerEx(_hFile, zero, &lpos, FILE_CURRENT);

	if(!result)
		throw IoExc("Error calling SetFilePointerEx().");

	pos = lpos.QuadPart;
	
	return pos;
}


void
IStreamPlatform::seekg_file(Int64 pos)
{
	LARGE_INTEGER lpos, out;

	lpos.QuadPart = pos;

	BOOL result = SetFilePointerEx(_hFile, lpos, &out, FILE_BEGIN);

	if(!result || lpos.QuadPart != out.QuadPart)
		throw IoExc("Error calling SetFilePointerEx().");
}


Int64
IStreamPlatform::file_size()
{
	return GetFileSize(_hFile, NULL);
}


DateTime
IStreamPlatform::file_modtime()
{
	FILETIME fileTime;

	BOOL result = GetFileTime(_hFile, NULL, NULL, &fileTime);

	return fileTime;
}


OStreamPlatform::OStreamPlatform(const char fileName[]):
	OStream(fileName)
{
	_hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if(_hFile == INVALID_HANDLE_VALUE)
		throw IoExc("Couldn't open file.");
}


OStreamPlatform::OStreamPlatform(const uint16_t fileName[]):
	OStream("Unicode Path")
{
	_hFile = CreateFileW((LPCWSTR)fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if(_hFile == INVALID_HANDLE_VALUE)
		throw IoExc("Couldn't open file.");
}


OStreamPlatform::~OStreamPlatform()
{
	BOOL result = CloseHandle(_hFile);

	assert(result == TRUE);
}


void
OStreamPlatform::write (const char c[/*n*/], int n)
{
	DWORD count = n, out = 0;
	
	BOOL result = WriteFile(_hFile, (LPVOID)c, count, &out, NULL);

	if(!result || out != count)
		throw IoExc("Not able to write.");
}


Int64
OStreamPlatform::tellp ()
{
	Int64 pos;
	LARGE_INTEGER lpos, zero;

	zero.QuadPart = 0;

	BOOL result = SetFilePointerEx(_hFile, zero, &lpos, FILE_CURRENT);

	if(!result)
		throw IoExc("Error calling SetFilePointerEx().");

	pos = lpos.QuadPart;
	
	return pos;
}


void
OStreamPlatform::seekp (Int64 pos)
{
	LARGE_INTEGER lpos;

	lpos.QuadPart = pos;

	BOOL result = SetFilePointerEx(_hFile, lpos, NULL, FILE_BEGIN);

	if(!result)
		throw IoExc("Error calling SetFilePointerEx().");
}
#endif // WIN32


#pragma mark-


PathString::PathString()
{
	_path = new A_PathType[1];
	
	_path[0] = '\0';
}


PathString::PathString(const PathString &other)
{
	_path = new A_PathType[ StrLen(other.string()) + 1 ];
	
	StrCpy(_path, other.string());
}


PathString::PathString(const A_char *str)
{
#ifdef AE_UNICODE_PATHS
	// this AE_UNICODE_PATHS version is unlikely to ever get called,
	// but need to fill it in for linking purposes
	int utf8size = StrLen(str) + 1;
	
	utf16_char *temp = new utf16_char[utf8size];
	
	bool success = UTF8toUTF16(str, temp, utf8size);
	
	assert(success); // not really prepared for this to fail
	
	if(success)
	{
		_path = new A_PathType[ StrLen(temp) + 1 ];
		
		StrCpy(_path, temp);
	}
	else
	{
		_path = new A_PathType[1];
		
		_path[0] = '\0';
	}
	
	delete [] temp;
#else
	_path = new A_PathType[ StrLen(str) + 1 ];
	
	StrCpy(_path, str);
#endif
}


PathString::PathString(const A_u_short *str)
{
#ifdef AE_UNICODE_PATHS
	_path = new A_PathType[ StrLen(str) + 1 ];
	
	StrCpy(_path, str);
#else
	std::string utf8string = UTF16toUTF8(str);
	
	_path = new A_PathType[ utf8string.size() + 1 ];
	
	StrCpy(_path, utf8string.c_str());
#endif
}


PathString::~PathString()
{
	delete [] _path;
}


PathString &
PathString::operator = (const PathString &other)
{
	if(this != &other)
	{
		delete [] _path;
		
		_path = new A_PathType[ StrLen(other.string()) + 1 ];
		
		StrCpy(_path, other.string());
	}
	
	return *this;
}


bool
PathString::operator == (const PathString &other) const
{
	bool match = true;
	
	const A_PathType *s1 = _path;
	const A_PathType *s2 = other.string();
	
	do{
		if(*s1 != *s2++)
			match = false;
			
	}while(match && *s1++ != '\0');
	
	return match;
}


template <typename T>
int
PathString::StrLen(const T *str)
{
	int len = 0;
	
	while(*str++ != '\0')
		len++;
	
	return len;
}


template <typename T>
void
PathString::StrCpy(T *ostr, const T *istr)
{
	do{
		*ostr++ = *istr;
	}while(*istr++ != '\0');
}
