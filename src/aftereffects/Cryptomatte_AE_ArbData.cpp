//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//
//	mmm...arbitrary
//
//

#include "Cryptomatte_AE.h"

#include "MurmurHash3.h"

#include <assert.h>


#ifndef SWAP_LONG
#define SWAP_LONG(a)		((a >> 24) | ((a >> 8) & 0xff00) | ((a << 8) & 0xff0000) | (a << 24))
#endif

static void 
SwapArbData(CryptomatteArbitraryData *arb_data)
{
	arb_data->manifest_size = SWAP_LONG(arb_data->manifest_size);
	arb_data->manifest_hash = SWAP_LONG(arb_data->manifest_hash);
	arb_data->selection_size = SWAP_LONG(arb_data->selection_size);
	arb_data->selection_size = SWAP_LONG(arb_data->selection_size);
}


static void
HashManifest(CryptomatteArbitraryData *arb)
{
	MurmurHash3_x86_32(&arb->data[0], arb->manifest_size, 0, &arb->manifest_hash);
}


static void
HashSelection(CryptomatteArbitraryData *arb)
{
	MurmurHash3_x86_32(&arb->data[arb->manifest_size], arb->selection_size, 0, &arb->selection_hash);
}


const char *
GetLayer(const CryptomatteArbitraryData *arb)
{
	return arb->layer;
}


const char *
GetSelection(const CryptomatteArbitraryData *arb)
{
	return &arb->data[arb->manifest_size];
}


const char *
GetManifest(const CryptomatteArbitraryData *arb)
{
	return &arb->data[0];
}


void
SetArb(PF_InData *in_data, PF_ArbitraryH *arbH, const std::string &l, const std::string &selection, const std::string &manifest)
{
	assert(sizeof(CryptomatteArbitraryData) == 116);
	
	const size_t siz = sizeof(CryptomatteArbitraryData) + manifest.size() + selection.size();
	
	if(*arbH == NULL)
	{
		*arbH = PF_NEW_HANDLE(siz);
	}
	else
	{
		const size_t handle_siz = PF_GET_HANDLE_SIZE(*arbH);
		
		if(siz != handle_siz)
			PF_RESIZE_HANDLE(siz, arbH);
	}
	
	CryptomatteArbitraryData *arb = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(*arbH);
	
	arb->magic[0] = 'c';
	arb->magic[1] = 'r';
	arb->magic[2] = 'y';
	arb->magic[3] = '1';

	std::string layer = l;
	
	if(layer.size() > MAX_LAYER_NAME_LEN)
		layer.resize(MAX_LAYER_NAME_LEN);
	
	strncpy(arb->layer, layer.c_str(), MAX_LAYER_NAME_LEN + 1);
	
	arb->manifest_size = manifest.size() + 1;
	arb->selection_size = selection.size() + 1;
	
	strncpy(&arb->data[0], manifest.c_str(), arb->manifest_size);
	strncpy(&arb->data[arb->manifest_size], selection.c_str(), arb->selection_size);
	
	assert(manifest == GetManifest(arb));
	assert(selection == GetSelection(arb));
	
	HashManifest(arb);
	HashSelection(arb);
	
	PF_UNLOCK_HANDLE(*arbH);
}


void
SetArbSelection(PF_InData *in_data, PF_ArbitraryH *arbH, const std::string &selection)
{
	CryptomatteArbitraryData *arb = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(*arbH);
	
	const size_t siz = sizeof(CryptomatteArbitraryData) + arb->manifest_size + selection.size();
	
	const size_t handle_siz = PF_GET_HANDLE_SIZE(*arbH);
	
	if(siz != handle_siz)
	{
		PF_UNLOCK_HANDLE(*arbH);
		
		PF_RESIZE_HANDLE(siz, arbH);
		
		arb = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(*arbH);
	}
	
	arb->selection_size = selection.size() + 1;
	
	strncpy(&arb->data[arb->manifest_size], selection.c_str(), arb->selection_size);
	
	HashSelection(arb);
	
	PF_UNLOCK_HANDLE(*arbH);
}


PF_Err
ArbNewDefault(PF_InData *in_data, PF_OutData *out_data,
	void				*refconPV,
	PF_ArbitraryH		*arbPH)
{
	PF_Err err = PF_Err_NONE;
	
	if(arbPH)
	{
		SetArb(in_data, arbPH, "", "", "");
	}
	
	return err;
}


static PF_Err
ArbDispose(PF_InData *in_data, PF_OutData *out_data,
	void				*refconPV,
	PF_ArbitraryH		arbH)
{
	if(arbH)
		PF_DISPOSE_HANDLE(arbH);
	
	return PF_Err_NONE;
}


static PF_Err
ArbCopy(PF_InData *in_data, PF_OutData *out_data,
	void				*refconPV,
	PF_ArbitraryH		src_arbH,
	PF_ArbitraryH 		*dst_arbPH)
{
	PF_Err 	err 	= PF_Err_NONE;
	
	if(src_arbH && dst_arbPH)
	{
		// allocate using the creation function
		err = ArbNewDefault(in_data, out_data, refconPV, dst_arbPH);
		
		if(!err)
		{
			const A_u_longlong siz = PF_GET_HANDLE_SIZE(src_arbH);
			
			PF_RESIZE_HANDLE(siz, dst_arbPH);
		
			CryptomatteArbitraryData *in_arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(src_arbH),
							*out_arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(*dst_arbPH);
			
			assert(in_arb_data->magic[0] == 'c' &&	in_arb_data->magic[1] == 'r' &&
					in_arb_data->magic[2] == 'y' && in_arb_data->magic[3] == '1');
				
			assert(out_arb_data->magic[0] == 'c' &&	out_arb_data->magic[1] == 'r' &&
					out_arb_data->magic[2] == 'y' && out_arb_data->magic[3] == '1');
					
			memcpy(out_arb_data, in_arb_data, siz);
			
		#ifndef NDEBUG
			HashManifest(out_arb_data);
			HashSelection(out_arb_data);
			
			assert(out_arb_data->manifest_hash == in_arb_data->manifest_hash);
			assert(out_arb_data->selection_hash == in_arb_data->selection_hash);
		#endif
			
			PF_UNLOCK_HANDLE(src_arbH);
			PF_UNLOCK_HANDLE(*dst_arbPH);
		}
	}
	
	return err;
}


static PF_Err
ArbFlatSize(PF_InData *in_data, PF_OutData *out_data,
	void				*refconPV,
	PF_ArbitraryH		arbH,
	A_u_long			*flat_data_sizePLu)
{
	// flat is the same size as inflated
	if(arbH)
		*flat_data_sizePLu = PF_GET_HANDLE_SIZE(arbH);
	
	return PF_Err_NONE;
}


static PF_Err
ArbFlatten(PF_InData *in_data, PF_OutData *out_data,
	void				*refconPV,
	PF_ArbitraryH		arbH,
	A_u_long			buf_sizeLu,
	void				*flat_dataPV)
{
	PF_Err 	err 	= PF_Err_NONE;
	
	if(arbH && flat_dataPV)
	{
		// they provide the buffer, we just move data
		CryptomatteArbitraryData *in_arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(arbH),
						*out_arb_data = (CryptomatteArbitraryData *)flat_dataPV;

		assert(buf_sizeLu >= PF_GET_HANDLE_SIZE(arbH));
	
		memcpy(out_arb_data, in_arb_data, PF_GET_HANDLE_SIZE(arbH));
		
	#ifdef AE_BIG_ENDIAN
		// really, you're compiling this for PPC?
		SwapArbData(out_arb_data);
	#endif
	
		PF_UNLOCK_HANDLE(arbH);
	}
	
	return err;
}


static PF_Err
ArbUnFlatten(PF_InData *in_data, PF_OutData *out_data,
	void				*refconPV,
	A_u_long			buf_sizeLu,
	const void			*flat_dataPV,
	PF_ArbitraryH		*arbPH)
{
	PF_Err 	err 	= PF_Err_NONE;
	
	if(arbPH && flat_dataPV)
	{
		// they provide a flat buffer, we have to make the handle (using the default function)
		err = ArbNewDefault(in_data, out_data, refconPV, arbPH);
		
		if(!err && *arbPH)
		{
			PF_RESIZE_HANDLE(buf_sizeLu, arbPH);
			
			CryptomatteArbitraryData *in_arb_data = (CryptomatteArbitraryData *)flat_dataPV,
							*out_arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(*arbPH);
			
			
			assert(in_arb_data->magic[0] == 'c' &&	in_arb_data->magic[1] == 'r' &&
					in_arb_data->magic[2] == 'y' && in_arb_data->magic[3] == '1');
				
			assert(out_arb_data->magic[0] == 'c' &&	out_arb_data->magic[1] == 'r' &&
					out_arb_data->magic[2] == 'y' && out_arb_data->magic[3] == '1');
			assert(buf_sizeLu <= PF_GET_HANDLE_SIZE(*arbPH));
		
			memcpy(out_arb_data, in_arb_data, buf_sizeLu);
			
		#ifdef AE_BIG_ENDIAN
			// swap bytes back to PPC style (?)
			SwapArbData(out_arb_data);
		#endif
		
		#ifndef NDEBUG
			const Hash old_manifest_hash = out_arb_data->manifest_hash;
			const Hash old_selection_hash = out_arb_data->selection_hash;
		#endif
			
			HashManifest(out_arb_data);
			HashSelection(out_arb_data);
			
		#ifndef NDEBUG
			assert(old_manifest_hash == out_arb_data->manifest_hash);
			assert(old_selection_hash == out_arb_data->selection_hash);
		#endif
		
			PF_UNLOCK_HANDLE(*arbPH);
		}
	}
	
	return err;
}

static PF_Err
ArbInterpolate(PF_InData *in_data, PF_OutData *out_data,
	void				*refconPV,
	PF_ArbitraryH		left_arbH,
	PF_ArbitraryH		right_arbH,
	PF_FpLong			tF,
	PF_ArbitraryH		*interpPH)
{
	PF_Err 	err 	= PF_Err_NONE;
	
	assert(FALSE); // we shouldn't be doing this in Cryptomatte - we said we didn't interpolate
	
	if(left_arbH && right_arbH && interpPH)
	{
		// allocate using our own func
		err = ArbNewDefault(in_data, out_data, refconPV, interpPH);
		
		if(!err && *interpPH)
		{
			const A_u_longlong siz = PF_GET_HANDLE_SIZE(left_arbH);
			
			PF_RESIZE_HANDLE(siz, interpPH);
			
			// we're just going to copy the left_data
			CryptomatteArbitraryData *in_arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(left_arbH),
							*out_arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(*interpPH);
			
			memcpy(out_arb_data, in_arb_data, siz);
			
			PF_UNLOCK_HANDLE(left_arbH);
			PF_UNLOCK_HANDLE(*interpPH);
		}
	}
	
	return err;
}


static PF_Err
ArbCompare(PF_InData *in_data, PF_OutData *out_data,
	void				*refconPV,
	PF_ArbitraryH		a_arbH,
	PF_ArbitraryH		b_arbH,
	PF_ArbCompareResult	*compareP)
{
	PF_Err 	err 	= PF_Err_NONE;
	
	if(a_arbH && b_arbH)
	{
		CryptomatteArbitraryData *a_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(a_arbH),
						*b_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(b_arbH);

		assert(!strncmp(a_data->magic, "cry1", 4) && !strncmp(b_data->magic, "cry1", 4));
		
		if(!strncmp(GetLayer(a_data), GetLayer(b_data), MAX_LAYER_NAME_LEN + 1) && 
			a_data->manifest_hash == b_data->manifest_hash &&
			a_data->selection_hash == b_data->selection_hash)
		{
			*compareP = PF_ArbCompare_EQUAL;
		}
		else
			*compareP = PF_ArbCompare_NOT_EQUAL;
		
		
		PF_UNLOCK_HANDLE(a_arbH);
		PF_UNLOCK_HANDLE(b_arbH);
	}
	
	return err;
}


PF_Err 
HandleArbitrary(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output,
	PF_ArbParamsExtra	*extra)
{
	PF_Err 	err 	= PF_Err_NONE;
	
	if(extra->id == ARBITRARY_DATA_ID)
	{
		switch(extra->which_function)
		{
			case PF_Arbitrary_NEW_FUNC:
				err = ArbNewDefault(in_data, out_data, extra->u.new_func_params.refconPV, extra->u.new_func_params.arbPH);
				break;
			case PF_Arbitrary_DISPOSE_FUNC:
				err = ArbDispose(in_data, out_data, extra->u.dispose_func_params.refconPV, extra->u.dispose_func_params.arbH);
				break;
			case PF_Arbitrary_COPY_FUNC:
				err = ArbCopy(in_data, out_data, extra->u.copy_func_params.refconPV, extra->u.copy_func_params.src_arbH, extra->u.copy_func_params.dst_arbPH);
				break;
			case PF_Arbitrary_FLAT_SIZE_FUNC:
				err = ArbFlatSize(in_data, out_data, extra->u.flat_size_func_params.refconPV, extra->u.flat_size_func_params.arbH, extra->u.flat_size_func_params.flat_data_sizePLu);
				break;
			case PF_Arbitrary_FLATTEN_FUNC:
				err = ArbFlatten(in_data, out_data, extra->u.flatten_func_params.refconPV, extra->u.flatten_func_params.arbH, extra->u.flatten_func_params.buf_sizeLu, extra->u.flatten_func_params.flat_dataPV);
				break;
			case PF_Arbitrary_UNFLATTEN_FUNC:
				err = ArbUnFlatten(in_data, out_data, extra->u.unflatten_func_params.refconPV, extra->u.unflatten_func_params.buf_sizeLu, extra->u.unflatten_func_params.flat_dataPV, extra->u.unflatten_func_params.arbPH);
				break;
			case PF_Arbitrary_INTERP_FUNC:
				err = ArbInterpolate(in_data, out_data, extra->u.interp_func_params.refconPV, extra->u.interp_func_params.left_arbH, extra->u.interp_func_params.right_arbH, extra->u.interp_func_params.tF, extra->u.interp_func_params.interpPH);
				break;
			case PF_Arbitrary_COMPARE_FUNC:
				err = ArbCompare(in_data, out_data, extra->u.compare_func_params.refconPV, extra->u.compare_func_params.a_arbH, extra->u.compare_func_params.b_arbH, extra->u.compare_func_params.compareP);
				break;
			// these are necessary for copying and pasting keyframes
			// for now, we better not be called to do this
			case PF_Arbitrary_PRINT_SIZE_FUNC:
				assert(FALSE); //err = ArbPrintSize(in_data, out_data, extra->u.print_size_func_params.refconPV, extra->u.print_size_func_params.arbH, extra->u.print_size_func_params.print_sizePLu);
				break;
			case PF_Arbitrary_PRINT_FUNC:
				assert(FALSE); //err = ArbPrint(in_data, out_data, extra->u.print_func_params.refconPV, extra->u.print_func_params.print_flags, extra->u.print_func_params.arbH, extra->u.print_func_params.print_sizeLu, extra->u.print_func_params.print_bufferPC);
				break;
			case PF_Arbitrary_SCAN_FUNC:
				assert(FALSE); //err = ArbScan(in_data, out_data, extra->u.scan_func_params.refconPV, extra->u.scan_func_params.bufPC, extra->u.scan_func_params.bytes_to_scanLu, extra->u.scan_func_params.arbPH);
				break;
		}
	}
	
	
	return err;
}
