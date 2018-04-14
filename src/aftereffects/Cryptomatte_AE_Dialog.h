//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//

#pragma once

#ifndef _CRYPTOMATTE_DIALOG_H_
#define _CRYPTOMATTE_DIALOG_H_

#include "Cryptomatte_AE.h"

#include <string>

bool Cryptomatte_Dialog(
	std::string			&layer,
	std::string			&selection,
	std::string			&manifest,
	const char			*plugHndl,
	const void			*mwnd);


#endif // _CRYPTOMATTE_DIALOG_H_
