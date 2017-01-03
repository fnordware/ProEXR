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

#ifndef __ProEXR_PSIO_H__
#define __ProEXR_PSIO_H__

#include <ImfIO.h>


#ifdef WIN32
#include <Windows.h>
#endif


#ifdef __APPLE__
#include <CoreServices/CoreServices.h>

#ifndef MAC_OS_X_VERSION_10_5
#define MAC_OS_X_VERSION_10_5 1050
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5
typedef unsigned short FSIORefNum;
#endif
#endif // __APPLE__


class IStreamPS : public Imf::IStream
{
  public:
  
	IStreamPS(int refNum, const char fileName[]);
	virtual ~IStreamPS();
	
	virtual bool isMemoryMapped() const;
	virtual bool read(char c[/*n*/], int n);
	virtual char *readMemoryMapped(int n);
	virtual Imf::Int64 tellg();
	virtual void seekg(Imf::Int64 pos);
	
	// function to load the file into a buffer and start memory mapping
	void memoryMap();
	void unMemoryMap();
	
  private:
	Imf::Int64 file_size();
	bool read_file(char c[/*n*/], int n);
	Imf::Int64 tellg_file();
	void seekg_file(Imf::Int64 pos);
	
  
  private:
	void *_vfile;
	Imf::Int64 _voffset;
	Imf::Int64 _vsize;
  
#ifdef __APPLE__
	FSIORefNum _refNum;
#endif

#ifdef WIN32
	HANDLE _hFile;
#endif
};


class OStreamPS : public Imf::OStream
{
  public:
  
	OStreamPS(int refNum, const char fileName[]);
	virtual ~OStreamPS() {}
	
	virtual void write(const char c[/*n*/], int n);
	virtual Imf::Int64 tellp();
	virtual void seekp(Imf::Int64 pos);

  private:
#ifdef __APPLE__
	FSIORefNum _refNum;
#endif

#ifdef WIN32
	HANDLE _hFile;
#endif
};

#endif // __ProEXR_PSIO_H__

