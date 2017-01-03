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



#include "iccProfileAttribute.h"

using namespace Imf;


iccProfileAttribute::iccProfileAttribute()
{
	_profile = NULL;
	_size = 0;
}

iccProfileAttribute::iccProfileAttribute(void *profile, size_t size)
{
	_profile = malloc(_size = size);
	memcpy(_profile, profile, size);
}

iccProfileAttribute::~iccProfileAttribute()
{
	if(_profile)
	{
		free(_profile);
		_profile = NULL;
		_size = 0;
	}
}

const void *
iccProfileAttribute::value(size_t &size) const
{
	size = _size;
	return _profile;
}

const char *
iccProfileAttribute::typeName() const
{
	return staticTypeName();
}

const char *
iccProfileAttribute::staticTypeName()
{
	return "iccProfile";
}

Attribute *
iccProfileAttribute::copy() const
{
	return new iccProfileAttribute(_profile, _size);
}

void
iccProfileAttribute::writeValueTo(OStream &os, int version) const
{
	if(_profile)
		Xdr::write<StreamIO>(os, (char *)_profile, _size);
	else
		throw Iex::NullExc("Profile not initialized.");
}

void
iccProfileAttribute::readValueFrom(IStream &is, int size, int version)
{
	if(_profile)
		free(_profile);
	
	_profile = malloc(_size = size);
	
	Xdr::read<StreamIO>(is, (char *)_profile, size);
}

void
iccProfileAttribute::copyValueFrom(const Attribute &other)
{
	const iccProfileAttribute &o = dynamic_cast<const iccProfileAttribute &>( other );
	
	if(_profile)
		free(_profile);
	
	if(o._profile && o._size)
	{
		_profile = malloc(_size = o._size);
		memcpy(_profile, o._profile, o._size);
	}
	else
	{
		_profile = NULL;
		_size = 0;
	}
}

Attribute *
iccProfileAttribute::makeNewAttribute()
{
	return new iccProfileAttribute();
}

void
iccProfileAttribute::registerAttributeType()
{
	Attribute::registerAttributeType(staticTypeName(), makeNewAttribute);
}


#define STD_ICC_PROFILE_NAME	"iccProfile"

void
addICCprofile(Header &header, const iccProfileAttribute &value)
{
	header.insert(STD_ICC_PROFILE_NAME, value);
}

void
addICCprofile(Header &header, void *profile, size_t size)
{
	addICCprofile(header, iccProfileAttribute(profile, size));
}

bool
hasICCprofile(const Header &header)
{
	return header.findTypedAttribute<iccProfileAttribute>(STD_ICC_PROFILE_NAME) != 0;
}

const iccProfileAttribute &
ICCprofileAttribute(const Header &header)
{
	return header.typedAttribute<iccProfileAttribute>(STD_ICC_PROFILE_NAME);
}

iccProfileAttribute &
ICCprofileAttribute(Header &header)
{
	return header.typedAttribute <iccProfileAttribute> (STD_ICC_PROFILE_NAME);
}

