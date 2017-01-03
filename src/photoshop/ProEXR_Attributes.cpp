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


#include "ProEXR_Attributes.h"

#include <ImfChannelList.h>
#include <ImfFloatAttribute.h>
#include <ImfDoubleAttribute.h>
#include <ImfIntAttribute.h>
#include <ImfBoxAttribute.h>
#include <ImfVecAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfLineOrderAttribute.h>
#include <ImfTileDescriptionAttribute.h>
#include <ImfEnvmapAttribute.h>
#include <ImfStandardAttributes.h>
#include <ImfStringVectorAttribute.h>
#include <ImfFloatVectorAttribute.h>

#ifdef __APPLE__
	#define MAC_ENV 1
	#include <Carbon.h>
#else
	#define WIN_ENV 1
	#include <Windows.h>
	#include <sys/timeb.h>
#endif

#include <time.h>

using namespace Imf;
using namespace Imath;
using namespace std;

void AddExtraAttributes(Header &header)
{
	// add time attributes
	time_t the_time = time(NULL);
	tm *local_time = localtime(&the_time);
	
	if(local_time)
	{
		char date_string[256];
		sprintf(date_string, "%04d:%02d:%02d %02d:%02d:%02d", local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
													local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
		
		addCapDate(header, string(date_string) );
#ifdef WIN_ENV
		_timeb win_time;
		_ftime(&win_time);
		addUtcOffset(header, (float)( (win_time.timezone - (win_time.dstflag ? 60 : 0) ) & 60));
#else
		addUtcOffset(header, (float)-local_time->tm_gmtoff);
#endif
	}
	
#ifdef EMBED_PERSONAL_INFO

#define COMPUTER_NAME_SIZE	256
	char computer_name_str[COMPUTER_NAME_SIZE];
	char user_name_str[COMPUTER_NAME_SIZE];

#ifdef MAC_ENV
	// user and computer name
	CFStringRef user_name = CSCopyUserName(FALSE);  // set this to TRUE for shorter unix-style name
	CFStringRef comp_name = CSCopyMachineName();
	
	if(user_name)
	{
		CFStringGetCString(user_name, user_name_str, COMPUTER_NAME_SIZE-1, kCFStringEncodingMacRoman);
		addOwner(header, string(user_name_str) );
		CFRelease(user_name);
	}
	
	if(comp_name)
	{
		CFStringGetCString(comp_name, computer_name_str, COMPUTER_NAME_SIZE-1, kCFStringEncodingMacRoman);
		header.insert("computerName", StringAttribute( string(computer_name_str) ) );
		CFRelease(comp_name);
	}
#else // WIN_ENV
	DWORD buf_size = COMPUTER_NAME_SIZE-1;

	if( GetComputerName(computer_name_str, &buf_size) )
		header.insert("computerName", StringAttribute( string(computer_name_str) ) );

	buf_size = COMPUTER_NAME_SIZE-1;

	if( GetUserName(user_name_str, &buf_size) )
		addOwner(header, string(user_name_str) );
#endif

#endif // EMBED_PERSONAL_INFO
}

static void searchReplace(string &s, const string &search, const string &replace)
{
	// locate the search strings
	vector<string::size_type> positions;

	string::size_type last_pos = 0;

	while(last_pos != string::npos && last_pos < s.size())
	{
		last_pos = s.find(search, last_pos);

		if(last_pos != string::npos)
		{
			positions.push_back(last_pos);
		
			last_pos += search.size();
		}
	}

	// replace with the replace string, starting from the end
	int i = positions.size();

	while(i--)
	{
		s.erase(positions[i], search.size());
		s.insert(positions[i], replace);
	}
}

static void fixForXMP(string &val)
{
	searchReplace(val, "&", "&amp;");
	searchReplace(val, "<", "&lt;");
	searchReplace(val, ">", "&gt;");
}

template <class AttrType>
static void PrintMatrixValue(ostringstream &s, const Attribute &attrib, int n)
{
	const AttrType &a = dynamic_cast<const AttrType &>(attrib);
	
	s << ": ";
	
	for(int r=0; r < n; r++)
	{
		if(r == 0)
			s << "[";
		else
			s << ", ";
		
		for(int c=0; c < n; c++)
		{
			if(c == 0)
				s << "{";
			else
				s << ", ";
			
			s << a.value().x[c][r];
			
			if(c == (n-1))
				s << "}";
		}
		
		if(r == (n-1))
			s << "]";
	}
}

static void PrintAttributeValue(string &desc, const Attribute &attrib)
{
	const string &typeName = attrib.typeName();

	ostringstream s;

	// for the attributes we know, cat the value onto desc
	// using dynamic_cast - don't forget RTTI (actually OpenEXR doesn't work at all without RTTI)
	if(typeName == "string")
	{
		const StringAttribute &a = dynamic_cast<const StringAttribute &>(attrib);
		
		string val = a.value();

		fixForXMP(val);
		
		s << ": \"" << val << "\"";
	}
	else if(typeName == "float")
	{
		const FloatAttribute &a = dynamic_cast<const FloatAttribute &>(attrib);
		
		s << ": " << a.value();
	}
	else if(typeName == "double")
	{
		const DoubleAttribute &a = dynamic_cast<const DoubleAttribute &>(attrib);
		
		s << ": " << a.value();
	}
	else if(typeName == "int")
	{
		const IntAttribute &a = dynamic_cast<const IntAttribute &>(attrib);
		
		s << ": " << a.value();
	}
	else if(typeName == "rational")
	{
		const RationalAttribute &a = dynamic_cast<const RationalAttribute &>(attrib);
		
		s << ": " << a.value().n << " / " << a.value().d;
	}
	else if(typeName == "box2i")
	{
		const Box2iAttribute &a = dynamic_cast<const Box2iAttribute &>(attrib);
		
		s << ": [" <<
			a.value().min.x << ", " <<
			a.value().min.y << ", " <<
			a.value().max.x << ", " <<
			a.value().max.y <<
		"]";
	}
	else if(typeName == "box2f")
	{
		const Box2fAttribute &a = dynamic_cast<const Box2fAttribute &>(attrib);
		
		s << ": [" <<
			a.value().min.x << ", " <<
			a.value().min.y << ", " <<
			a.value().max.x << ", " <<
			a.value().max.y <<
		"]";
	}
	else if(typeName == "v2i")
	{
		const V2iAttribute &a = dynamic_cast<const V2iAttribute &>(attrib);
		
		s << ": [" << a.value().x << ", " << a.value().y << "]";
	}
	else if(typeName == "v2f")
	{
		const V2fAttribute &a = dynamic_cast<const V2fAttribute &>(attrib);
		
		s << ": [" << a.value().x << ", " << a.value().y << "]";
	}
	else if(typeName == "v2d")
	{
		const V2dAttribute &a = dynamic_cast<const V2dAttribute &>(attrib);
		
		s << ": [" << a.value().x << ", " << a.value().y << "]";
	}
	else if(typeName == "v3i")
	{
		const V3iAttribute &a = dynamic_cast<const V3iAttribute &>(attrib);
		
		s << ": [" << a.value().x << ", " << a.value().y << ", " << a.value().z << "]";
	}
	else if(typeName == "v3f")
	{
		const V3fAttribute &a = dynamic_cast<const V3fAttribute &>(attrib);
		
		s << ": [" << a.value().x << ", " << a.value().y << ", " << a.value().z << "]";
	}
	else if(typeName == "v3d")
	{
		const V3dAttribute &a = dynamic_cast<const V3dAttribute &>(attrib);
		
		s << ": [" << a.value().x << ", " << a.value().y << ", " << a.value().z << "]";
	}
	else if(typeName == "m33f")
	{
		PrintMatrixValue<M33fAttribute>(s, attrib, 3);
	}
	else if(typeName == "m33d")
	{
		PrintMatrixValue<M33dAttribute>(s, attrib, 3);
	}
	else if(typeName == "m44f")
	{
		PrintMatrixValue<M44fAttribute>(s, attrib, 4);
	}
	else if(typeName == "m44d")
	{
		PrintMatrixValue<M44dAttribute>(s, attrib, 4);
	}
	else if(typeName == "compression")
	{
		const CompressionAttribute &a = dynamic_cast<const CompressionAttribute &>(attrib);
		
		s << ": ";
		
		switch( a.value() )
		{
			case NO_COMPRESSION:		s << "None";		break;
			case RLE_COMPRESSION:		s << "RLE";			break;
			case ZIPS_COMPRESSION:		s << "Zip";			break;
			case ZIP_COMPRESSION:		s << "Zip16";		break;
			case PIZ_COMPRESSION:		s << "Piz";			break;
			case PXR24_COMPRESSION:		s << "PXR24";		break;
			case B44_COMPRESSION:		s << "B44";			break;
			case B44A_COMPRESSION:		s << "B44A";		break;
			case DWAA_COMPRESSION:		s << "DWAA";		break;
			case DWAB_COMPRESSION:		s << "DWAB";		break;
		}
	}
	else if(typeName == "lineOrder")
	{
		const LineOrderAttribute &a = dynamic_cast<const LineOrderAttribute &>(attrib);
		
		s << ": ";
		
		switch( a.value() )
		{
			case INCREASING_Y:		s << "Increasing Y";			break;
			case DECREASING_Y:		s << "Decreasing Y";			break;
			case RANDOM_Y:			s << "Random Y";				break;
		}
	}
	else if(typeName == "chromaticities")
	{
		const ChromaticitiesAttribute &a = dynamic_cast<const ChromaticitiesAttribute &>(attrib);
		
		s << ": " <<
			"r(" << a.value().red.x << ", " << a.value().red.y << ") " <<
			"g(" << a.value().green.x << ", " << a.value().green.y << ") " <<
			"b(" << a.value().blue.x << ", " << a.value().blue.y << ") " <<
			"w(" << a.value().white.x << ", " << a.value().white.y << ") ";
	}
	else if(typeName == "tiledesc")
	{
		const TileDescriptionAttribute &a = dynamic_cast<const TileDescriptionAttribute &>(attrib);
		
		s << ": [" << a.value().xSize << ", " << a.value().ySize << "]";
		
		if(a.value().mode == MIPMAP_LEVELS)
			s << ", MipMap";
		else if(a.value().mode == RIPMAP_LEVELS)
			s << ", RipMap";
	}
	else if(typeName == "envmap")
	{
		const EnvmapAttribute &a = dynamic_cast<const EnvmapAttribute &>(attrib);
		
		if(a.value() == ENVMAP_LATLONG)
			s << ": Lat-Long";
		else if(a.value() == ENVMAP_CUBE)
			s << ": Cube";
	}
	else if(typeName == "stringvector")
	{
		const StringVectorAttribute &a = dynamic_cast<const StringVectorAttribute &>(attrib);
		
		s << ": [";
		
		bool first_one = true;

		for(StringVector::const_iterator i = a.value().begin(); i != a.value().end(); ++i)
		{
			if(first_one)
				first_one = false;
			else
				s << ", ";
				
			string val = *i;
			
			fixForXMP(val);
				
			s << "\"" << val << "\"";
		}

		s << "]";
	}
	else if(typeName == "floatvector")
	{
		const FloatVectorAttribute &a = dynamic_cast<const FloatVectorAttribute &>(attrib);
		
		s << ": [";
		
		bool first_one = true;

		for(FloatVector::const_iterator i = a.value().begin(); i != a.value().end(); ++i)
		{
			if(first_one)
				first_one = false;
			else
				s << ", ";
				
			s << *i;
		}

		s << "]";
	}
	else if(typeName == "timecode")
	{
		const TimeCodeAttribute &a = dynamic_cast<const TimeCodeAttribute &>(attrib);
		
		const TimeCode &v = a.value();
		
		const string sep = (v.dropFrame() ? ";" : ":");
		
		s << ": ";
		
		s << v.hours() << sep;
		s << setfill('0') << setw(2) << v.minutes() << sep;
		s << setfill('0') << setw(2) << v.seconds() << sep;
		s << setfill('0') << setw(2) << v.frame();
		
		if( v.colorFrame() )
			s << ", Color";
		
		if( v.fieldPhase() )
			s << ", Field";
		
		if( v.bgf0() )
			s << ", bgf0";
			
		if( v.bgf1() )
			s << ", bgf1";

		if( v.bgf2() )
			s << ", bgf2";
		
		for(int i=1; i <= 8; i++)
		{
			if(v.binaryGroup(i) != 0)
				s << ", Group " << i << ": " << v.binaryGroup(i);
		}
	}
	else if(typeName == "keycode")
	{
		const KeyCodeAttribute &a = dynamic_cast<const KeyCodeAttribute &>(attrib);
		
		const KeyCode &v = a.value();
		
		s << ": ";
		
		s << "filmMfcCode: " << v.filmMfcCode() << " ";
		s << "filmType: " << v.filmType() << " ";
		s << "prefix: " << v.prefix() << " ";
		s << "count: " << v.count() << " ";
		s << "perfOffset: " << v.perfOffset() << " ";
		s << "perfsPerFrame: " << v.perfsPerFrame() << " ";
		s << "perfsPerCount: " << v.perfsPerCount() << " ";
	}
	
	
	desc += s.str();
}

static void AddDescription(string &desc, const ProEXRdoc_read &file)
{
	const string newline("&#xA;");
	
	// header
	desc += "ProEXR File Description" + newline + newline;
	
	int parts = file.parts();
	
	for(int i=0; i < parts; i++)
	{
		if(parts > 1)
		{
			if(i != 0)
				desc += newline + newline;
			
			stringstream s;
			s << "++Part " << (i + 1) << "++";
			
			desc += s.str() + newline + newline;
		}
		
		const Header &head = file.header(i);
		
		// Attribtes
		desc += "=Attributes=" + newline;
		
		for (Header::ConstIterator j = head.begin(); j != head.end(); ++j)
		{
			const Attribute &attrib = j.attribute();
			
			string attr_type(attrib.typeName());
			string attr_name(j.name());
			
			
			desc += attr_name + " (" + attr_type + ")";
			
			// print out attribute values if we know how
			PrintAttributeValue(desc, attrib);
			
			desc += newline;
		}

			
		// Channels
		desc += newline + "=Channels=" + newline;
		
		const ChannelList &channels = head.channels();

		for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
		{
			// things we'll want to know about the channels
			string channel_name(i.name());
			
			Channel channel = i.channel();
			Imf::PixelType pix_type = i.channel().type;
			
			fixForXMP(channel_name);

			desc += channel_name;
			
			if(pix_type == Imf::HALF)
				desc += " (half)" + newline;
			else if(pix_type == Imf::FLOAT)
				desc += " (float)" + newline;
			else if(pix_type == Imf::UINT)
				desc += " (uint)" + newline;
		}
	}
}


string CreateXMPdescription(const ProEXRdoc_read &file)
{
	string XMPtext("");
	
	char unicode_nbsp[4] = { 0xef, 0xbb, 0xbf, '\0' };
	
	// make a basic XMP block with one main value - the description
	// we'd break this up into something nicer, but XMP is a little rigid
	// this guarantees that it'll show up in Photoshop File Info
	XMPtext += "<?xpacket begin=\"" + string(unicode_nbsp) + "\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n";
	XMPtext += "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"3.1.1-112\">\n";
	XMPtext += "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n";
	XMPtext += "<rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n";
	XMPtext += "<dc:format>image/exr</dc:format>\n";
	XMPtext += "<dc:description>\n";
	XMPtext += "<rdf:Alt>\n";

	XMPtext += "<rdf:li xml:lang=\"x-default\">";
	
	// here's where the description goes
	AddDescription(XMPtext, file);
	
	XMPtext += "</rdf:li>\n";

	XMPtext += "</rdf:Alt>\n";
	XMPtext += "</dc:description>\n";
	XMPtext += "</rdf:Description>\n";
	XMPtext += "</rdf:RDF>\n";
	XMPtext += "</x:xmpmeta>\n";
	
	
	return XMPtext;
}


static void
quotedTokenize(const string& str,
				  vector<string>& tokens,
				  const string& delimiters = " ")
{
	// this function will respect quoted strings when tokenizing
	// the quotes will NOT be included in the returned strings
	
	int i = 0;
	bool in_quotes = false;
	
	// if there are un-quoted delimiters in the beginning, skip them
	while(i < str.size() && str[i] != '\"' && string::npos != delimiters.find(str[i]) )
		i++;
	
	string::size_type lastPos = i;
	
	while(i < str.size())
	{
		if(str[i] == '\"' && (i == 0 || str[i-1] != '\\'))
			in_quotes = !in_quotes;
		else if(!in_quotes)
		{
			if( string::npos != delimiters.find(str[i]) )
			{
				string token = str.substr(lastPos, i - lastPos);
				
				// clear quotes
				if(token[token.size() - 1] == '\"')
					token.erase(token.size() - 1, 1);
				
				if(token[0] == '\"')
					token.erase(0, 1);
				
			
				tokens.push_back(token);
				
				lastPos = i + 1;
				
				// if there are more delimiters ahead, push forward
				while(lastPos < str.size() && (str[lastPos] != '\"' || str[lastPos-1] != '\\') && string::npos != delimiters.find(str[lastPos]) )
					lastPos++;
					
				i = lastPos;
				continue;
			}
		}
		
		i++;
	}
	
	if(in_quotes)
		throw Iex::BaseExc("Quoted tokenize error.");
	
	// we're at the end, was there anything left?
	if(str.size() - lastPos > 0)
		tokens.push_back( str.substr(lastPos) );
}


static void fixFromXMP(string &val)
{
	searchReplace(val, "&amp;", "&");
	searchReplace(val, "&lt;", "<");
	searchReplace(val, "&gt;", ">");
}

template <typename VALUETYPE>
static Imf::TypedAttribute<VALUETYPE> * ReadPlainValue(const string &attrValue)
{
	istringstream s(attrValue);
	
	VALUETYPE val;
	
	s >> val;
	
	return new Imf::TypedAttribute<VALUETYPE>(val);
}

template <typename VALUETYPE>
static Imf::TypedAttribute<VALUETYPE> * ReadBoxValue(const string &attrValue)
{
	vector<string> tokens;
	quotedTokenize(attrValue, tokens, " [],");
	
	if(tokens.size() == 4)
	{
		istringstream s0(tokens[0]);
		istringstream s1(tokens[1]);
		istringstream s2(tokens[2]);
		istringstream s3(tokens[3]);
	
		VALUETYPE val;
		
		s0 >> val.min.x;
		s1 >> val.min.y;
		s2 >> val.max.x;
		s3 >> val.max.y;
		
		return new Imf::TypedAttribute<VALUETYPE>(val);
	}
	
	return NULL;
}

template <typename VALUETYPE>
static Imf::TypedAttribute<VALUETYPE> * ReadV2Value(const string &attrValue)
{
	vector<string> tokens;
	quotedTokenize(attrValue, tokens, " [],");
	
	if(tokens.size() == 2)
	{
		istringstream s0(tokens[0]);
		istringstream s1(tokens[1]);
	
		VALUETYPE val;
		
		s0 >> val.x;
		s1 >> val.y;
		
		return new Imf::TypedAttribute<VALUETYPE>(val);
	}
	
	return NULL;
}

template <typename VALUETYPE>
static Imf::TypedAttribute<VALUETYPE> * ReadV3Value(const string &attrValue)
{
	vector<string> tokens;
	quotedTokenize(attrValue, tokens, " [],");
	
	if(tokens.size() == 3)
	{
		istringstream s0(tokens[0]);
		istringstream s1(tokens[1]);
		istringstream s2(tokens[2]);
	
		VALUETYPE val;
		
		s0 >> val.x;
		s1 >> val.y;
		s2 >> val.z;
		
		return new Imf::TypedAttribute<VALUETYPE>(val);
	}
	
	return NULL;
}

template <typename VALUETYPE>
static Imf::TypedAttribute<VALUETYPE> * ReadMatrixValue(const string &attrValue, int n)
{
	vector<string> tokens;
	quotedTokenize(attrValue, tokens, " []{},");
	
	const int num_elements = (n * n);
	
	if(tokens.size() == num_elements)
	{
		VALUETYPE val;
	
		for(int i=0; i < num_elements; i++)
		{
			istringstream s(tokens[i]);
			
			const int r = (i / n);
			const int c = (i % n);
			
			s >> val.x[c][r];
		}
	
		return new Imf::TypedAttribute<VALUETYPE>(val);
	}
	
	return NULL;
}

static Imf::Attribute * ParseValue(const string &typeName, const string &attrValue)
{
	assert(attrValue.size() > 0);

	if(typeName == "string")
	{
		const int start_pos = (attrValue[0] == '\"' ? 1 : 0);
		const int string_len = (attrValue[attrValue.size() - 1] == '\"' ? (attrValue.size() - 1 - start_pos) : (attrValue.size() - start_pos));
		
		if(string_len > 0)
		{
			string val = attrValue.substr(start_pos, string_len);
			
			fixFromXMP(val);
		
			return new Imf::StringAttribute(val);
		}
	}
	else if(typeName == "float")
	{
		return ReadPlainValue<float>(attrValue);
	}
	else if(typeName == "double")
	{
		return ReadPlainValue<double>(attrValue);
	}
	else if(typeName == "int")
	{
		return ReadPlainValue<int>(attrValue);
	}
	else if(typeName == "rational")
	{
		vector<string> tokens;
		quotedTokenize(attrValue, tokens, " /");
		
		if(tokens.size() == 2)
		{
			istringstream s0(tokens[0]);
			istringstream s1(tokens[1]);
		
			Imf::Rational val;
			
			s0 >> val.n;
			s1 >> val.d;
			
			return new Imf::RationalAttribute(val);
		}
	}
	else if(typeName == "box2i")
	{
		return ReadBoxValue<Imath::Box2i>(attrValue);
	}
	else if(typeName == "box2f")
	{
		return ReadBoxValue<Imath::Box2f>(attrValue);
	}
	else if(typeName == "v2i")
	{
		return ReadV2Value<Imath::V2i>(attrValue);
	}
	else if(typeName == "v2f")
	{
		return ReadV2Value<Imath::V2f>(attrValue);
	}
	else if(typeName == "v2d")
	{
		return ReadV2Value<Imath::V2d>(attrValue);
	}
	else if(typeName == "v3i")
	{
		return ReadV3Value<Imath::V3i>(attrValue);
	}
	else if(typeName == "v3f")
	{
		return ReadV3Value<Imath::V3f>(attrValue);
	}
	else if(typeName == "v3d")
	{
		return ReadV3Value<Imath::V3d>(attrValue);
	}
	else if(typeName == "m33f")
	{
		return ReadMatrixValue<Imath::M33f>(attrValue, 3);
	}
	else if(typeName == "m33d")
	{
		return ReadMatrixValue<Imath::M33d>(attrValue, 3);
	}
	else if(typeName == "m44f")
	{
		return ReadMatrixValue<Imath::M44f>(attrValue, 4);
	}
	else if(typeName == "m44d")
	{
		return ReadMatrixValue<Imath::M44d>(attrValue, 4);
	}
	else if(typeName == "lineOrder")
	{
		Imf::LineOrder val;
		
		if(attrValue == "Increasing Y")
		{
			val = Imf::INCREASING_Y;
		}
		else if(attrValue == "Decreasing Y")
		{
			val = Imf::DECREASING_Y; // setting this does seem to work, I guess the library handles it
		}
		//else if(attrValue == "Random Y")
		//{
		//	val = Imf::RANDOM_Y;
		//}
		else
			return NULL;
		
		return new LineOrderAttribute(val);
	}
	else if(typeName == "chromaticities")
	{
		vector<string> tokens;
		quotedTokenize(attrValue, tokens, " rgbw(),");
		
		if(tokens.size() == 8)
		{
			istringstream s0(tokens[0]);
			istringstream s1(tokens[1]);
			istringstream s2(tokens[2]);
			istringstream s3(tokens[3]);
			istringstream s4(tokens[4]);
			istringstream s5(tokens[5]);
			istringstream s6(tokens[6]);
			istringstream s7(tokens[7]);
		
			Imf::Chromaticities val;
			
			s0 >> val.red.x;
			s1 >> val.red.y;
			s2 >> val.green.x;
			s3 >> val.green.y;
			s4 >> val.blue.x;
			s5 >> val.blue.y;
			s6 >> val.white.x;
			s7 >> val.white.y;
			
			return new Imf::ChromaticitiesAttribute(val);
		}
	}
	else if(typeName == "envmap")
	{
		Imf::Envmap val;
		
		if(attrValue == "Lat-Long")
		{
			val = Imf::ENVMAP_LATLONG;
		}
		if(attrValue == "Cube")
		{
			val = Imf::ENVMAP_CUBE;
		}
		else
			return NULL;
		
		return new EnvmapAttribute(val);
	}
	else if(typeName == "stringvector")
	{
		vector<string> tokens;
		quotedTokenize(attrValue, tokens, " ,[]");
		
		Imf::StringVector val;
		
		for(int i=0; i < tokens.size(); i++)
		{
			string &str = tokens[i];
			
			fixFromXMP(str);
		
			val.push_back(str);
		}
		
		return new StringVectorAttribute(val);
	}
	else if(typeName == "floatvector")
	{
		vector<string> tokens;
		quotedTokenize(attrValue, tokens, " ,[]");
		
		Imf::FloatVector val;
		
		for(int i=0; i < tokens.size(); i++)
		{
			istringstream s(tokens[i]);
			
			float f;
			
			s >> f;
		
			val.push_back(f);
		}
		
		return new FloatVectorAttribute(val);
	}
	else if(typeName == "timecode")
	{
		vector<string> tokens;
		quotedTokenize(attrValue, tokens, " :;");
		
		if(tokens.size() >= 4)
		{
			istringstream s0(tokens[0]);
			istringstream s1(tokens[1]);
			istringstream s2(tokens[2]);
			istringstream s3(tokens[3]);
			
			int hours, minutes, seconds, frame;
			
			s0 >> hours;
			s1 >> minutes;
			s2 >> seconds;
			s3 >> frame;
			
			const bool dropFrame = (attrValue.find(';') != string::npos);
			const bool colorFrame = (attrValue.find("Color") != string::npos);
			const bool fieldPhase = (attrValue.find("Field") != string::npos);
			const bool bgf0 = (attrValue.find("bgf0") != string::npos);
			const bool bgf1 = (attrValue.find("bgf1") != string::npos);
			const bool bgf2 = (attrValue.find("bgf2") != string::npos);
			
			const Imf::TimeCode val(hours, minutes, seconds, frame, dropFrame, colorFrame, fieldPhase, bgf0, bgf1, bgf2);
			
			return new TimeCodeAttribute(val);
		}
	}
	else if(typeName == "keycode")
	{
		vector<string> tokens;
		quotedTokenize(attrValue, tokens, " ");
		
		assert(tokens.size() % 2 == 0);
		
		Imf::KeyCode val;
		
		int pos = 0;
		
		while(pos < tokens.size() - 1)
		{
			const string &key = tokens[pos];
			const string &value = tokens[pos + 1];
			
			istringstream s(value);
			
			int v;
			
			s >> v;
			
			if(key == "filmMfcCode:")
			{
				if(v >= 0 && v <= 99)
					val.setFilmMfcCode(v);
			}
			else if(key == "filmType:")
			{
				if(v >= 0 && v <= 99)
					val.setFilmType(v);
			}
			else if(key == "prefix:")
			{
				if(v >= 0 && v <= 999999)
					val.setPrefix(v);
			}
			else if(key == "count:")
			{
				if(v >= 0 && v <= 9999)
					val.setCount(v);
			}
			else if(key == "perfOffset:")
			{
				if(v >= 0 && v <= 119)
					val.setPerfOffset(v);
			}
			else if(key == "perfsPerFrame:")
			{
				if(v >= 1 && v <= 15)
					val.setPerfsPerFrame(v);
			}
			else if(key == "perfsPerCount:")
			{
				if(v >= 20 && v <= 120)
					val.setPerfsPerCount(v);
			}
			
			pos += 2;
		}
		
		return new KeyCodeAttribute(val);
	}
	
	return NULL;
}

static void ParseAttribute(Imf::Header &header, int width, int height, const string &attrName, const string &attrType, const string &attrValue)
{
	if((attrName == "dataWindowOverride" && attrType != "box2i") ||
		(attrName == "displayWindowOverride" && attrType != "box2i") ||
		(attrName == "dwaCompressionLevel" && attrType != "float") ||
		(attrName == "screenWindowCenter" && attrType != "v2f") ||
		(attrName == "screenWindowWidth" && attrType != "float") ||
		(attrName == "lineOrder" && attrType != "lineOrder"))
	{
		assert(false); // gave me the wrong type
	}
	else if(attrName == "compression" ||
			attrName == "channels" ||
			attrName == "dataWindow" ||
			attrName == "deepImageState" ||
			attrName == "displayWindow" ||
			attrName == "tiledesc" ||
			attrName == "type" ||
			attrName == "PSlayers")
	{
		// type may be fine, but I'm not doing anything with it
	}
	else if(attrName == "dataWindowOverride")
	{
		// Because dataWindow and displayWindow should not be overridden lightly,
		// I'm going to make the user rename them if they really want to override.
		// dataWindowOverride will be stored as dataWindow if it checks out.
	
		assert(attrType == "box2i");
		
		Imf::Box2iAttribute *attr = dynamic_cast<Imf::Box2iAttribute *>( ParseValue(attrType, attrValue) );
		
		if(attr != NULL)
		{
			const Imath::Box2i &dataWindow = attr->value();
			
			const int attrWidth = (dataWindow.max.x - dataWindow.min.x + 1);
			const int attrHeight = (dataWindow.max.y - dataWindow.min.y + 1);
			
			if(attrWidth == width && attrHeight == height)
			{
				// only use this dataWindow if it matches our dimensions
			
				header.insert("dataWindow", *attr);
				
				delete attr;
			}
		}
	}
	else if(attrName == "displayWindowOverride")
	{
		// Don't have to check displayWindow, but still make the user rename the attribute.
	
		assert(attrType == "box2i");
		
		Imf::Box2iAttribute *attr = dynamic_cast<Imf::Box2iAttribute *>( ParseValue(attrType, attrValue) );
		
		if(attr != NULL)
		{
			header.insert("displayWindow", *attr);
			
			delete attr;
		}
	}
	else
	{
		Imf::Attribute *attribute = ParseValue(attrType, attrValue);
		
		if(attribute != NULL)
		{
			header.insert(attrName, *attribute);
			
			delete attribute;
		}
	}
}

static void ParseLine(Imf::Header &header, int width, int height, const string &line)
{
	// each line goes like this:
	// attrName (attrType): value
	
	const string attr_type_start_text = " (";
	const string attr_type_end_text = "): ";
	
	const string::size_type attr_type_start_pos = line.find(attr_type_start_text);
	const string::size_type attr_type_end_pos = line.find(attr_type_end_text);
	
	if(attr_type_start_pos != string::npos && attr_type_end_pos != string::npos && attr_type_start_pos < attr_type_end_pos)
	{
		const string attrName = line.substr(0, attr_type_start_pos);
		
		const string::size_type attr_type_begin_pos = (attr_type_start_pos + attr_type_start_text.size());
		const string attrType = line.substr(attr_type_begin_pos, attr_type_end_pos - attr_type_begin_pos);
		
		const string::size_type attr_value_begin_pos = (attr_type_end_pos + attr_type_end_text.size());
		const string attrValue = line.substr(attr_value_begin_pos);
		
		if(attrName.size() > 0 && attrType.size() > 0 && attrValue.size() > 0)
		{
			ParseAttribute(header, width, height, attrName, attrType, attrValue);
		}
	}
}

void ParseXMPdescription(Imf::Header &header, int width, int height, const char *xmp_text, size_t xmp_size)
{
	if(xmp_text != NULL && xmp_size > 0)
	{
		const string XMPtext(xmp_text, xmp_size);
		
		const string::size_type rdf_description_start =  XMPtext.find("<rdf:Description");
		
		if(rdf_description_start != string::npos)
		{
			const string::size_type rdf_description_end = XMPtext.find("</rdf:Description", rdf_description_start);
			
			if(rdf_description_end != string::npos)
			{
				const string::size_type dc_description_start = XMPtext.find("<dc:description>", rdf_description_start);
				const string::size_type dc_description_end = XMPtext.find("</dc:description>", rdf_description_start);
				
				if(dc_description_start != string::npos && dc_description_end != string::npos && dc_description_start < dc_description_end)
				{
					const string::size_type rdf_alt_start = XMPtext.find("<rdf:Alt>", dc_description_start);
					const string::size_type rdf_alt_end = XMPtext.find("</rdf:Alt>", dc_description_start);
					
					if(rdf_alt_start != string::npos && rdf_alt_end != string::npos && rdf_alt_start < rdf_alt_end)
					{
						const string::size_type rdf_li_start = XMPtext.find("<rdf:li", rdf_alt_start);
						const string::size_type rdf_li_end = XMPtext.find("</rdf:li>", rdf_alt_start);
						
						if(rdf_li_start != string::npos && rdf_li_end != string::npos && rdf_li_start < rdf_li_end)
						{
							const string rdf_li_tag_end_text = ">";
						
							const string::size_type rdf_li_tag_end = XMPtext.find(rdf_li_tag_end_text, rdf_li_start);
							
							if(rdf_li_tag_end != string::npos)
							{
								const string::size_type description_start = rdf_li_tag_end + rdf_li_tag_end_text.size();
								const string::size_type description_end = rdf_li_end;
								
								// parse into lines
								const string newline("&#xA;");
								
								
								string::size_type current_position = description_start;
								
								while(current_position < description_end)
								{
									string::size_type next_newline = XMPtext.find(newline, current_position);
									
									if(next_newline == string::npos || next_newline > description_end)
										next_newline = description_end;
									
									const string line = XMPtext.substr(current_position, next_newline - current_position);
									
									ParseLine(header, width, height, line);
									
									current_position = next_newline + newline.size();
								}
							}
						}
					}
				}
			}
		}
	}
}
