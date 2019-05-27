/*******************************************************************
Purpose: Image brighten with C, assembly, MMX

Reference guide:

http://www.plantation-productions.com/Webster/www.artofasm.com/Windows/HTML/AoATOC.html

Note: above guide uses format <inst>(<source>, <dest>) while our compiler
requires <inst> <dest>, <source>

*******************************************************************/

#include <windows.h>
#include <mmsystem.h>
#include <string>
LARGE_INTEGER startTime;
LARGE_INTEGER endTime;
double time;
HBITMAP hBitmap;
BITMAP Bitmap;
BITMAPFILEHEADER bmfh;
BITMAPINFO     * pbmi;
BYTE* pBits;
HINSTANCE ghInstance;
HWND timeDisplayer;
std::string timeString;

LRESULT CALLBACK HelloWndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR szCMLine, int iCmdShow){
	ghInstance = hInstance;
	static TCHAR szAppName[] = TEXT("HelloApplication");//name of app
	HWND	hwnd;//holds handle to the main window
	MSG		msg;//holds any message retrieved from the msg queue
	WNDCLASS wndclass;//wnd class for registration

	//defn wndclass attributes for this application
	wndclass.style = CS_HREDRAW | CS_VREDRAW;//redraw on refresh both directions
	wndclass.lpfnWndProc = HelloWndProc;//wnd proc to handle windows msgs/commands
	wndclass.cbClsExtra = 0;//class space for expansion/info carrying
	wndclass.cbWndExtra = 0;//wnd space for info carrying
	wndclass.hInstance = hInstance;//application instance handle
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);//set icon for window
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);//set cursor for window
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);//set background
	wndclass.lpszMenuName = NULL;//set menu
	wndclass.lpszClassName = szAppName;//set application name

	//register wndclass to O/S so approp. wnd msg are sent to application
	if (!RegisterClass(&wndclass)){
		MessageBox(NULL, TEXT("This program requires Windows 95/98/NT"),
			szAppName, MB_ICONERROR);//if unable to be registered
		return 0;
	}
	//create the main window and get it's handle for future reference
	hwnd = CreateWindow(szAppName,		//window class name
		TEXT("Fotoshop"), // window caption
		WS_OVERLAPPEDWINDOW,	//window style
		CW_USEDEFAULT,		//initial x position
		CW_USEDEFAULT,		//initial y position
		CW_USEDEFAULT,		//initial x size
		CW_USEDEFAULT,		//initial y size
		NULL,				//parent window handle
		NULL,				//window menu handle
		hInstance,			//program instance handle
		NULL);				//creation parameters
	ShowWindow(hwnd, iCmdShow);//set window to be shown
	UpdateWindow(hwnd);//force an update so window is drawn

	//messgae loop
	while (GetMessage(&msg, NULL, 0, 0)){//get message from queue
		TranslateMessage(&msg);//for keystroke translation
		DispatchMessage(&msg);//pass msg back to windows for processing
		//note that this is to put windows o/s in control, rather than this app
	}

	return msg.wParam;
}
void nonAsMbrighten(BITMAP* bitmap, INT brighten, BYTE* temppBits){
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsperPixel = bitmap->bmBitsPixel;
	BYTE value = 0;//temp location of brighten value
	
	for (int i = 0; i < height*3; i++){
		for (int j = 0; j < width; j++){
				value = temppBits[i*width + j] ;
				value = max(0,min(255, value + brighten));//deal with rollover via saturation
				pBits[i*width + j] = value;
		}
	}
}
/**
mmx registers: mm0-mm7 - general purpose
pxor - packed xor between two registers
movd - moves a double word (32 bits) between memory and reg or reg to reg
por - packed "or" between two registers
psllq - packed shift left quad (4 DWORDS) 
movq - move quad (4 DWORDS or 64 bits) between memory and reg or reg to reg
paddusb - adds unsigned bytes (8 bytes) with saturation between two reg

*/
void mmx_brighten(BITMAP* bitmap, INT brighten, BYTE* buffer){
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;

	__asm {

		// save all registers you will be using onto stack
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		PUSH EDI

		// calculate bytes per pixel
		MOV EAX, bitsPerPixel
		SHR EAX, 3
		MOV ECX, EAX

		// calculate the number of pixels
		MOV EAX, width
		MUL height

		// calculate the number of bytes in image (pixels*bytesperpixel)
		MUL ECX

		// store the address of the buffer into a register (e.g. ebx)
		MOV EBX, buffer

		//setup counter register
		MOV EDX, ECX
		MOV ECX, EAX
		MOV EAX, EDX
		MOV EDX, brighten
		MOV EDI, 0
		CMP EDX, 0
		JGE SETUP
		NEG EDX
		MOV EDI, 1
		SETUP:
		MOV brighten, EDX
		movd mm2, brighten
		pxor mm1, mm1
		por mm1, mm2
		psllq mm1, 8
		por mm1, mm2
		psllq mm1, 8
		por mm1, mm2

			psllq mm1, 8
			por mm1, mm2
			psllq mm1, 8
			por mm1, mm2
			psllq mm1, 8
			por mm1, mm2
			psllq mm1, 8
			por mm1, mm2
			psllq mm1, 8
			por mm1, mm2
		CMP EDI, 0
		JG SUBITERATION

		ADDITERATION :
			movq mm0, [EBX]
			paddusb mm0, mm1
			movq [EBX], mm0

			//increment loop counter by 4
			ADD EBX, 8
			SUB ECX, 8

			//loop back up
			CMP ECX, 0
			JG ADDITERATION
		JMP END
		SUBITERATION :
			movq mm0, [EBX]
			psubusb mm0, mm1
			movq[EBX], mm0

			//increment loop counter by 4
			ADD EBX, 8
			SUB ECX, 8

			//loop back up
			CMP ECX, 0
			JG SUBITERATION

		END:
		POP EDI
		POP EDX
		POP ECX
		POP EBX
		POP EAX
		emms
	}
}

void assembly_brighten(BITMAP* bitmap, INT rmod, INT gmod, INT bmod, BYTE* buffer){
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;

	//REGISTERS

	//EAX, EBX, ECX, EDX are general purpose registers
	//ESI, EDI, EBP are also available as general purpose registers
	//AX, BX, CX, DX are the lower 16-bit of the above registers (think E as extended)
	//AH, AL, BH, BL, CH, CL, DH, DL are 8-bit high/low registers of the above (AX, BX, etc)
	//Typical use:
	//EAX accumulator for operands and results
	//EBX base pointer to data in the data segment
	//ECX counter for loops
	//EDX data pointer and I/O pointer
	//EBP frame pointer for stack frames
	//ESP stack pointer hardcoded into PUSH/POP operations
	//ESI source index for array operations
	//EDI destination index for array operations [e.g. copying arrays]
	//EIP instruction pointer
	//EFLAGS results flag hardcoded into conditional operations

	//SOME INSTRUCTIONS

	//MOV <source>, <destination>: mov reg, reg; mov reg, immediate; mov reg, memory; mov mem, reg; mov mem, imm
	//INC and DEC on registers or memory
	//ADD destination, source
	//SUB destination, source
	//CMP destination, source : sets the appropriate flag after performing (destination) - (source)
	//JMP label - jumps unconditionally ie. always to location marked by "label"
	//JE - jump if equal, JG/JL - jump if greater/less, JGE/JLE if greater or equal/less or equal, JNE - not equal, JZ - zero flag set
	//LOOP target: uses ECX to decrement and jump while ECX>0
	//logical instructions: AND, OR, XOR, NOT - performs bitwise logical operations. Note TEST is non-destructive AND instruction
	//SHL destination, count : shift left, SHR destination, count :shift right - carry flag (CF) and zero (Z) bits used, CL register often used if shift known
	//ROL - rotate left, ROR rotate right, RCL (rotate thru carry left), RCR (rotate thru carry right)
	//EQU - used to elimate hardcoding to create constants
	//MUL destination, source : multiplication
	//PUSH <source> - pushes source onto the stack
	//POP <destination> - pops off the stack into destination
	__asm
	{
	// save all registers you will be using onto stack
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		
	// calculate bytes per pixel
		MOV EAX, bitsPerPixel
		SHR EAX, 3
		MOV ECX, EAX

	// calculate the number of pixels
		MOV EAX, width
		MUL height

	// calculate the number of bytes in image (pixels*bytesperpixel)
		MUL ECX

	// store the address of the buffer into a register (e.g. ebx)
		MOV EBX, buffer

	//setup counter register
		MOV EDX, ECX
		MOV ECX, EAX
		MOV EAX, EDX
		
	//create a loop
	//loop while still have pixels to brighten
	//jump out of loop if done
		ITERATION:
			PUSH EAX
			PUSH EBX
			PUSH ECX
			PUSH EDX
	//load a pixel into a register
			MOV EAX, [EBX]
	//need to work with each colour plane: A R G B
	//load same pixel then into 3 more registers
			MOV EBX, EAX
			MOV ECX, EAX
			MOV EDX, EAX
	//shift bits down for each channel
			SHR EAX, 0
			SHR EBX, 8
			SHR ECX, 16
			SHR EDX, 24
	//clear out other bits
			AND EAX, 0xFF
			AND EBX, 0xFF
			AND ECX, 0xFF
			AND EDX, 0xFF
			

	//add brighten value to each pixel
	//check each pixel to see if saturated
	//if greater than 255, set to 255
			// working with the blue stuff... :v
			CMP bmod, 0
			JL BLUE_SUB
			ADD EAX, bmod
			CMP EAX, 0xFF
			JL BLUE_READY
			MOV EAX, 0xFF
			JMP BLUE_READY
			BLUE_SUB:
			NEG EAX
			SUB EAX, bmod
			NEG EAX
			CMP EAX, 0
			JGE BLUE_READY
			MOV EAX, 0x00
			BLUE_READY:

			// now working with the green one
			CMP gmod, 0
			JL GREEN_SUB
			ADD EBX, gmod
			CMP EBX, 0xFF
			JL GREEN_READY
			MOV EBX, 0xFF
			JMP GREEN_READY
			GREEN_SUB :
			NEG EBX
			SUB EBX, gmod
			NEG EBX
			CMP EBX, 0
			JGE GREEN_READY
			MOV EBX, 0x00
			GREEN_READY:

			// finally the red! o/
			CMP rmod, 0
			JL RED_SUB
			ADD ECX, rmod
			CMP ECX, 0xFF
			JL RED_READY
			MOV ECX, 0xFF
			JMP RED_READY
			RED_SUB :
			NEG ECX
			SUB ECX, rmod
			NEG ECX
			CMP ECX, 0
			JGE RED_READY
			MOV ECX, 0x00
			RED_READY:


	//put pixel back together again
			SHL EAX, 0
			SHL EBX, 8
			SHL ECX, 16
			SHL EDX, 24
			OR EAX, EBX
			OR EAX, ECX
			OR EAX, EDX
			
	//shift each channel amount needed
	//add each channel
	//store back into buffer
			POP EDX
			POP ECX
			POP EBX
			MOV [EBX], EAX
			POP EAX
	
	//increment loop counter by 4
			ADD EBX, EAX
			SUB ECX, EAX
	
	//loop back up
			CMP ECX, 0
			JG ITERATION
	//restore registers to original values before leaving
		POP EDX
		POP ECX
		POP EBX
		POP EAX
	//function
	}
}

void assembly_weird_saturation_questionmark(BITMAP* bitmap, INT satmod, BYTE* buffer) {
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;
	INT rmod, gmod, bmod;
	__asm
	{
		// save all registers you will be using onto stack
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		PUSH EDI
		PUSH ESI

		// calculate bytes per pixel
		MOV EAX, bitsPerPixel
		SHR EAX, 3
		MOV ECX, EAX

		// calculate the number of pixels
		MOV EAX, width
		MUL height

		// calculate the number of bytes in image (pixels*bytesperpixel)
		MUL ECX

		// store the address of the buffer into a register (e.g. ebx)
		MOV EBX, buffer

		//setup counter register
		MOV EDX, ECX
		MOV ECX, EAX
		MOV EAX, EDX

		//create a loop
		//loop while still have pixels to brighten
		//jump out of loop if done
		ITERATION :
			PUSH EAX
			PUSH ECX
			PUSH EBX
			//load a pixel into a register
			MOV EAX, [EBX]
			//need to work with each colour plane: A R G B
			//load same pixel then into 3 more registers
			MOV EBX, EAX
			MOV ECX, EAX
			MOV EDI, EAX
			//shift bits down for each channel
			SHR EBX, 0
			SHR ECX, 8
			SHR EDI, 16
			//clear out other bits
			AND EBX, 0xFF
			AND ECX, 0xFF
			AND EDI, 0xFF


			//add brighten value to each pixel
			//check each pixel to see if saturated
			//if greater than 255, set to 255
			MOV EAX, 0
			MOV EDX, 0
			ADD EAX, EBX
			ADD EAX, ECX
			ADD EAX, EDI
			MOV ESI, 3
			DIV ESI

			// working with the blue stuff... :v
			CHECK_BLUE :
			CMP EBX, EAX
			JL BLUE_DEC
			JG BLUE_INC
			MOV bmod, 0
			JMP CHECK_GREEN
			BLUE_INC:
			MOV EDX, satmod
			MOV bmod, EDX
			JMP CHECK_GREEN
			BLUE_DEC :
			MOV EDX, satmod
			NEG EDX
			MOV bmod, EDX

			// working with the green stuff... :v
			CHECK_GREEN :
			CMP ECX, EAX
			JL GREEN_DEC
			JG GREEN_INC
			MOV gmod, 0
			JMP CHECK_RED
			GREEN_INC :
			MOV EDX, satmod
			MOV gmod, EDX
			JMP CHECK_RED
			GREEN_DEC :
			MOV EDX, satmod
			NEG EDX
			MOV gmod, EDX

			// working with the red stuff... :v
			CHECK_RED :
			CMP EDI, EAX
			JL RED_DEC
			JG RED_INC
			MOV rmod, 0
			JMP CHANGE_COLORS
			RED_INC :
			MOV EDX, satmod
			MOV rmod, EDX
			JMP CHANGE_COLORS
			RED_DEC :
			MOV EDX, satmod
			NEG EDX
			MOV rmod, EDX

			CHANGE_COLORS:
			POP EBX
			PUSH EBX

			//load a pixel into a register
			MOV EAX, [EBX]
			//need to work with each colour plane: A R G B
			//load same pixel then into 3 more registers
			MOV EBX, EAX
			MOV ECX, EAX
			MOV EDX, EAX
			//shift bits down for each channel
			SHR EAX, 0
			SHR EBX, 8
			SHR ECX, 16
			SHR EDX, 24
			//clear out other bits
			AND EAX, 0xFF
			AND EBX, 0xFF
			AND ECX, 0xFF
			AND EDX, 0xFF


			//add brighten value to each pixel
			//check each pixel to see if saturated
			//if greater than 255, set to 255
					// working with the blue stuff... :v
			CMP bmod, 0
			JL BLUE_SUB
			ADD EAX, bmod
			CMP EAX, 0xFF
			JL BLUE_READY
			MOV EAX, 0xFF
			JMP BLUE_READY
			BLUE_SUB :
			NEG EAX
			SUB EAX, bmod
			NEG EAX
			CMP EAX, 0
			JGE BLUE_READY
			MOV EAX, 0x00
			BLUE_READY:

			// now working with the green one
			CMP gmod, 0
			JL GREEN_SUB
			ADD EBX, gmod
			CMP EBX, 0xFF
			JL GREEN_READY
			MOV EBX, 0xFF
			JMP GREEN_READY
			GREEN_SUB :
			NEG EBX
			SUB EBX, gmod
			NEG EBX
			CMP EBX, 0
			JGE GREEN_READY
			MOV EBX, 0x00
			GREEN_READY:

			// finally the red! o/
			CMP rmod, 0
			JL RED_SUB
			ADD ECX, rmod
			CMP ECX, 0xFF
			JL RED_READY
			MOV ECX, 0xFF
			JMP RED_READY
			RED_SUB :
			NEG ECX
			SUB ECX, rmod
			NEG ECX
			CMP ECX, 0
			JGE RED_READY
			MOV ECX, 0x00
			RED_READY:


			//put pixel back together again
			SHL EAX, 0
			SHL EBX, 8
			SHL ECX, 16
			SHL EDX, 24
			OR EAX, EBX
			OR EAX, ECX
			OR EAX, EDX
			//shift each channel amount needed
			//add each channel
			//store back into buffer
			POP EBX
			POP ECX
			MOV[EBX], EAX
			POP EAX

			//increment loop counter by 4
			ADD EBX, EAX
			SUB ECX, EAX

			//loop back up
			CMP ECX, 0
			JG ITERATION
			//restore registers to original values before leaving
			POP ESI
			POP EDI
			POP EDX
			POP ECX
			POP EBX
			POP EAX
			//function
	}
}

void assembly_invertcolors(BITMAP* bitmap, BYTE* buffer) {
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;

	__asm
	{
		// save all registers you will be using onto stack
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		PUSH EDI

		// calculate bytes per pixel
		MOV EAX, bitsPerPixel
		SHR EAX, 3
		MOV ECX, EAX

		// calculate the number of pixels
		MOV EAX, width
		MUL height

		// calculate the number of bytes in image (pixels*bytesperpixel)
		MUL ECX

		// store the address of the buffer into a register (e.g. ebx)
		MOV EBX, buffer

		//setup counter register
		MOV EDX, ECX
		MOV ECX, EAX
		MOV EAX, EDX

		//create a loop
		//loop while still have pixels to brighten
		//jump out of loop if done
		ITERATION :
		PUSH EAX
			PUSH EBX
			PUSH ECX
			PUSH EDX
			//load a pixel into a register
			MOV EAX, [EBX]
			//need to work with each colour plane: A R G B
			//load same pixel then into 3 more registers
			MOV EBX, EAX
			MOV ECX, EAX
			MOV EDX, EAX
			//shift bits down for each channel
			SHR EAX, 0
			SHR EBX, 8
			SHR ECX, 16
			SHR EDX, 24
			//clear out other bits
			AND EAX, 0xFF
			AND EBX, 0xFF
			AND ECX, 0xFF
			AND EDX, 0xFF


			MOV EDI, 0xFF
			SUB EDI, EAX
			MOV EAX, EDI
			MOV EDI, 0xFF
			SUB EDI, EBX
			MOV EBX, EDI
			MOV EDI, 0xFF
			SUB EDI, ECX
			MOV ECX, EDI

			//put pixel back together again
			SHL EAX, 0
			SHL EBX, 8
			SHL ECX, 16
			SHL EDX, 24
			OR EAX, EBX
			OR EAX, ECX
			OR EAX, EDX

			//shift each channel amount needed
			//add each channel
			//store back into buffer
			POP EDX
			POP ECX
			POP EBX
			MOV[EBX], EAX
			POP EAX

			//increment loop counter by 4
			ADD EBX, EAX
			SUB ECX, EAX

			//loop back up
			CMP ECX, 0
			JG ITERATION
			//restore registers to original values before leaving
			POP EDI
			POP EDX
			POP ECX
			POP EBX
			POP EAX
			//function
	}
}

void assembly_changeHUE(BITMAP* bitmap, BYTE* buffer) {
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;

	__asm
	{
		// save all registers you will be using onto stack
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		PUSH EDI

		// calculate bytes per pixel
		MOV EAX, bitsPerPixel
		SHR EAX, 3
		MOV ECX, EAX

		// calculate the number of pixels
		MOV EAX, width
		MUL height

		// calculate the number of bytes in image (pixels*bytesperpixel)
		MUL ECX

		// store the address of the buffer into a register (e.g. ebx)
		MOV EBX, buffer

		//setup counter register
		MOV EDX, ECX
		MOV ECX, EAX
		MOV EAX, EDX
		
		ADD EBX, 2

		//create a loop
		//loop while still have pixels to brighten
		//jump out of loop if done
		ITERATION :
		PUSH EAX
			PUSH EBX
			PUSH ECX
			PUSH EDX
			//load a pixel into a register
			MOV EAX, [EBX]
			//need to work with each colour plane: A R G B
			//load same pixel then into 3 more registers
			MOV EBX, EAX
			MOV ECX, EAX
			MOV EDX, EAX
			//shift bits down for each channel
			SHR EAX, 0
			SHR EBX, 8
			SHR ECX, 16
			SHR EDX, 24
			//clear out other bits
			AND EAX, 0xFF
			AND EBX, 0xFF
			AND ECX, 0xFF
			AND EDX, 0xFF

			//put pixel back together again
			SHL EAX, 0
			SHL EBX, 24
			SHL ECX, 8
			SHL EDX, 16
			OR EAX, EBX
			OR EAX, ECX
			OR EAX, EDX

			//shift each channel amount needed
			//add each channel
			//store back into buffer
			POP EDX
			POP ECX
			POP EBX
			MOV [EBX], EAX
			POP EAX

			//increment loop counter by 4
			ADD EBX, EAX
			SUB ECX, EAX

			//loop back up
			CMP ECX, 0
			JG ITERATION
			//restore registers to original values before leaving
			POP EDI
			POP EDX
			POP ECX
			POP EBX
			POP EAX
			//function
	}
}

void assembly_flipimg(BITMAP* bitmap, BYTE* buffer) {
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;

	__asm
	{
		// save all registers you will be using onto stack
		PUSH ESI
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		PUSH EDI

		// calculate bytes per pixel
		MOV EAX, bitsPerPixel
		SHR EAX, 3
		MOV ECX, EAX

		// calculate the number of pixels
		MOV EAX, width
		MUL height

		// calculate the number of bytes in image (pixels*bytesperpixel)
		MUL ECX
		MOV ESI, EAX
		SHR EAX, 1

		// store the address of the buffer into a register (e.g. ebx)
		MOV EBX, buffer
		ADD ESI, EBX

		//setup counter register
		MOV EDX, ECX
		MOV ECX, EAX
		MOV EAX, EDX
		MOV EAX, EDX
		SUB ESI, EAX
		//By now I'm completely lost on how I'm using the registers. It's an insane mess!

		//create a loop
		//loop while still have pixels to brighten
		//jump out of loop if done
		ITERATION :
			PUSH EAX
			PUSH EBX
			PUSH ECX
			PUSH EDX
			//load a pixel into a register
			MOV EAX, [EBX]
			MOV EDI, [ESI]
			MOV EBX, EAX
			MOV EDX, EDI
			AND EAX, 0xFF000000
			AND EBX, 0x00FFFFFF
			AND EDI, 0xFF000000
			AND EDX, 0x00FFFFFF
			OR EAX, EDX
			OR EDI, EBX


			//shift each channel amount needed
			//add each channel
			//store back into buffer
			POP EDX
			POP ECX
			POP EBX
			MOV [EBX], EAX
			POP EAX
			MOV [ESI], EDI

			//increment loop counter by 4
			ADD EBX, EAX
			SUB ESI, EAX
			SUB ECX, EAX


			//loop back up
			CMP ECX, 0
			JG ITERATION
			//restore registers to original values before leaving
			POP EDI
			POP EDX
			POP ECX
			POP EBX
			POP EAX
			POP ESI
			//function
	}
}
void assembly_not_quite_greyscale_but_almost(BITMAP* bitmap, BYTE* buffer) {
	INT width = bitmap->bmWidth;
	INT height = bitmap->bmHeight;
	INT bitsPerPixel = bitmap->bmBitsPixel;
	INT rmod, gmod, bmod;
	__asm
	{
		// save all registers you will be using onto stack
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		PUSH EDI
		PUSH ESI

		// calculate bytes per pixel
		MOV EAX, bitsPerPixel
		SHR EAX, 3
		MOV ECX, EAX

		// calculate the number of pixels
		MOV EAX, width
		MUL height

		// calculate the number of bytes in image (pixels*bytesperpixel)
		MUL ECX

		// store the address of the buffer into a register (e.g. ebx)
		MOV EBX, buffer

		//setup counter register
		MOV EDX, ECX
		MOV ECX, EAX
		MOV EAX, EDX

		//create a loop
		//loop while still have pixels to brighten
		//jump out of loop if done
		ITERATION :
		PUSH EAX
			PUSH ECX
			PUSH EBX
			//load a pixel into a register
			MOV EAX, [EBX]
			//need to work with each colour plane: A R G B
			//load same pixel then into 3 more registers
			MOV ECX, EAX
			MOV EDI, EAX
			MOV ESI, EAX
			//shift bits down for each channel
			SHR ECX, 0
			SHR EDI, 8
			SHR ESI, 16
			//clear out other bits
			AND ECX, 0xFF
			AND EDI, 0xFF
			AND ESI, 0xFF


			//add brighten value to each pixel
			//check each pixel to see if saturated
			//if greater than 255, set to 255
			MOV EAX, 0
			MOV EDX, 0
			ADD EAX, ECX
			ADD EAX, EDI
			ADD EAX, ESI
			

			MOV EAX, ECX
			MOV EDX, 2
			MUL EDX
			MOV ECX, EAX

			MOV EAX, EDI
			MOV EDX, 9
			MUL EDX
			MOV EDI, EAX

			MOV EAX, ESI
			MOV EDX, 5
			MUL EDX
			MOV ESI, EAX

			MOV EAX, 0
			ADD EAX, ECX
			ADD EAX, EDI
			ADD EAX, ESI
			SHR EAX, 4


			MOV ESI, EAX
			MOV EAX, [EBX]
			AND EAX, 0xFF000000
			OR EAX, ESI
			SHL ESI, 8
			OR EAX, ESI
			SHL ESI, 8
			OR EAX, ESI

			//shift each channel amount needed
			//add each channel
			//store back into buffer
			POP EBX
			POP ECX
			MOV[EBX], EAX
			POP EAX

			//increment loop counter by 4
			ADD EBX, EAX
			SUB ECX, EAX

			//loop back up
			CMP ECX, 0
			JG ITERATION
			//restore registers to original values before leaving
			POP ESI
			POP EDI
			POP EDX
			POP ECX
			POP EBX
			POP EAX
			//function
	}
}



#define COLOR_STEP 10
#define IMG_Y 200

#define NONASSEMBLYBRIGHTBTN  (HMENU) 1000
#define NONASSEMBLYDARKBTN  (HMENU) 1001
#define ASSEMBLYBRIGHTRBTN (HMENU) 1002
#define ASSEMBLYDARKRBTN (HMENU) 1101
#define ASSEMBLYBRIGHTGBTN (HMENU) 1102
#define ASSEMBLYDARKGBTN (HMENU) 1103
#define ASSEMBLYBRIGHTBBTN (HMENU) 1104
#define ASSEMBLYDARKBBTN (HMENU) 1105
#define ASSEMBLYSATBTN (HMENU) 1106
#define ASSEMBLYINVCOLBTN (HMENU) 1107
#define ASSEMBLYFLIPBTN (HMENU) 1108
#define ASSEMBLYCHANGEHUEBTN (HMENU) 1109
#define ASSEMBLYGREYSCALEBTN (HMENU) 1110
#define MMXBRIGHTBTN (HMENU) 1201
#define MMXDARKBTN (HMENU) 1202

#define RESET_X curX = 0
#define RESET_Y curY = 0
#define Y_STEP (curY += stepY) - stepY
#define X_STEP (curX += stepX) - stepX
#define X_MINISTEP (curX += stepX/5) - stepX/5

/**
Purpose: To handle windows messages for specific cases including when
the window is first created, refreshing (painting), and closing
the window.

Returns: Long - any error message (see Win32 API for details of possible error messages)
Notes:	 CALLBACK is defined as __stdcall which defines a calling
convention for assembly (stack parameter passing)
**/
LRESULT CALLBACK HelloWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
	HWND btnHwnd;
	HDC		hdc;
	PAINTSTRUCT ps;
	RECT	rect;
	BITMAP* bitmap;
	HDC hdcMem;
	BOOL bSuccess;
	DWORD dwBytesRead, dwInfoSize;
	HANDLE hFile;
	int error = 0;
	BYTE *temppBits;
	bool copyTemp = true;
	INT stepX = 100;
	INT stepY = 25;
	INT curX = 0;
	INT curY = 0;
	RESET_X;
	RESET_Y;
	


	switch (message){
	case WM_CREATE://additional things to do when window is created
		CreateWindowEx(NULL, TEXT("STATIC"), TEXT("non-assembly"), WS_CHILD | WS_VISIBLE | SS_CENTER, curX, Y_STEP, stepX, stepY, hwnd, HMENU(NULL), GetModuleHandle(NULL), NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("brighten"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, NONASSEMBLYBRIGHTBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("darken"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, NONASSEMBLYDARKBTN, ghInstance, NULL);

		RESET_Y;
		X_STEP;
		X_MINISTEP;
		CreateWindowEx(NULL, TEXT("STATIC"), TEXT("assembly"), WS_CHILD | WS_VISIBLE | SS_CENTER, curX, Y_STEP, 3*stepX, stepY, hwnd, HMENU(NULL), GetModuleHandle(NULL), NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("brighten red"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYBRIGHTRBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("darken red"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYDARKRBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("Saturation(?)"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYSATBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("Rotate HUE"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYCHANGEHUEBTN, ghInstance, NULL);
		curY = stepY;
		X_STEP;
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("brighten green"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYBRIGHTGBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("darken green"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYDARKGBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("Invert Colors"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYINVCOLBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("Greyscale(?)"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYGREYSCALEBTN, ghInstance, NULL);
		curY = stepY;
		X_STEP;
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("brighten blue"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYBRIGHTBBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("darken blue"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYDARKBBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("Flip Image"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, ASSEMBLYFLIPBTN, ghInstance, NULL);
		
		RESET_Y;
		X_STEP;
		X_MINISTEP;
		CreateWindowEx(NULL, TEXT("STATIC"), TEXT("mmx"), WS_CHILD | WS_VISIBLE | SS_CENTER, curX, Y_STEP, stepX, stepY, hwnd, HMENU(NULL), GetModuleHandle(NULL), NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("brighten"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, MMXBRIGHTBTN, ghInstance, NULL);
		btnHwnd = CreateWindowEx(NULL, TEXT("BUTTON"), TEXT("darken"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			curX, Y_STEP, stepX, stepY, hwnd, MMXDARKBTN, ghInstance, NULL);


		curY = 5 * stepY;
		curX = 0;
		CreateWindowEx(NULL, TEXT("STATIC"), TEXT("Last operation time (ns): "), WS_CHILD | WS_VISIBLE | SS_CENTER, curX, curY, 3 * stepX, stepY, hwnd, HMENU(NULL), GetModuleHandle(NULL), NULL);
		curX = 3 * stepX;
		timeDisplayer = CreateWindowEx(NULL, TEXT("STATIC"), TEXT(""), WS_CHILD | WS_VISIBLE | SS_CENTER, curX, Y_STEP, stepX, stepY, hwnd, HMENU(NULL), GetModuleHandle(NULL), NULL);

		hFile = CreateFile(TEXT("test.bmp"), GENERIC_READ, FILE_SHARE_READ,
                         NULL, OPEN_EXISTING, 0, NULL) ;

		if (hFile == INVALID_HANDLE_VALUE){
			error = GetLastError();
			return 0;
		}
         
		error = sizeof(BITMAPFILEHEADER);

		bSuccess = ReadFile(hFile, &bmfh, sizeof (BITMAPFILEHEADER),
			&dwBytesRead, NULL);

		if (!bSuccess || (dwBytesRead != sizeof (BITMAPFILEHEADER))
			|| (bmfh.bfType != *(WORD *) "BM"))
		{
//			CloseHandle(hFile);
			return NULL;
		}
		dwInfoSize = bmfh.bfOffBits - sizeof (BITMAPFILEHEADER);

		pbmi = (BITMAPINFO*)malloc(dwInfoSize);

		bSuccess = ReadFile(hFile, pbmi, dwInfoSize, &dwBytesRead, NULL);

		if (!bSuccess || (dwBytesRead != dwInfoSize))
		{
			free(pbmi);
			CloseHandle(hFile);
			return NULL;
		}
		hBitmap = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, (VOID**)&pBits, NULL, 0);
		ReadFile(hFile, pBits, bmfh.bfSize - bmfh.bfOffBits, &dwBytesRead, NULL);



		GetObject(hBitmap, sizeof(BITMAP), &Bitmap);
		return 0;
	case WM_LBUTTONDOWN:
		return 0;
	case WM_RBUTTONDOWN:
		return 0;
	case WM_COMMAND:
		hdc = GetDC(hwnd);
		hdcMem = CreateCompatibleDC(hdc);
		GetObject(hBitmap, sizeof(BITMAP), &Bitmap);
		temppBits = (BYTE*)malloc(Bitmap.bmWidth*Bitmap.bmHeight * 3);
		memcpy(temppBits, pBits, Bitmap.bmWidth*Bitmap.bmHeight * 3);
		hBitmap = CreateDIBSection(NULL, pbmi, DIB_RGB_COLORS, (VOID**)&pBits, NULL, 0);
		QueryPerformanceCounter(&startTime);
		switch (LOWORD(wParam)) {
		case (int)NONASSEMBLYBRIGHTBTN:
			copyTemp = false;
			nonAsMbrighten(&Bitmap, COLOR_STEP, temppBits);
			break;
		case (int)NONASSEMBLYDARKBTN:
			copyTemp = false;
			nonAsMbrighten(&Bitmap, -COLOR_STEP, temppBits);
			break;
		case (int)ASSEMBLYBRIGHTRBTN:
			assembly_brighten(&Bitmap, COLOR_STEP, 0, 0, temppBits);
			break;
		case (int)ASSEMBLYDARKRBTN:
			assembly_brighten(&Bitmap, -COLOR_STEP, 0, 0, temppBits);
			break;
		case (int)ASSEMBLYBRIGHTGBTN:
			assembly_brighten(&Bitmap, 0, COLOR_STEP, 0, temppBits);
			break;
		case (int)ASSEMBLYDARKGBTN:
			assembly_brighten(&Bitmap, 0, -COLOR_STEP, 0, temppBits);
			break;
		case (int)ASSEMBLYBRIGHTBBTN:
			assembly_brighten(&Bitmap, 0, 0, COLOR_STEP, temppBits);
			break;
		case (int)ASSEMBLYDARKBBTN:
			assembly_brighten(&Bitmap, 0, 0, -COLOR_STEP, temppBits);
			break;
		case (int)ASSEMBLYSATBTN:
			assembly_weird_saturation_questionmark(&Bitmap, COLOR_STEP, temppBits);
			break;
		case (int)ASSEMBLYINVCOLBTN:
			assembly_invertcolors(&Bitmap, temppBits);
			break;
		case (int)ASSEMBLYCHANGEHUEBTN:
			assembly_changeHUE(&Bitmap, temppBits);
			break;
		case (int)ASSEMBLYFLIPBTN:
			assembly_flipimg(&Bitmap, temppBits);
			break;
		case (int)ASSEMBLYGREYSCALEBTN:
			assembly_not_quite_greyscale_but_almost(&Bitmap, temppBits);
			break;
		case (int)MMXBRIGHTBTN:
			mmx_brighten(&Bitmap, COLOR_STEP, temppBits);
			break;
		case (int)MMXDARKBTN:
			mmx_brighten(&Bitmap, -COLOR_STEP, temppBits);
			break;
		}
		QueryPerformanceCounter(&endTime);
		if(copyTemp)
			memcpy(pBits, temppBits, Bitmap.bmWidth*Bitmap.bmHeight * 3);

		SelectObject(hdcMem, hBitmap);
		BitBlt(hdc, 0, IMG_Y, Bitmap.bmWidth, Bitmap.bmHeight,
			hdcMem, 0, 0, SRCCOPY);

		time = endTime.QuadPart - startTime.QuadPart;
		timeString = std::to_string(time);
		
		SetWindowText(timeDisplayer, timeString.c_str());
		return 0;
	case WM_PAINT://what to do when a paint msg occurs
		hdc = BeginPaint(hwnd, &ps);//get a handle to a device context for drawing
		GetClientRect(hwnd, &rect);//define drawing area for clipping
		//GetObject(hBitmap, sizeof(BITMAP), &Bitmap);
		hdcMem = CreateCompatibleDC(hdc);
		SelectObject(hdcMem, hBitmap);

		BitBlt(hdc, 0, IMG_Y, Bitmap.bmWidth, Bitmap.bmHeight,
			hdcMem, 0, 0, SRCCOPY);


		EndPaint(hwnd, &ps);//release the device context
		return 0;

	case WM_DESTROY://how to handle a destroy (close window app) msg
		PostQuitMessage(0);
		return 0;
	}
	//return the message to windows for further processing
	return DefWindowProc(hwnd, message, wParam, lParam);
}
