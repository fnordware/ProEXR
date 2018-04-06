//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//

#import <Cocoa/Cocoa.h>

#if !NSINTEGER_DEFINED
typedef int NSInteger;
#define NSINTEGER_DEFINED 1
#endif

@interface Cryptomatte_AE_Dialog_Controller : NSWindowController {
	IBOutlet NSTextField *layerField;
    IBOutlet NSTextField *selectionField;
    IBOutlet NSTextField *manifestField;
	IBOutlet NSWindow *window;
}

- (id)init:(NSString *)layer
	selection:(NSString *)selection
	manifest:(NSString *)manifest;

- (IBAction)clickOK:(NSButton *)sender;
- (IBAction)clickCancel:(NSButton *)sender;

- (NSString *)getLayer;
- (NSString *)getSelection;
- (NSString *)getManifest;

@end

