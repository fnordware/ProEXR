// Copyright 2007-2017 Brendan Bolles (http://www.fnordware.com)
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
// WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "ImfHybridInputFile.h"

#include "ImfInputPart.h"
#include "ImfPartType.h"

#include "Iex.h"


OPENEXR_IMF_INTERNAL_NAMESPACE_SOURCE_ENTER


using namespace std;
using IMATH_NAMESPACE::Box2i;


HybridInputFile::HybridInputFile(const char fileName[], bool renameFirstPart, int numThreads, bool reconstructChunkOffsetTable) :
	_multiPart(fileName, numThreads, reconstructChunkOffsetTable),
	_renameFirstPart(renameFirstPart)
{
	setup();
}


HybridInputFile::HybridInputFile(IStream& is, bool renameFirstPart, int numThreads, bool reconstructChunkOffsetTable) :
	_multiPart(is, numThreads, reconstructChunkOffsetTable),
	_renameFirstPart(renameFirstPart)
{
	setup();
}


bool
HybridInputFile::isComplete() const
{
	for(int i=0; i < _multiPart.parts(); i++)
	{
		if( !_multiPart.partComplete(i) )
			return false;
	}
	
	return true;
}


void
HybridInputFile::readPixels(int scanLine1, int scanLine2)
{
	for(int n=0; n < _multiPart.parts(); n++)
	{
		FrameBuffer part_fb;
	
		for(FrameBuffer::ConstIterator i = _frameBuffer.begin(); i != _frameBuffer.end(); i++)
		{
			if( _map.find( i.name() ) != _map.end() )
			{
				const HybridChannel &hyChan = _map[ i.name() ];
				
				if(hyChan.part == n)
				{
					part_fb.insert( hyChan.name, i.slice() );
				}
			}
			else if(n == 0)
			{
				// for channels that will be simply be filled
				const bool rename = (_multiPart.parts() > 1);
				
				const string name_never_loaded = (rename ? string("zzNOLOADzz") + i.name() : i.name());
				
				part_fb.insert( name_never_loaded, i.slice() );
			}
		}
		
		if(part_fb.begin() != part_fb.end()) // i.e. it's not empty
		{
			const Box2i &dataW = _multiPart.header(n).dataWindow();
			
			const int startScanline = max(scanLine1, dataW.min.y);
			const int endScanline = min(scanLine2, dataW.max.y);
			
			if(endScanline >= startScanline)
			{
				InputPart inPart(_multiPart, n);
				
				inPart.setFrameBuffer(part_fb);
				
				inPart.readPixels(startScanline, endScanline);
			}
		}
	}
}


void
HybridInputFile::setup()
{
	for(int n=0; n < _multiPart.parts(); n++)
	{
		const Header &head = _multiPart.header(n);
		
		if(head.type() != OPENEXR_IMF_INTERNAL_NAMESPACE::DEEPTILE)
		{
			// this will make a dataWindow that can hold the dataWindows of every part
			_dataWindow.extendBy( head.dataWindow() );
			
			// all displayWindows should be the same, actually
			_displayWindow.extendBy( head.displayWindow() );
			
			
			const ChannelList &chans = head.channels();

			for(ChannelList::ConstIterator i = chans.begin(); i != chans.end(); ++i)
			{
				const bool rename = (_multiPart.parts() > 1) && (n > 0 || _renameFirstPart) && head.hasName();
				
				const string hybrid_name = (rename ? head.name() + "." + i.name() : i.name());
				
				_map[ hybrid_name ] = HybridChannel(n, i.name());
				
				_chanList.insert(hybrid_name, i.channel());
			}
		}
	}
	
	if(_chanList.begin() == _chanList.end()) // empty
		throw IEX_NAMESPACE::BaseExc("DeepTile images not supported");  // only reason this should happen
}


OPENEXR_IMF_INTERNAL_NAMESPACE_SOURCE_EXIT