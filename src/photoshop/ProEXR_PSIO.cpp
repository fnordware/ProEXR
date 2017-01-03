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


#include "ProEXR_PSIO.h"

#include <IexBaseExc.h>

#include <assert.h>

using namespace Imf;
using namespace Iex;


bool
IStreamPS::isMemoryMapped() const
{
	return (_vfile != NULL);
}


bool
IStreamPS::read(char c[/*n*/], int n)
{
	if( isMemoryMapped() )
	{
		bool success = true;
		
		try {
			memcpy(c, readMemoryMapped(n), n);
		}
		catch(...) { success = false; }
		
		return success;
	}
	else
		return read_file(c, n);
}

char *
IStreamPS::readMemoryMapped(int n)
{
	assert(isMemoryMapped() && _vfile != NULL);

	if( !isMemoryMapped() )
		throw LogicExc("readMemoryMapped() called for non memory-mapped file.");
	
	if(n > (_vsize - _voffset))
		throw IoExc("Trying to read past the end of a memory-mapped file.");
	
	char *ptr = ((char *)_vfile + _voffset);
	
	_voffset += n;
	
	return ptr;
}


Int64
IStreamPS::tellg()
{
	if( isMemoryMapped() )
	{
		return _voffset;
	}
	else
		return tellg_file();
}


void
IStreamPS::seekg(Int64 pos)
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
IStreamPS::memoryMap()
{
	assert( !isMemoryMapped() );
	
	if( !isMemoryMapped() )
	{
		_vsize = file_size();
		
		_vfile = malloc(_vsize);
		
		if(_vfile)
		{
			_voffset = tellg_file();
			
			seekg_file(0);
			
			bool result = read_file((char *)_vfile, _vsize);
			
			if(!result)
				unMemoryMap();
			
			seekg_file(_voffset);
		}
	}
}


void
IStreamPS::unMemoryMap()
{
	if( isMemoryMapped() )
	{
		//seekg_file(_voffset); // use if you want to still access the file
		
		free(_vfile);
		
		_vfile = NULL;
	}
}


#ifdef __APPLE__

IStreamPS::IStreamPS(int refNum, const char fileName[]):
	IStream(fileName),
	_vfile(NULL)
{
	_refNum = refNum;
	
	seekg(0); // start from the beginning of the file
}


IStreamPS::~IStreamPS()
{
	unMemoryMap();
	
	seekg_file(0); // back to the beginning
}


Int64
IStreamPS::file_size()
{
	SInt64 fork_size = 0;
	
	OSErr result = FSGetForkSize(_refNum, &fork_size);
	
	if(result != noErr)
		throw IoExc("Error calling FSGetForkSize().");
	
	return fork_size;
}


bool
IStreamPS::read_file(char c[/*n*/], int n)
{
	ByteCount count = n;
	
	OSErr result = FSReadFork(_refNum, fsAtMark, 0, count, (void *)c, &count);
	
	return (result == noErr && count == n);
}


Int64
IStreamPS::tellg_file()
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
IStreamPS::seekg_file(Int64 pos)
{
	OSErr result = FSSetForkPosition(_refNum, fsFromStart, pos);

	if(result != noErr)
		throw IoExc("Error calling FSSetForkPosition().");
}


OStreamPS::OStreamPS(int refNum, const char fileName[]) :
	OStream(fileName)
{
	_refNum = refNum;
}

void
OStreamPS::write(const char c[/*n*/], int n)
{
	ByteCount count = n;

	OSErr result = FSWriteFork(_refNum, fsAtMark, 0, count, (const void *)c, &count);

	if(count != n || result != noErr)
		throw IoExc("Not able to write.");
}


Int64
OStreamPS::tellp()
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
OStreamPS::seekp(Int64 pos)
{
	OSErr result = FSSetForkPosition(_refNum, fsFromStart, pos);

	if(result != noErr)
		throw IoExc("Error calling FSSetForkPosition().");
}

#endif // __APPLE__


#ifdef WIN32

IStreamPS::IStreamPS(int refNum, const char fileName[]):
	IStream(fileName),
	_vfile(NULL)
{
	_hFile = (HANDLE)refNum;
	
	seekg(0); // start from the beginning of the file
}


IStreamPS::~IStreamPS()
{
	unMemoryMap();
	
	seekg_file(0); // back to the beginning
}


Int64
IStreamPS::file_size()
{
	return GetFileSize(_hFile, NULL);
}


bool
IStreamPS::read_file(char c[/*n*/], int n)
{
	DWORD count = n, out = 0;
	
	BOOL result = ReadFile(_hFile, (LPVOID)c, count, &out, NULL);
	
	return (result && (out == n));
}


Int64
IStreamPS::tellg_file()
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
IStreamPS::seekg_file(Int64 pos)
{
	LARGE_INTEGER lpos, out;

	lpos.QuadPart = pos;

	BOOL result = SetFilePointerEx(_hFile, lpos, &out, FILE_BEGIN);

	if(!result || lpos.QuadPart != out.QuadPart)
		throw IoExc("Error calling SetFilePointerEx().");
}

	
OStreamPS::OStreamPS(int refNum, const char fileName[]) :
	OStream(fileName)
{
	_hFile = (HANDLE)refNum;
}


void
OStreamPS::write(const char c[/*n*/], int n)
{
	DWORD count = n, out = 0;
	
	BOOL result = WriteFile(_hFile, (LPVOID)c, count, &out, NULL);

	if(!result || out != count)
		throw IoExc("Not able to write.");
}


Int64
OStreamPS::tellp()
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
OStreamPS::seekp(Int64 pos)
{
	LARGE_INTEGER lpos;

	lpos.QuadPart = pos;

	BOOL result = SetFilePointerEx(_hFile, lpos, NULL, FILE_BEGIN);

	if(!result)
		throw IoExc("Error calling SetFilePointerEx().");
}

#endif // WIN32
