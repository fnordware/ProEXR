
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

#ifndef INCLUDED_IMF_HYBRID_INPUT_FILE_H
#define INCLUDED_IMF_HYBRID_INPUT_FILE_H

#include "ImfHeader.h"
#include "ImfMultiPartInputFile.h"
#include "ImfFrameBuffer.h"
#include "ImfChannelList.h"
#include "ImathBox.h"


OPENEXR_IMF_INTERNAL_NAMESPACE_HEADER_ENTER


class IMF_EXPORT HybridInputFile : public GenericInputFile
{
  public:
	HybridInputFile(const char fileName[], bool renameFirstPart = false,
					int numThreads = globalThreadCount(),
					bool reconstructChunkOffsetTable = true);
					
	HybridInputFile(IStream& is, bool renameFirstPart = false,
					int numThreads = globalThreadCount(),
					bool reconstructChunkOffsetTable = true);

	virtual ~HybridInputFile() {}
	
	
	int parts() const { return _multiPart.parts(); }

	const Header &  header(int n) const { return _multiPart.header(n); }
	
	std::string partPrefix(int n) const;
	
	int			    version () const { return _multiPart.version(); }
	
	bool		isComplete () const;
	
	const ChannelList &		channels () const { return _chanList; }
	
	const IMATH_NAMESPACE::Box2i & dataWindow() const { return _dataWindow; }
	const IMATH_NAMESPACE::Box2i & displayWindow() const { return _displayWindow; }
	
	
	void		setFrameBuffer (const FrameBuffer &frameBuffer) { _frameBuffer = frameBuffer; }
	
	const FrameBuffer &	frameBuffer () const { return _frameBuffer; }
	
    void		readPixels (int scanLine1, int scanLine2);
    void		readPixels (int scanLine) { readPixels(scanLine, scanLine); }
	
  private:
	void setup();

  private:
	MultiPartInputFile _multiPart;
	
	const bool _renameFirstPart;
	
	IMATH_NAMESPACE::Box2i _dataWindow;
	IMATH_NAMESPACE::Box2i _displayWindow;
	
	FrameBuffer		_frameBuffer;
	
	typedef struct HybridChannel {
		int part;
		std::string name;
		
		HybridChannel(int p=0, const std::string &n="") : part(p), name(n) {}
	}HybridChannel;
	
	typedef std::map<std::string, HybridChannel> HybridChannelMap;
	
	HybridChannelMap _map;
	
	ChannelList _chanList;
};


OPENEXR_IMF_INTERNAL_NAMESPACE_HEADER_EXIT

#endif // INCLUDED_IMF_HYBRID_INPUT_FILE_H