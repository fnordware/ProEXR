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


#include <ImfAttribute.h>
#include <ImfHeader.h>

class iccProfileAttribute : public Imf::Attribute
{
  public:
	iccProfileAttribute();
	iccProfileAttribute(void *profile, size_t size);
	virtual ~iccProfileAttribute();
	
	const void *value(size_t &size) const;
	
	virtual const char *typeName() const;
	static const char *staticTypeName();
	
	virtual Imf::Attribute *copy() const;
	
	virtual void writeValueTo(Imf::OStream &os, int version) const;
	virtual void readValueFrom (Imf::IStream &is, int size, int version);
	
	virtual void copyValueFrom(const Imf::Attribute &other);
	
	static Imf::Attribute *makeNewAttribute();
	static void registerAttributeType();
	
  protected:

  private:
	void *_profile;
	size_t _size;
};


void addICCprofile(Imf::Header &header, const iccProfileAttribute &value);
void addICCprofile(Imf::Header &header, void *profile, size_t size);
bool hasICCprofile(const Imf::Header &header);
const iccProfileAttribute & ICCprofileAttribute(const Imf::Header &header);
iccProfileAttribute & ICCprofileAttribute(Imf::Header &header);