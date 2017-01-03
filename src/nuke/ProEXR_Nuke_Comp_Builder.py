# ProEXR Nuke Comp Builder
# 	by Brendan Bolles
# 	www.fnordware.com
#
#	ProEXR is a Photoshop plug-in that will save the layers in a Photoshop document
#	to a single multi-channel OpenEXR file.  But while Photoshop has notions of
#	layers that have a stacking order, transfer modes, opacity, etc, OpenEXR only
#	holds an unordered list of channels.
#
#	To pass along information about Photoshop's layers, ProEXR embeds an EXR string
#	attribute called "PSlayers" with that information.  Nuke 6.3 and later reads
#	this metadata into its data stream, which can then be used to reconstuct the
#	Photoshop composite.  That's what this script does.
#
#	It has not been thoroughly tested on every possible EXR/Nuke scenario.  If
#	you encounter issues or have suggestions, pass them on!
#
#	Thanks to Josh LaCross for the idea, and for valuable feedback.
#


import nuke

#
#	Like regular tokenize functions, but also allows you to surround phrases with
#	quotes and have them treated as whole units.
#
#	'hello there big boy' -> {'hello', 'there', 'big', 'boy'}
#	'hello there "big boy" -> {'hello', 'there', 'big boy'}
#
def quotedTokenize(string, tokens=' \t'):
	token_array = []

	in_quotes = False

	i = 0

	# if there are unquoted delimiters in the beginning, skip them
	while (i < len(string)) and (string[i] != '\"') and (string[i] in tokens):
		i += 1

	token_begin = i
	
	while i < len(string):
		if string[i] == '\"' and ((i == 0) or (string[i-1] != '\\')):
			in_quotes = not in_quotes
		elif not in_quotes:
			if string[i] in tokens:
				token_array.append(string[token_begin : i])

				token_begin = i + 1

				# if there are moe delimiters, push forward
				while (token_begin < len(string)) and (string[token_begin] != '\"' or (string[token_begin] == '\\')) and (string[token_begin] in tokens):
					token_begin += 1

				i = token_begin
				continue

		i += 1
	
	# at the end now, there's probably one more string to append
	if token_begin < len(string):
		token_array.append(string[token_begin : ])

	return token_array


#
#	The PSlayers string is a series of these separated by commas:
#
#	diffuse[visible:true, mode:Normal]{r:diffuse.R, g:diffuse.G, b:diffuse.B, a:diffuse.A}
#
#	This function does a tokenizes the string, but ignores delimiters inside the {}[] stuff
#
def tokenizePSlayers(layers_string):
	layer_string_array = []

	in_quotes = False
	in_brackets = False
	in_braces = False

	layer_begin = 0
	i = 0

	while i < len(layers_string):
		if layers_string[i] == '\"' and ((i == 0) or (layers_string[i-1] != '\\')):
			in_quotes = not in_quotes
		else:
			if not in_quotes:
				if layers_string[i] == '[':
					in_brackets = True
				elif layers_string[i] == ']':
					in_brackets = False
				elif layers_string[i] == '{':
					in_braces = True
				elif layers_string[i] == '}':
					in_braces = False
				else:
					if not in_brackets and not in_braces and layers_string[i] == ',':
						layer_string = layers_string[layer_begin : i]

						if len(quotedTokenize(layer_string, '[]{}')) == 3:
							layer_string_array.append(layer_string)

						# find next layer string, if any
						next = i
						while (layers_string[next] in [',', ' ']) and (next < len(layers_string)):
							next += 1

						if next < len(layers_string):
							i = layer_begin = next
							continue
 
		i += 1

	# should have one last layer string that doesn't end with a comma
	if (len(layers_string) - layer_begin) >= 10:
		layer_string = layers_string[layer_begin : ]
		
		if len(quotedTokenize(layer_string, '[]{}')) == 3:
			layer_string_array.append(layer_string)

	return layer_string_array


#
#	"hello" -> hello
#	"hello \"mate\"" -> hello "mate"
#
def deQuote(string):
	start_pos = 0
	end_pos = len(string)

	if string[0] == '\"':
		start_pos = 1
		
		if string[end_pos - 1] == '\"':
			end_pos = end_pos - 1

	return string[start_pos : end_pos].replace('\\\"', '\"')


#
#	"animal:parrot, alive:false" -> {'animal':'parrot', 'alive':'false'}
#						 
def makeDictFromColonList(string):
	the_dict = {}

	pair_array = quotedTokenize(string, tokens=', ')

	for pair in pair_array:
		split_pair = pair.split(':')

		if len(split_pair) == 2:
			the_dict[ deQuote(split_pair[0]) ] = deQuote( split_pair[1] )

	return the_dict


#
#	Turns the whole PSlayers string into an array of dictionaries with information
#	about each layer.
#
def parsePSlayers(layers_string):
	layer_array = []

	layer_string_array = tokenizePSlayers(layers_string)
	
	for layer_string in layer_string_array:
		layer_tokens = quotedTokenize(layer_string, tokens='[]{}')
		
		if len(layer_tokens) == 3:
			layer_dict = {'name' : deQuote(layer_tokens[0]),
							'properties' : makeDictFromColonList(layer_tokens[1]),
							'channels' : makeDictFromColonList(layer_tokens[2]) }

			layer_array.append(layer_dict)

	return layer_array


#
#	'diffuse.R' -> 'diffuse'
#
def getLayerNameFromChannelName(channel_name):
	end_of_layer = channel_name.rfind('.')

	if end_of_layer == -1:
		end_of_layer = len(channel_name)

	return channel_name[ : end_of_layer]


#
#	OpenEXR channels are called things like "diffuse.R"
#	Here we try to guess what Nuke will call them, hopefully "diffuse"
#
def getLayerName(layer_dict):
	layer_name = layer_dict['name']

	red_layer_name = getLayerNameFromChannelName( layer_dict['channels'].get('r', 'RGB') )

	if layer_name in ('RGB', 'RGBA', 'rgb', 'rgba'):
		layer_name = 'rgba'
	elif layer_name != red_layer_name:
		layer_name = red_layer_name
		
	# There are many illegal characters that Nuke will replace with an underscore
	# But really, it's better to just avoid these
	illegal = ' !@#$%^&*()-+=;:<>,\'"'
	
	for i in range(0, len(layer_name)):
		if layer_name[i] in illegal:
			layer_name = layer_name.replace(layer_name[i], '_')

	return layer_name;


#
# Convert from Photoshop's transfer modes to Nuke's.
# Not all have a good correlation; we default to over.
# Feel free to experiement with matching them yourself.
#
def PhotoshopToNukeMode(mode):
	mode_dict = {'Normal' : 'over',
				#'Dissolve' : 
				'Darken' : 'darken',
				'Multiply' : 'mult',
				'ColorBurn' : 'color burn',
				#'LinearBurn'
				#'DarkerColor'
				'Lighten' : 'lighten',
				'Screen' : 'scrn',
				'ColorDodge' : 'color dodge',
				'LinearDodge' : 'plus', # Nuke's Linear Dodge is different from Photoshop's, which is just Add
				#'LighterColor'
				'Overlay' : 'overlay',
				'SoftLight' : 'soft light',
				'HardLight' : 'hard light',
				#'VividLight' # Nuke's vivid light is nothing like Photoshop's
				'LinearLight' : 'linear light',
				#'PinLight'
				#'HardMix'
				'Difference' : 'difference',
				'Exclusion' : 'exclusion',
				#'Hue'
				#'Saturation'
				#'Color'
				#'Luminosity'
				#'DancingDissolve'
				'ClassicColorBurn' : 'color burn',
				'Add' : 'plus',
				'ClassicColorDodge' : 'color dodge',
				'ClassicDifference' : 'difference',
				'StencilAlpha' : 'mask',
				#'StencilLuma'
				'SilhouetteAlpha' : 'stencil',
				#'SilhouetteLuma'
				#'AlphaAdd'
				'LuminescentPremul' : 'over',
				'Subtract' : 'from',
				'Divide' : 'divide' }

	return mode_dict.get(mode, 'over')




#
# starts here
#
def ProEXR_Nuke_Comp_Builder():
	if len( nuke.selectedNodes() ) > 0:
		root_node = nuke.selectedNode()
	
		available_layers = nuke.layers(root_node)
	
		layers_string = root_node.metadata().get('exr/PSlayers', None)
	
		if layers_string is not None:
			layer_dict_array = parsePSlayers(layers_string)
			
			# Shuffle nodes get the channels out
			for i in range(0, len(layer_dict_array)):
				layer_dict = layer_dict_array[i]
				layer_name = getLayerName(layer_dict_array[i])
	
				# checking the layer names we have against Nuke's
				# no guarantee we'll get them all
				if layer_name in available_layers:
					# new Shuffle node
					shuffle_node = nuke.nodes.Shuffle(label = layer_dict['name'])
	
					# silly thing we need for PLE (or else we hit the node access limit and get errors)
					if nuke.env['ple']:
						nuke.message('PLE says wait!')
	
					shuffle_node.setXYpos(root_node.xpos() + (i * 100), root_node.ypos() + 150)
					shuffle_node.setInput(0, root_node)
					shuffle_node['in'].setValue( layer_name )
					shuffle_node['postage_stamp'].setValue(True)
	
					layer_dict['shuffle_node'] = shuffle_node
			
					# Remove nodes 
					remove_node = nuke.nodes.Remove()
					
					remove_node.setXYpos(shuffle_node.xpos(), shuffle_node.ypos() + 85)
					remove_node.setInput(0, shuffle_node)
					remove_node['operation'].setValue('keep')
					remove_node['channels'].setValue('rgba')
					
					layer_dict['remove_node'] = remove_node
			
			# Merge nodes
			if len(layer_dict_array) >= 2:
				last_merge = layer_dict_array[0].get('remove_node', None)
		
				for i in range(1, len(layer_dict_array)):
					layer_dict = layer_dict_array[i]
					
					# new Merge node
					new_merge = nuke.nodes.Merge()
					
					# silly thing we need for PLE (or else we hit the node access limit and get errors)
					if nuke.env['ple']:
						nuke.message('PLE says wait!')
	
					remove_node = layer_dict.get('remove_node', None)
					
					if last_merge is not None:
						new_merge.setInput(0, last_merge)
	
					if remove_node is not None:
						new_merge.setInput(1, remove_node)
						
						new_merge.setXYpos(remove_node.xpos(), remove_node.ypos() + 100)				   
	
					# set merge parameters based on Photoshop transfer modes, opacity, etc
					if layer_dict['properties'].get('visible', 'true') == 'false':
						new_merge['disable'].setValue(True)
	
					new_merge['operation'].setValue( PhotoshopToNukeMode(layer_dict['properties'].get('mode', 'Normal') ) )
					
					# special case for Mult - in Photoshop, nothing with black alpha will get multed
					if new_merge['operation'].value() == 'mult' and remove_node is not None:
						new_merge.setInput(2, remove_node)
					
	
					opacity = int( layer_dict['properties'].get('opacity', '255') )
	
					if opacity != 255:
						new_merge['mix'].setValue( float(opacity) / 255.0 )
	
					last_merge = new_merge
	
		else:
			# no PSlayers metadata, but we do have some channels, so we'll do our best
			nuke.message('No ProEXR PSlayers data available, using channel data instead')
	
			shuffle_array = []
	
			for i in range(0, len(available_layers)):
				layer = available_layers[i]
	
				shuffle_node = nuke.nodes.Shuffle(name = layer)
	
				# silly thing we need for PLE (or else we hit the node access limit and get errors)
				if nuke.env['ple']:
					nuke.message('PLE says wait!')
	
				shuffle_node.setXYpos(root_node.xpos() + (i * 100), root_node.ypos() + 150)
				shuffle_node.setInput(0, root_node)
				shuffle_node['in'].setValue( layer )
				shuffle_node['postage_stamp'].setValue(True)
	
				shuffle_array.append(shuffle_node)
	
			# decided merging doesn't really make sense if we don't know anything about the layers
			# un-comment if you disagree		   
			#if len(shuffle_array) >= 2:
			#	last_merge = shuffle_array[0]
			#
			#	for i in range(1, len(shuffle_array)):
			#		new_merge = nuke.nodes.Merge()
			#
			#		# silly thing we need for PLE (or else we hit the node access limit and get errors)
			#		nuke.env['ple']:
			#			nuke.message('PLE says wait!')
			#
			#		new_merge.setInput(0, last_merge)
			#		new_merge.setInput(1, shuffle_array[i])
			#
			#		new_merge.setXYpos(shuffle_array[i].xpos(), shuffle_array[i].ypos() + (i * 50))
			#
			#		last_merge = new_merge
				
	else:
		nuke.message('No node selected.')



if __name__ == "__main__":
	ProEXR_Nuke_Comp_Builder()
	