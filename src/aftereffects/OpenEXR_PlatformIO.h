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

#ifndef OPENEXR_PLATFORM_IO_H
#define OPENEXR_PLATFORM_IO_H

#include <ImfIO.h>

#include "fnord_SuiteHandler.h"


#ifdef WIN32
#include <Windows.h>
typedef unsigned short uint16_t;
typedef FILETIME DateTime;
#endif

#ifdef __APPLE__
#ifndef MAC_OS_X_VERSION_10_5
#define MAC_OS_X_VERSION_10_5 1050
#endif

typedef UTCDateTime	DateTime;

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5
typedef SInt16 FSIORefNum;
#endif
#endif // __APPLE__


class PathString
{
  public:
	PathString();
	PathString(const PathString &other);
	PathString(const A_char *str);
	PathString(const A_u_short *str);
	~PathString();
	
	PathString & operator = (const PathString &other);
	
	bool operator == (const PathString &other) const;
	bool operator != (const PathString &other) const { return !(*this == other); }
	
	const A_PathType *string() const { return _path; }

	template <typename T>
	static int StrLen(const T *str);
	
	template <typename T>
	static void StrCpy(T *ostr, const T *istr);
	
  private:
	A_PathType *_path;
};


void DeleteFileCache(const SPBasicSuite *pica_basicP, int timeout=0);


class IStreamPlatform : public Imf::IStream
{
  public:
  
	IStreamPlatform(const char fileName[], const SPBasicSuite *pica_basicP=NULL);
	IStreamPlatform(const uint16_t fileName[], const SPBasicSuite *pica_basicP=NULL);
	~IStreamPlatform();
	
	
	virtual bool isMemoryMapped() const;
	virtual bool read(char c[/*n*/], int n);
	virtual char *readMemoryMapped(int n);
	virtual Imf::Int64 tellg();
	virtual void seekg(Imf::Int64 pos);
	
	// function to load the file into a buffer and start memory mapping
	void memoryMap();
	void unMemoryMap();
	
	// access information relevant to caching
	const PathString & getPath() const { return _path; }
	DateTime getModTime() const { return _modtime; }
	
  private:
	void adopt_cache();
	void release_cache();

  private:
	void open_file(const char fileName[]);
	void open_file(const uint16_t fileName[]);
	void close_file();
	bool read_file(char c[/*n*/], int n);
	Imf::Int64 tellg_file();
	void seekg_file(Imf::Int64 pos);
	Imf::Int64 file_size();
	DateTime file_modtime();
  
  
  private:
	const SPBasicSuite *_pica_basicP;
	void *_vfile;
	Imf::Int64 _voffset;
	Imf::Int64 _vsize;

	PathString _path;
	DateTime _modtime;
	
#ifdef __APPLE__
	FSRef _fsRef;
	FSIORefNum _refNum;
#endif

#ifdef WIN32
	HANDLE _hFile;
#endif
};


class OStreamPlatform : public Imf::OStream
{
  public:
  
	OStreamPlatform(const char fileName[]);
	OStreamPlatform(const uint16_t fileName[]);
	~OStreamPlatform();
	
	void write (const char c[/*n*/], int n);
	Imf::Int64 tellp ();
	void seekp (Imf::Int64 pos);

  private:
	
#ifdef __APPLE__
	FSRef _fsRef;
	FSIORefNum _refNum;
#endif

#ifdef WIN32
	HANDLE _hFile;
#endif

};

#endif // OPENEXR_PLATFORM_IO_H