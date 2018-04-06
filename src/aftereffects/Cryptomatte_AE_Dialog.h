//
//	EXtractoR
//		by Brendan Bolles <brendan@fnordware.com>
//
//	extract float channels
//
//	Part of the fnord OpenEXR tools
//		http://www.fnordware.com/OpenEXR/
//
//

#pragma once

#ifndef _CRYPTOMATTE_DIALOG_H_
#define _CRYPTOMATTE_DIALOG_H_

#include "Cryptomatte_AE.h"

//#include <ImfChannelList.h>

#include <string>
//#include <vector>
//#include <map>
//
//typedef std::vector<std::string> ChannelVec;
//typedef std::map<std::string, ChannelVec> LayerMap;

bool Cryptomatte_Dialog(
	std::string			&layer,
	std::string			&selection,
	std::string			&manifest,
	const char			*plugHndl,
	const void			*mwnd);


#endif // _CRYPTOMATTE_DIALOG_H_