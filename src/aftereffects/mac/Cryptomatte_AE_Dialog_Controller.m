//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//

#import "Cryptomatte_AE_Dialog_Controller.h"

@implementation Cryptomatte_AE_Dialog_Controller

- (id)init:(NSString *)layer
	selection:(NSString *)selection
	manifest:(NSString *)manifest
{
	self = [super initWithWindowNibName:@"Cryptomatte_AE_Dialog"];
	
	if(self)
	{
		if(!([NSBundle loadNibNamed:@"Cryptomatte_AE_Dialog" owner:self]))
			return nil;
		
		[[self window] center];
		
		[layerField setStringValue:layer];
		[selectionField setStringValue:selection];
		[manifestField setStringValue:manifest];
	}
	
	return self;
}

- (IBAction)clickOK:(NSButton *)sender {
    [NSApp stopModal];
}

- (IBAction)clickCancel:(NSButton *)sender {
    [NSApp abortModal];
}

- (NSString *)getLayer {
	return [layerField stringValue];
}

- (NSString *)getSelection {
	return [selectionField stringValue];
}

- (NSString *)getManifest {
	return [manifestField stringValue];
}

@end
