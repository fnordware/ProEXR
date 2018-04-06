//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//

#include "Cryptomatte_AE.h"

#include "Cryptomatte_AE_Dialog.h"

#include "picojson.h"

#include <assert.h>

#include <float.h>

#include <sstream>
#include <iomanip>

//using namespace std;

class ErrThrower : public std::exception
{
  public:
	ErrThrower(A_Err err = A_Err_NONE) throw() : _err(err) {}
	ErrThrower(const ErrThrower &other) throw() : _err(other._err) {}
	virtual ~ErrThrower() throw() {}

	ErrThrower & operator = (A_Err err)
	{
		_err = err;
		
		if(_err != A_Err_NONE)
			throw *this;
		
		return *this;
	}

	A_Err err() const { return _err; }
	
	virtual const char* what() const throw() { return "AE Error"; }
	
  private:
	A_Err _err;
};


CryptomatteContext::CryptomatteContext(CryptomatteArbitraryData *arb) :
	_hash(0)
{
	if(arb == NULL)
		throw CryptomatteException("no arb");
	
	_layer = GetLayer(arb);
	
	picojson::value manifestObj;
	picojson::parse(manifestObj, GetManifest(arb));
	
	if( manifestObj.is<picojson::object>() )
	{
		const picojson::object &object = manifestObj.get<picojson::object>();
		
		for(picojson::object::const_iterator i = object.begin(); i != object.end(); ++i)
		{
			const std::string &name = i->first;
			const picojson::value &value = i->second;
			
			if( value.is<std::string>() )
			{
				const std::string &hexString = value.get<std::string>();
				
				if(hexString.size() == 8)
				{
					unsigned int intValue = 0;
					const int matched = sscanf(hexString.c_str(), "%x", &intValue);
					
					if(matched && intValue)
					{
						_manifest[name] = intValue;
					}
				}
			}
		}
	}
	
	SetSelection(arb);
}


CryptomatteContext::~CryptomatteContext()
{
	for(std::vector<Level *>::iterator i = _levels.begin(); i != _levels.end(); ++i)
	{
		Level *level = *i;
		
		delete level;
	}
}


void
CryptomatteContext::SetSelection(CryptomatteArbitraryData *arb)
{
	if(_hash != arb->hash)
	{
		_hash = arb->hash;
		
		_selection.clear();
		
		try
		{
			const std::string selectionString = GetSelection(arb);
			
			if(!selectionString.empty())
			{
				std::vector<std::string> tokens;
				quotedTokenize(selectionString, tokens, ", ");
				
				for(std::vector<std::string>::const_iterator i = tokens.begin(); i != tokens.end(); ++i)
				{
					const std::string val = deQuote(*i);
					
					if( _manifest.count(val) )
					{
						_selection.insert(_manifest[val]);
					}
					else if(val.size() == 8)
					{
						unsigned int intValue = 0;
						const int matched = sscanf(val.c_str(), "%x", &intValue);
						
						if(matched && intValue)
							_selection.insert(intValue);
					}
				}
			}
		}
		catch(...) {}
	}
}


void
CryptomatteContext::LoadLevels(PF_InData *in_data)
{
	for(std::vector<Level *>::iterator i = _levels.begin(); i != _levels.end(); ++i)
	{
		Level *level = *i;
		
		delete level;
	}

	_levels.clear();
	

	if(!_layer.empty())
	{
		AEGP_SuiteHandler suites(in_data->pica_basicP);
		
		PF_ChannelSuite *cs = suites.PFChannelSuite();
		
		A_long num_channels = 0;
		cs->PF_GetLayerChannelCount(in_data->effect_ref, CRYPTO_INPUT, &num_channels);
		
		if(num_channels > 0)
		{
			// first we try to find 4-channel names
			std::string nextFourName;
			CalculateNext4Name(nextFourName);
			
			PF_ChannelRef four;
			
			for(int i=0; i < num_channels; i++)
			{
				PF_Boolean found;
				PF_ChannelRef channelRef;
				PF_ChannelDesc channelDesc;
				
				cs->PF_GetLayerChannelIndexedRefAndDesc(in_data->effect_ref,
														CRYPTO_INPUT,
														i,
														&found,
														&channelRef,
														&channelDesc);
														
				if(found && channelDesc.channel_type && channelDesc.data_type == PF_DataType_FLOAT &&
					channelDesc.dimension == 4 && channelDesc.name == nextFourName)
				{
					_levels.push_back(new Level(in_data, four, false) );
					_levels.push_back(new Level(in_data, four, true) );
					
					CalculateNext4Name(nextFourName);
					
					i = 0; // start over
				}
			}
			
			
			std::string nextHashName, nextCoverageName;
			CalculateNextNames(nextHashName, nextCoverageName);
			
			PF_ChannelRef hash, coverage;
			bool foundHash = false, foundCoverage = false;
			
			for(int i=0; i < num_channels; i++)
			{
				PF_Boolean found;
				PF_ChannelRef channelRef;
				PF_ChannelDesc channelDesc;
				
				cs->PF_GetLayerChannelIndexedRefAndDesc(in_data->effect_ref,
														CRYPTO_INPUT,
														i,
														&found,
														&channelRef,
														&channelDesc);
														
				if(found && channelDesc.channel_type && channelDesc.data_type == PF_DataType_FLOAT && channelDesc.dimension == 1)
				{
					if(channelDesc.name == nextHashName || channelDesc.name == nextCoverageName)
					{
						if(channelDesc.name == nextHashName)
						{
							hash = channelRef;
							foundHash = true;
						}
						else
						{
							coverage = channelRef;
							foundCoverage = true;
						}
						
						if(foundHash && foundCoverage)
						{
							_levels.push_back(new Level(in_data, hash, coverage) );
							
							CalculateNextNames(nextHashName, nextCoverageName);
							foundHash = false;
							foundCoverage = false;
							
							i = 0; // start over
						}
					}
				}
			}
		}
	}
}


float
CryptomatteContext::GetCoverage(int x, int y) const
{
	float coverage = 0.f;

	for(std::vector<Level *>::const_iterator i = _levels.begin(); i != _levels.end(); ++i)
	{
		const Level *level = *i;
		
		if(level->GetHash(x, y) == 0)
			break;
		
		coverage += level->GetCoverage(_selection, x, y);
	}
	
	return coverage;
}


std::set<std::string>
CryptomatteContext::GetItems(int x, int y) const
{
	std::set<std::string> items;
	
	for(std::vector<Level *>::const_iterator i = _levels.begin(); i != _levels.end(); ++i)
	{
		const Level *level = *i;
		
		const A_u_long hash = level->GetHash(x, y);
		
		if(hash != 0)
		{
			std::string item;
			
			for(std::map<std::string, A_u_long>::const_iterator j = _manifest.begin(); j != _manifest.end() && item.empty(); ++j)
			{
				const std::string &name = j->first;
				const A_u_long &value = j->second;
				
				if(hash == value)
					item = name;
			}
			
			if( item.empty() )
			{
				char hexStr[9];
				sprintf(hexStr, "%08x", hash);
				
				item = hexStr;
			}
			
			items.insert(item);
		}
	}
	
	return items;
}


std::string
CryptomatteContext::searchReplace(const std::string &str, const std::string &search, const std::string &replace)
{
	std::string s = str;
	
	// locate the search strings
	std::vector<std::string::size_type> positions;

	std::string::size_type last_pos = 0;

	while(last_pos != std::string::npos && last_pos < s.size())
	{
		last_pos = s.find(search, last_pos);

		if(last_pos != std::string::npos)
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
	
	return s;
}


std::string
CryptomatteContext::deQuote(const std::string &s)
{
	std::string::size_type start_pos = (s[0] == '\"' ? 1 : 0);
	std::string::size_type end_pos = ( (s.size() >= 2 && s[s.size()-1] == '\"' && s[s.size()-2] != '\\') ? s.size()-2 : s.size()-1);

	return searchReplace(s.substr(start_pos, end_pos + 1 - start_pos), "\\\"", "\"");
}


void
CryptomatteContext::quotedTokenize(const std::string &str, std::vector<std::string> &tokens, const std::string& delimiters)
{
	// this function will respect quoted strings when tokenizing
	// the quotes will be included in the returned strings
	
	int i = 0;
	bool in_quotes = false;
	
	// if there are un-quoted delimiters in the beginning, skip them
	while(i < str.size() && str[i] != '\"' && std::string::npos != delimiters.find(str[i]) )
		i++;
	
	std::string::size_type lastPos = i;
	
	while(i < str.size())
	{
		if(str[i] == '\"' && (i == 0 || str[i-1] != '\\'))
			in_quotes = !in_quotes;
		else if(!in_quotes)
		{
			if( std::string::npos != delimiters.find(str[i]) )
			{
				tokens.push_back(str.substr(lastPos, i - lastPos));
				
				lastPos = i + 1;
				
				// if there are more delimiters ahead, push forward
				while(lastPos < str.size() && (str[lastPos] != '\"' || str[lastPos-1] != '\\') && std::string::npos != delimiters.find(str[lastPos]) )
					lastPos++;
					
				i = lastPos;
				continue;
			}
		}
		
		i++;
	}
	
	if(in_quotes)
		throw CryptomatteException("Quoted tokenize error.");
	
	// we're at the end, was there anything left?
	if(str.size() - lastPos > 0)
		tokens.push_back( str.substr(lastPos) );
}


int
CryptomatteContext::Width() const
{
#ifdef NDEBUG
	if(_levels.size() > 0)
		return _levels[0].Width();
	else
		return 0;
#else
	int width = 0;
	
	for(std::vector<Level *>::const_iterator i = _levels.begin(); i != _levels.end(); ++i)
	{
		const Level *level = *i;
		
		if(width == 0)
			width = level->Width();
		else
			assert(width == level->Width());
	}
	
	return width;
#endif
}


int
CryptomatteContext::Height() const
{
#ifdef NDEBUG
	if(_levels.size() > 0)
		return _levels[0].Height();
	else
		return 0;
#else
	int height = 0;
	
	for(std::vector<Level *>::const_iterator i = _levels.begin(); i != _levels.end(); ++i)
	{
		const Level *level = *i;
		
		if(height == 0)
			height = level->Height();
		else
			assert(height == level->Height());
	}
	
	return height;
#endif
}


CryptomatteContext::Level::Level(PF_InData *in_data, PF_ChannelRef &hash, PF_ChannelRef &coverage) :
	_hash(NULL),
	_coverage(NULL)
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	PF_ChannelSuite *cs = suites.PFChannelSuite();
	
	PF_ChannelChunk hashChunk;
	
	PF_Err err = cs->PF_CheckoutLayerChannel(in_data->effect_ref,
												&hash,
												in_data->current_time,
												in_data->time_step,
												in_data->time_scale,
												PF_DataType_FLOAT,
												&hashChunk);
											
	if(!err && hashChunk.dataPV != NULL)
	{
		assert(hashChunk.dimensionL == 1);
		assert(hashChunk.data_type == PF_DataType_FLOAT);
		
		_hash = new FloatBuffer(in_data, (char *)hashChunk.dataPV, hashChunk.widthL, hashChunk.heightL, sizeof(float), hashChunk.row_bytesL);
		
		cs->PF_CheckinLayerChannel(in_data->effect_ref, &hash, &hashChunk);
	}
	else
		throw CryptomatteException("Failed to load hash");
	
	
	PF_ChannelChunk coverageChunk;
	
	err = cs->PF_CheckoutLayerChannel(in_data->effect_ref,
										&coverage,
										in_data->current_time,
										in_data->time_step,
										in_data->time_scale,
										PF_DataType_FLOAT,
										&coverageChunk);
												
	if(!err && coverageChunk.dataPV != NULL)
	{
		assert(coverageChunk.dimensionL == 1);
		assert(coverageChunk.data_type == PF_DataType_FLOAT);
		
		assert(coverageChunk.widthL == hashChunk.widthL);
		assert(coverageChunk.heightL == hashChunk.heightL);
		
		_coverage = new FloatBuffer(in_data, (char *)coverageChunk.dataPV, coverageChunk.widthL, coverageChunk.heightL, sizeof(float), coverageChunk.row_bytesL);
		
		cs->PF_CheckinLayerChannel(in_data->effect_ref, &coverage, &coverageChunk);
	}
	else
	{
		delete _hash;
		
		throw CryptomatteException("Failed to load hash");
	}
}


CryptomatteContext::Level::Level(PF_InData *in_data, PF_ChannelRef &four, bool secondHalf) :
	_hash(NULL),
	_coverage(NULL)
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	PF_ChannelSuite *cs = suites.PFChannelSuite();
	
	PF_ChannelChunk fourChunk;
	
	PF_Err err = cs->PF_CheckoutLayerChannel(in_data->effect_ref,
												&four,
												in_data->current_time,
												in_data->time_step,
												in_data->time_scale,
												PF_DataType_FLOAT,
												&fourChunk);
											
	if(!err && fourChunk.dataPV != NULL)
	{
		assert(fourChunk.dimensionL == 4);
		assert(fourChunk.data_type == PF_DataType_FLOAT);
		
		// it'll be ARGB
		
		_hash = new FloatBuffer(in_data,
								(char *)fourChunk.dataPV + (sizeof(float) * (secondHalf ? 3 : 1)),
								fourChunk.widthL,
								fourChunk.heightL,
								sizeof(float) * 4,
								fourChunk.row_bytesL);
		
		_coverage = new FloatBuffer(in_data,
								(char *)fourChunk.dataPV + (sizeof(float) * (secondHalf ? 0 : 2)),
								fourChunk.widthL,
								fourChunk.heightL,
								sizeof(float) * 4,
								fourChunk.row_bytesL);
								
		cs->PF_CheckinLayerChannel(in_data->effect_ref, &four, &fourChunk);
	}
	else
		throw CryptomatteException("Failed to load hash");
}


CryptomatteContext::Level::~Level()
{
	delete _hash;
	delete _coverage;
}


float
CryptomatteContext::Level::GetCoverage(const std::set<A_u_long> &selection, int x, int y) const
{
	const float coverage = _coverage->Get(x, y);
	
	if(coverage > 0.f)
	{
		const float floatHash = _hash->Get(x, y);
		
		const A_u_long *hash = (A_u_long *)&floatHash;
		
		return (selection.count(*hash) ? coverage : 0.f);
	}
	else
		return 0.f;
}


A_u_long
CryptomatteContext::Level::GetHash(int x, int y) const
{
	const float floatHash = _hash->Get(x, y);
	
	const A_u_long *hash = (A_u_long *)&floatHash;
	
	return *hash;
}


typedef struct FloatBufferIterateData {
	char *buf;
	char *origin;
	int width;
	ptrdiff_t xStride;
	ptrdiff_t yStride;
	
	FloatBufferIterateData(char *b, char *o, int w, ptrdiff_t x, ptrdiff_t y) :
		buf(b),
		origin(o),
		width(w),
		xStride(x),
		yStride(y)
		{}
} FloatBufferIterateData;


static PF_Err
FloatBuffer_Iterate(void *refconPV,
					A_long thread_indexL,
					A_long i,
					A_long iterationsL)
{
	FloatBufferIterateData *i_data = (FloatBufferIterateData *)refconPV;
	
	const size_t rowbytes = sizeof(float) * i_data->width;
	
	float *in = (float *)(i_data->origin + (i * i_data->yStride));
	float *out = (float *)(i_data->buf + (i * rowbytes));
	
	const int inStep = (i_data->xStride / sizeof(float));
	const int outStep = 1;
	
	for(int x=0; x < i_data->width; x++)
	{
		*out = *in;
		
		in += inStep;
		out += outStep;
	}

	return PF_Err_NONE;
}


CryptomatteContext::Level::FloatBuffer::FloatBuffer(PF_InData *in_data, char *origin, int width, int height, ptrdiff_t xStride, ptrdiff_t yStride) :
	_buf(NULL),
	_width(width),
	_height(height)
{
	const size_t rowbytes = sizeof(float) * _width;
	const size_t siz = rowbytes * _height;
	
	_buf = (char *)malloc(siz);
	
	if(_buf == NULL)
		throw CryptomatteException("Memory error");
	
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	FloatBufferIterateData iter(_buf,origin, width, xStride, yStride);
	
	suites.PFIterate8Suite()->iterate_generic(height, &iter, FloatBuffer_Iterate);
	
	/*
	for(int y=0; y < height; y++)
	{
		float *in = (float *)(origin + (y * yStride));
		float *out = (float *)(_buf + (y * rowbytes));
		
		const int inStep = (xStride / sizeof(float));
		const int outStep = 1;
		
		for(int x=0; x < width; x++)
		{
			*out = *in;
			
			in += inStep;
			out += outStep;
		}
	}*/
}


CryptomatteContext::Level::FloatBuffer::~FloatBuffer()
{
	if(_buf)
		free(_buf);
}


void
CryptomatteContext::CalculateNextNames(std::string &nextHashName, std::string &nextCoverageName)
{
	const int layerNum = (_levels.size() / 2);
	const bool useBA = (_levels.size() % 2);
	
	std::stringstream ss1, ss2;
	
	ss1 << _layer << std::setw(2) << std::setfill('0') << layerNum << ".";
	ss2 << _layer << std::setw(2) << std::setfill('0') << layerNum << ".";
	
	if(useBA)
	{
		ss1 << "blue";
		ss2 << "alpha";
	}
	else
	{
		ss1 << "red";
		ss2 << "green";
	}
	
	nextHashName = ss1.str();
	nextCoverageName = ss2.str();
}


void
CryptomatteContext::CalculateNext4Name(std::string &fourName)
{
	const int layerNum = (_levels.size() / 2);
	
	std::stringstream ss;
	
	ss <<
		_layer << std::setw(2) << std::setfill('0') << layerNum << ".alpha" << "|" <<
		_layer << std::setw(2) << std::setfill('0') << layerNum << ".red" << "|" <<
		_layer << std::setw(2) << std::setfill('0') << layerNum << ".green" << "|" <<
		_layer << std::setw(2) << std::setfill('0') << layerNum << ".blue";
	
	fourName = ss.str();
}

#pragma mark-


static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_SPRINTF(	out_data->return_msg, 
				"%s - %s\r\rwritten by %s\r\rv%d.%d - %s\r\r%s\r%s",
				NAME,
				DESCRIPTION,
				AUTHOR, 
				MAJOR_VERSION, 
				MINOR_VERSION,
				RELEASE_DATE,
				COPYRIGHT,
				WEBSITE);
				
	return PF_Err_NONE;
}


static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	//	We do very little here.
		
	out_data->my_version 	= 	PF_VERSION(	MAJOR_VERSION, 
											MINOR_VERSION,
											BUG_VERSION, 
											STAGE_VERSION, 
											BUILD_VERSION);

	out_data->out_flags 	= 	PF_OutFlag_DEEP_COLOR_AWARE		|
								PF_OutFlag_PIX_INDEPENDENT		|
								PF_OutFlag_CUSTOM_UI			|
							#ifdef WIN_ENV
								PF_OutFlag_KEEP_RESOURCE_OPEN	|
							#endif
								PF_OutFlag_USE_OUTPUT_EXTENT;

	out_data->out_flags2 	=	PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG |
								PF_OutFlag2_SUPPORTS_SMART_RENDER	|
								PF_OutFlag2_FLOAT_COLOR_AWARE;
	
	return PF_Err_NONE;
}


static PF_Err
ParamsSetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err 			err = PF_Err_NONE;
	PF_ParamDef		def;


	// readout
	AEFX_CLR_STRUCT(def);
	 // we can time_vary once we're willing to print and scan ArbData text
	def.flags = PF_ParamFlag_CANNOT_TIME_VARY;
	
#define ARB_REFCON NULL // used by PF_ADD_ARBITRARY
	
	ArbNewDefault(in_data, out_data, ARB_REFCON, &def.u.arb_d.dephault);

	PF_ADD_ARBITRARY("Settings",
						kUI_CONTROL_WIDTH,
						kUI_CONTROL_HEIGHT,
						PF_PUI_CONTROL,
						def.u.arb_d.dephault,
						ARBITRARY_DATA_ID,
						ARB_REFCON);
	
/*	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY;
	def.ui_flags = PF_PUI_STD_CONTROL_ONLY;
	PF_ADD_POPUP(	"Click Action",
					ACTION_NUM_OPTIONS, //number of choices
					ACTION_ADD, //default
					ACTION_MENU_STR,
					ACTION_ID);
*/

	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX("", "Matte Only",
					FALSE, 0,
					MATTE_ID);

	out_data->num_params = CRYPTO_NUM_PARAMS;

	// register custom UI
	if(!err) 
	{
		PF_CustomUIInfo			ci;

		AEFX_CLR_STRUCT(ci);
		
		ci.events				= PF_CustomEFlag_EFFECT | PF_CustomEFlag_LAYER | PF_CustomEFlag_COMP;
 		
		ci.comp_ui_width		= ci.comp_ui_height = 0;
		ci.comp_ui_alignment	= PF_UIAlignment_NONE;
		
		ci.layer_ui_width		= 0;
		ci.layer_ui_height		= 0;
		ci.layer_ui_alignment	= PF_UIAlignment_NONE;
		
		ci.preview_ui_width		= 0;
		ci.preview_ui_height	= 0;
		ci.layer_ui_alignment	= PF_UIAlignment_NONE;

		err = (*(in_data->inter.register_ui))(in_data->effect_ref, &ci);
	}


	return err;
}


static PF_Err
SequenceSetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;
	
	CryptomatteSequenceData *sequence_data = NULL;
	
	// set up sequence data
	if( (in_data->sequence_data == NULL) )
	{
		out_data->sequence_data = PF_NEW_HANDLE( sizeof(CryptomatteSequenceData) );
		
		sequence_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(out_data->sequence_data);
	}
	else // reset pre-existing sequence data
	{
		if( PF_GET_HANDLE_SIZE(in_data->sequence_data) != sizeof(CryptomatteSequenceData) )
		{
			PF_RESIZE_HANDLE(sizeof(CryptomatteSequenceData), &in_data->sequence_data);
		}
			
		sequence_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(in_data->sequence_data);
	}
	
	// set defaults
	sequence_data->context = NULL;
	sequence_data->selectionChanged = FALSE;
	
	PF_UNLOCK_HANDLE(in_data->sequence_data);
	
	return err;
}


static PF_Err 
SequenceSetdown (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;
	
	if(in_data->sequence_data)
	{
		CryptomatteSequenceData *seq_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(in_data->sequence_data);
		
		if(seq_data->context != NULL)
		{
			CryptomatteContext *ctx = (CryptomatteContext *)seq_data->context;
			
			delete ctx;
			
			seq_data->context = NULL;
			seq_data->selectionChanged = FALSE;
		}
	
		PF_DISPOSE_HANDLE(in_data->sequence_data);
	}

	return err;
}


/*
static PF_Err
UserChangedParam (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_UserChangedParamExtra*	extra)
{
	return PF_Err_NONE;
}
*/

static PF_Err 
SequenceFlatten (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	if(in_data->sequence_data)
	{
		CryptomatteSequenceData *seq_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(in_data->sequence_data);
		
		if(seq_data->context != NULL)
		{
			CryptomatteContext *ctx = (CryptomatteContext *)seq_data->context;
			
			delete ctx;
			
			seq_data->context = NULL;
			seq_data->selectionChanged = FALSE;
		}
	
		PF_UNLOCK_HANDLE(in_data->sequence_data);
	}

	return PF_Err_NONE;
}


static PF_Boolean
IsEmptyRect(const PF_LRect *r){
	return (r->left >= r->right) || (r->top >= r->bottom);
}

#ifndef mmin
	#define mmin(a,b) ((a) < (b) ? (a) : (b))
	#define mmax(a,b) ((a) > (b) ? (a) : (b))
#endif


static void
UnionLRect(const PF_LRect *src, PF_LRect *dst)
{
	if (IsEmptyRect(dst)) {
		*dst = *src;
	} else if (!IsEmptyRect(src)) {
		dst->left 	= mmin(dst->left, src->left);
		dst->top  	= mmin(dst->top, src->top);
		dst->right 	= mmax(dst->right, src->right);
		dst->bottom = mmax(dst->bottom, src->bottom);
	}
}


static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err err = PF_Err_NONE;
	PF_RenderRequest req = extra->input->output_request;
	PF_CheckoutResult in_result;
	
	//req.preserve_rgb_of_zero_alpha = TRUE;

	ERR(extra->cb->checkout_layer(	in_data->effect_ref,
									CRYPTO_INPUT,
									CRYPTO_INPUT,
									&req,
									in_data->current_time,
									in_data->time_step,
									in_data->time_scale,
									&in_result));


	UnionLRect(&in_result.result_rect, 		&extra->output->result_rect);
	UnionLRect(&in_result.max_result_rect, 	&extra->output->max_result_rect);	
	
	// BTW, just because we checked out the layer here, doesn't mean we really
	// have to check it out.  
	
	return err;
}

#pragma mark-

static inline float Clamp(const float &val)
{
	return (val > 1.f ? 1.f : (val < 0.f ? 0.f : val));
}

template <typename T>
static inline T FloatToChan(const float &val);

template <>
static inline PF_FpShort FloatToChan<PF_FpShort>(const float &val)
{
	return val;
}

template <>
static inline A_u_short FloatToChan<A_u_short>(const float &val)
{
	return ((Clamp(val) * (float)PF_MAX_CHAN16) + 0.5f);
}

template <>
static inline A_u_char FloatToChan<A_u_char>(const float &val)
{
	return ((Clamp(val) * (float)PF_MAX_CHAN8) + 0.5f);
}

/*
template <typename T>
static inline float ChanToFloat(const T &val);

template <>
static inline float ChanToFloat<PF_FpShort>(const PF_FpShort &val)
{
	return val;
}

template <>
static inline float ChanToFloat<A_u_short>(const A_u_short &val)
{
	return ((float)val / (float)PF_MAX_CHAN16);
}

template <>
static inline float ChanToFloat<A_u_char>(const A_u_char &val)
{
	return ((float)val / (float)PF_MAX_CHAN8);
}
*/


typedef struct AlphaIterateData {
	PF_InData			*in_data;
	CryptomatteContext	*context;
	PF_PixelPtr			data;
	A_long				rowbytes;
	PF_Point			channelMove;
	A_long				width;
	
	AlphaIterateData(PF_InData *in, CryptomatteContext *c, PF_PixelPtr d, A_long rb, PF_Point ch, A_long w) :
		in_data(in),
		context(c),
		data(d),
		rowbytes(rb),
		channelMove(ch),
		width(w) {}
} AlphaIterateData;


template <typename PIXTYPE, typename CHANTYPE>
static PF_Err
DrawAlpha_Iterate(void *refconPV,
					A_long thread_indexL,
					A_long i,
					A_long iterationsL)
{
	PF_Err err = PF_Err_NONE;
	
	AlphaIterateData *i_data = (AlphaIterateData *)refconPV;
	PF_InData *in_data = i_data->in_data;
	
	PIXTYPE *pix = (PIXTYPE *)((char *)i_data->data + ((i + i_data->channelMove.v) * i_data->rowbytes) + (i_data->channelMove.h * sizeof(PIXTYPE)));
	
	for(int x=0; x < i_data->width; x++)
	{
		pix->alpha = FloatToChan<CHANTYPE>(i_data->context->GetCoverage(x + i_data->channelMove.h, i + i_data->channelMove.v));
		
		pix++;
	}

#ifdef NDEBUG
	if(thread_indexL == 0)
		err = PF_ABORT(in_data);
#endif

	return err;
}


typedef struct MergeIterateData {
	PF_InData		*in_data;
	PF_EffectWorld	*alpha;
	PF_EffectWorld	*input;
	PF_EffectWorld	*output;
	PF_Point		worldMove;
	PF_Point		channelMove;
	A_long			width;
	bool			matteOnly;
	
	MergeIterateData(PF_InData *in, PF_EffectWorld *a, PF_EffectWorld *i, PF_EffectWorld *o, PF_Point w, PF_Point c, A_long wd, bool m) :
		in_data(in),
		alpha(a),
		input(i),
		output(o),
		worldMove(w),
		channelMove(c),
		width(wd),
		matteOnly(m) {}
} MergeIterateData;


template <typename PIXTYPE, typename CHANTYPE>
static PF_Err
Merge_Iterate(void *refconPV,
				A_long thread_indexL,
				A_long i,
				A_long iterationsL)
{
	PF_Err err = PF_Err_NONE;
	
	MergeIterateData *i_data = (MergeIterateData *)refconPV;
	PF_InData *in_data = i_data->in_data;
	
	PIXTYPE *alpha = (PIXTYPE *)((char *)i_data->alpha->data + ((i + i_data->channelMove.v) * i_data->alpha->rowbytes) + (i_data->channelMove.h * sizeof(PIXTYPE)));
	PIXTYPE *input = (PIXTYPE *)((char *)i_data->input->data + ((i + i_data->worldMove.v) * i_data->input->rowbytes) + (i_data->worldMove.h * sizeof(PIXTYPE)));
	PIXTYPE *output = (PIXTYPE *)((char *)i_data->output->data + ((i + i_data->worldMove.v) * i_data->output->rowbytes) + (i_data->worldMove.h * sizeof(PIXTYPE)));
	
	if(i_data->matteOnly)
	{
		const CHANTYPE white = FloatToChan<CHANTYPE>(1.f);
	
		for(int x=0; x < i_data->width; x++)
		{
			output->alpha = white;
			output->blue = output->green = output->red = alpha->alpha;
			
			alpha++;
			output++;
		}
	}
	else
	{
		for(int x=0; x < i_data->width; x++)
		{
			output->alpha = alpha->alpha;
			output->red = input->red;
			output->green = input->green;
			output->blue = input->blue;
		
			alpha++;
			input++;
			output++;
		}
	}

#ifdef NDEBUG
	if(thread_indexL == 0)
		err = PF_ABORT(in_data);
#endif

	return err;
}


#pragma mark-

static PF_Err
DoRender(
		PF_InData		*in_data,
		PF_EffectWorld 	*input,
		PF_ParamDef		*CRYPTO_data,
		PF_ParamDef		*CRYPTO_matte,
		PF_OutData		*out_data,
		PF_EffectWorld	*output)
{
	PF_Err ae_err = PF_Err_NONE;
	
	CryptomatteArbitraryData *arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(CRYPTO_data->u.arb_d.value);
	CryptomatteSequenceData *seq_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(in_data->sequence_data);

	PF_EffectWorld alphaWorldData;
	PF_EffectWorld *alphaWorld = NULL;
	
	//PF_EffectWorld tempAlphaWorldData;
	//PF_EffectWorld *tempAlphaWorld = NULL;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	try
	{
		ErrThrower err;

		CryptomatteContext *context = (CryptomatteContext *)seq_data->context;
		
		if(context != NULL)
		{
			if(context->Hash() != arb_data->hash)
			{
				if(seq_data->selectionChanged)
				{
					// only the selection changed, don't rebuild the whole thing
					context->SetSelection(arb_data);
				}
				else
				{
					delete context;
					
					context = NULL;
					
					seq_data->context = NULL;
				}
			}
		}
		
		
		if(context == NULL)
		{
			seq_data->context = context = new CryptomatteContext(arb_data);
		
			context->LoadLevels(in_data);
		}
		else if(!seq_data->selectionChanged)
		{
			// don't re-load levels if the selection JUST changed
			// hopefully people don't switch frames in between the click and the render,
			// say if caps lock was down
			
			context->LoadLevels(in_data);
		}
		
		seq_data->selectionChanged = FALSE;
		
	
		if( context->Valid() )
		{
			PF_PixelFormat format = PF_PixelFormat_INVALID;
			suites.PFWorldSuite()->PF_GetPixelFormat(output, &format);
			
			// make pixel world for Cryptomatte, black RGB with alpha
			alphaWorld = &alphaWorldData;
			suites.PFWorldSuite()->PF_NewWorld(in_data->effect_ref, context->Width(), context->Height(), TRUE, format, alphaWorld);
			

			// the origin might not be 0,0 and the ROI might not include the whole image
			// we have to figure out where we have to move our pointers to the right spot in each buffer
			// and copy only as far as we can

			// if the origin is negative, we move in the world, if positive, we move in the channel
			PF_Point world_move, chan_move;
			
			world_move.h = MAX(-output->origin_x, 0);
			world_move.v = MAX(-output->origin_y, 0);
			
			chan_move.h = MAX(output->origin_x, 0);
			chan_move.v = MAX(output->origin_y, 0);

			const int copy_width = MIN(output->width - world_move.h, alphaWorld->width - chan_move.h);
			const int copy_height = MIN(output->height - world_move.v, alphaWorld->height - chan_move.v);


			AlphaIterateData alpha_iter(in_data, context, alphaWorld->data, alphaWorld->rowbytes, chan_move, copy_width);
			
			if(format == PF_PixelFormat_ARGB128)
			{
				err = suites.PFIterate8Suite()->iterate_generic(copy_height, &alpha_iter, DrawAlpha_Iterate<PF_PixelFloat, PF_FpShort>);
			}
			else if(format == PF_PixelFormat_ARGB64)
			{
				err = suites.PFIterate8Suite()->iterate_generic(copy_height, &alpha_iter, DrawAlpha_Iterate<PF_Pixel16, A_u_short>);
			}
			else if(format == PF_PixelFormat_ARGB32)
			{
				err = suites.PFIterate8Suite()->iterate_generic(copy_height, &alpha_iter, DrawAlpha_Iterate<PF_Pixel, A_u_char>);
			}
			
			/*
			PF_EffectWorld *activeAlphaWorld = NULL;
			
			if(alphaWorld->width == output->width && alphaWorld->height == output->height)
			{
				activeAlphaWorld = alphaWorld;
			}
			else
			{
				tempAlphaWorld = &tempAlphaWorldData;
				
				err = suites.PFWorldSuite()->PF_NewWorld(in_data->effect_ref, output->width, output->height, TRUE, format, tempAlphaWorld);
				
				if(in_data->quality == PF_Quality_HI)
					err = suites.PFWorldTransformSuite()->copy_hq(in_data->effect_ref, alphaWorld, tempAlphaWorld, NULL, NULL);
				else
					err = suites.PFWorldTransformSuite()->copy(in_data->effect_ref, alphaWorld, tempAlphaWorld, NULL, NULL);
					
				activeAlphaWorld = tempAlphaWorld;
			}
			*/
			
			MergeIterateData merge_iter(in_data, alphaWorld, input, output, world_move, chan_move, copy_width, CRYPTO_matte->u.bd.value);
			
			if(format == PF_PixelFormat_ARGB128)
			{
				err = suites.PFIterate8Suite()->iterate_generic(copy_height, &merge_iter, Merge_Iterate<PF_PixelFloat, PF_FpShort>);
			}
			else if(format == PF_PixelFormat_ARGB64)
			{
				err = suites.PFIterate8Suite()->iterate_generic(copy_height, &merge_iter, Merge_Iterate<PF_Pixel16, A_u_short>);
			}
			else if(format == PF_PixelFormat_ARGB32)
			{
				err = suites.PFIterate8Suite()->iterate_generic(copy_height, &merge_iter, Merge_Iterate<PF_Pixel, A_u_char>);
			}
		}
		else
		{
			if(in_data->quality == PF_Quality_HI)
				err = suites.PFWorldTransformSuite()->copy_hq(in_data->effect_ref, input, output, NULL, NULL);
			else
				err = suites.PFWorldTransformSuite()->copy(in_data->effect_ref, input, output, NULL, NULL);
		}
	}
	catch(ErrThrower &e)
	{
		ae_err = e.err();
	}
	catch(...)
	{
		ae_err = PF_Err_BAD_CALLBACK_PARAM; 
	}
	
	//if(tempAlphaWorld)
	//	suites.PFWorldSuite()->PF_DisposeWorld(in_data->effect_ref, tempAlphaWorld);
	
	if(alphaWorld)
		suites.PFWorldSuite()->PF_DisposeWorld(in_data->effect_ref, alphaWorld);
	
	
	PF_UNLOCK_HANDLE(CRYPTO_data->u.arb_d.value);
	PF_UNLOCK_HANDLE(in_data->sequence_data);
	
	
	return ae_err;
}


static PF_Err
SmartRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_SmartRenderExtra		*extra)

{
	PF_Err			err		= PF_Err_NONE,
					err2 	= PF_Err_NONE;
					
	PF_EffectWorld *input, *output;
	
	PF_ParamDef CRYPTO_data,
				CRYPTO_matte;

	// zero-out parameters
	AEFX_CLR_STRUCT(CRYPTO_data);
	AEFX_CLR_STRUCT(CRYPTO_matte);
	
	
#define PF_CHECKOUT_PARAM_NOW( PARAM, DEST )	PF_CHECKOUT_PARAM(	in_data, (PARAM), in_data->current_time, in_data->time_step, in_data->time_scale, DEST )

	// get our arb data and see if it requires the input buffer
	err = PF_CHECKOUT_PARAM_NOW(CRYPTO_DATA, &CRYPTO_data);
	
	if(!err)
	{
		CryptomatteArbitraryData *arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(CRYPTO_data.u.arb_d.value);

		if(true)
		{
			err = extra->cb->checkout_layer_pixels(in_data->effect_ref, CRYPTO_INPUT, &input);
		}
		else
		{
			input = NULL;
		}
		
		PF_UNLOCK_HANDLE(CRYPTO_data.u.arb_d.value);
	}
	
	
	// always get the output buffer
	ERR(	extra->cb->checkout_output(	in_data->effect_ref, &output)	);


	// checkout the required params
	ERR(	PF_CHECKOUT_PARAM_NOW( CRYPTO_MATTE_ONLY,	&CRYPTO_matte )	);

	ERR(DoRender(	in_data, 
					input, 
					&CRYPTO_data,
					&CRYPTO_matte,
					out_data, 
					output));

	// Always check in, no matter what the error condition!
	ERR2(	PF_CHECKIN_PARAM(in_data, &CRYPTO_data )	);
	ERR2(	PF_CHECKIN_PARAM(in_data, &CRYPTO_matte )	);


	return err;
}


PF_Err 
DoDialog(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	A_Err			err 		= A_Err_NONE;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_ChannelSuite *cs = suites.PFChannelSuite();


	A_long chan_count = 0;
	cs->PF_GetLayerChannelCount(in_data->effect_ref, 0, &chan_count);

	if(chan_count == 0 || err)
	{
		PF_SPRINTF(out_data->return_msg, "No auxiliary channels available.");
	}
	else
	{
		CryptomatteArbitraryData *arb = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
		
		#ifdef MAC_ENV
			const char *plugHndl = "com.fnordware.AfterEffects.Cryptomatte";
			const void *mwnd = NULL;
		#else
			const char *plugHndl = NULL;
			HWND *mwnd = NULL;
			PF_GET_PLATFORM_DATA(PF_PlatData_MAIN_WND, &mwnd);
		#endif
		
		std::string layer = GetLayer(arb);
		std::string selection = GetSelection(arb);
		std::string manifest =GetManifest(arb);
		
		const bool clicked_ok = Cryptomatte_Dialog(layer, selection, manifest, plugHndl, mwnd);
		
		if(clicked_ok)
		{
			SetArb(in_data, &params[CRYPTO_DATA]->u.arb_d.value, layer, selection, manifest);
			
			params[CRYPTO_DATA]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
		}
		
		PF_UNLOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
		//PF_UNLOCK_HANDLE(in_data->sequence_data);
	}
	
	return err;
}


DllExport	
PF_Err 
PluginMain (	
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try	{
		switch (cmd) {
			case PF_Cmd_ABOUT:
				err = About(in_data,out_data,params,output);
				break;
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(in_data,out_data,params,output);
				break;
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(in_data,out_data,params,output);
				break;
			case PF_Cmd_SEQUENCE_SETUP:
			case PF_Cmd_SEQUENCE_RESETUP:
				err = SequenceSetup(in_data, out_data, params, output);
				break;
			case PF_Cmd_SEQUENCE_FLATTEN:
				err = SequenceFlatten(in_data, out_data, params, output);
				break;
			case PF_Cmd_SEQUENCE_SETDOWN:
				err = SequenceSetdown(in_data, out_data, params, output);
				break;
			//case PF_Cmd_USER_CHANGED_PARAM:
			//	err = UserChangedParam(in_data, out_data, params, (PF_UserChangedParamExtra *)extra);
			//	break;
			case PF_Cmd_SMART_PRE_RENDER:
				err = PreRender(in_data, out_data, (PF_PreRenderExtra *)extra);
				break;
			case PF_Cmd_SMART_RENDER:
				err = SmartRender(in_data, out_data, (PF_SmartRenderExtra *)extra);
				break;
			case PF_Cmd_EVENT:
				err = HandleEvent(in_data, out_data, params, output, (PF_EventExtra	*)extra);
				break;
			case PF_Cmd_DO_DIALOG:
				err = DoDialog(in_data, out_data, params, output);
				break;	
			case PF_Cmd_ARBITRARY_CALLBACK:
				err = HandleArbitrary(in_data, out_data, params, output, (PF_ArbParamsExtra	*)extra);
				break;
		}
	}
	catch(PF_Err &thrown_err) { err = thrown_err; }
	catch(...) { err = PF_Err_INTERNAL_STRUCT_DAMAGED; }
	
	return err;
}
