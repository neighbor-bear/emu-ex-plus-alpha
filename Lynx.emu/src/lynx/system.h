//
// Copyright (c) 2004 K. Wilkins
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//

//////////////////////////////////////////////////////////////////////////////
//                       Handy - An Atari Lynx Emulator                     //
//                          Copyright (c) 1996,1997                         //
//                                 K. Wilkins                               //
//////////////////////////////////////////////////////////////////////////////
// System object header file                                                //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This header file provides the interface definition and inline code for   //
// the system object, this object if what binds together all of the Handy   //
// hardware enmulation objects, its the glue that holds the system together //
//                                                                          //
//    K. Wilkins                                                            //
// August 1997                                                              //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
// Revision History:                                                        //
// -----------------                                                        //
//                                                                          //
// 01Aug1997 KW Document header added & class documented.                   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#ifndef SYSTEM_H
#define SYSTEM_H

#include "machine.h"

#define HANDY_SYSTEM_FREQ						16000000
#define HANDY_TIMER_FREQ						20

#define HANDY_FILETYPE_LNX		0
#define HANDY_FILETYPE_HOMEBREW	1
#define HANDY_FILETYPE_SNAPSHOT	2
#define HANDY_FILETYPE_ILLEGAL	3
#define HANDY_FILETYPE_RAW			4

#define HANDY_SCREEN_WIDTH	160
#define HANDY_SCREEN_HEIGHT	102
//
// Define the global variable list
//
MDFN_HIDE extern uint32	gSystemCycleCount;
MDFN_HIDE extern uint32	gSuzieDoneTime;
MDFN_HIDE extern uint32	gNextTimerEvent;
MDFN_HIDE extern uint32	gCPUBootAddress;
MDFN_HIDE extern uint32	gSystemIRQ;
MDFN_HIDE extern uint32	gSystemNMI;
MDFN_HIDE extern uint32	gSystemCPUSleep;
MDFN_HIDE extern uint32	gSystemHalt;
MDFN_HIDE extern bool LynxLineDrawn[256];

//
// Define the interfaces before we start pulling in the classes
// as many classes look for articles from the interfaces to
// allow compilation

#include "sysbase.h"

class CSystem;

//
// Now pull in the parts that build the system
//
#include "lynxbase.h"
#include "ram.h"
#include "rom.h"
#include "memmap.h"
#include "cart.h"
#include "susie.h"
#include "mikie.h"
#include "c65c02.h"

#define TOP_START	0xfc00
#define TOP_MASK	0x03ff
#define TOP_SIZE	0x400
#define SYSTEM_SIZE	65536

class CSystem : public CSystemBase
{
	public:
		CSystem(GameFile* gf) MDFN_COLD;
		~CSystem() MDFN_COLD;

	public:
    void HLE_BIOS_FE00(void);
    void HLE_BIOS_FE19(void);
    void HLE_BIOS_FE4A(void);
    void HLE_BIOS_FF80(void);
		void	Reset(void) MDFN_COLD;

		inline void Update(void)
		{
			// 
			// Only update if there is a predicted timer event
			//
			if(gSystemCycleCount>=gNextTimerEvent)
			{
				mMikie->Update();
			}
			//
			// Step the processor through 1 instruction
			//
			mCpu->Update();

			//
			// If the CPU is asleep then skip to the next timer event
			//			
			if(gSystemCPUSleep)
			{
				gSystemCycleCount=gNextTimerEvent;
			}
		}

		//
		// We MUST have separate CPU & RAM peek & poke handlers as all CPU accesses must
		// go thru the address generator at $FFF9
		//
		// BUT, Mikie video refresh & Susie see the whole system as RAM
		//
		// Complete and utter wankers, its taken me 1 week to find the 2 lines
		// in all the documentation that mention this fact, the mother of all
		// bugs has been found and FIXED.......

		//
		// CPU
		//
		inline void  Poke_CPU(uint32 addr, uint8 data) { mMemoryHandlers[addr]->Poke(addr,data);};
		inline uint8 Peek_CPU(uint32 addr) { return mMemoryHandlers[addr]->Peek(addr);};
		inline void  PokeW_CPU(uint32 addr,uint16 data) { mMemoryHandlers[addr]->Poke(addr,data&0xff);addr++;mMemoryHandlers[addr]->Poke(addr,data>>8);};
		inline uint16 PeekW_CPU(uint32 addr) {return ((mMemoryHandlers[addr]->Peek(addr))+(mMemoryHandlers[addr]->Peek(addr+1)<<8));};

// High level cart access for debug etc

		inline void  Poke_CART(uint32 addr, uint8 data) {mCart->Poke(addr,data);};
		inline uint8 Peek_CART(uint32 addr) {return mCart->Peek(addr);};
		inline void  CartBank(EMMODE bank) {mCart->BankSelect(bank);};
		inline uint32 CartSize(void) {return mCart->ObjectSize();};
		inline const char* CartGetName(void) { return mCart->CartGetName();};
		inline const char* CartGetManufacturer(void) { return mCart->CartGetManufacturer();};
		inline uint32 CartGetRotate(void) {return mCart->CartGetRotate();};

// Low level cart access for Suzy, Mikey

		inline void  Poke_CARTB0(uint8 data) {mCart->Poke0(data);};
		inline void  Poke_CARTB1(uint8 data) {mCart->Poke1(data);};
		inline uint8 Peek_CARTB0(void) {return mCart->Peek0();}
		inline uint8 Peek_CARTB1(void) {return mCart->Peek1();}
		inline void  CartAddressStrobe(bool strobe) {mCart->CartAddressStrobe(strobe);};
		inline void  CartAddressData(bool data) {mCart->CartAddressData(data);};

// Low level CPU access

		void	SetRegs(C6502_REGS &regs) {mCpu->SetRegs(regs);};
		void	GetRegs(C6502_REGS &regs) {mCpu->GetRegs(regs);};

// Mikey system interfacing

		void	DisplaySetAttributes(const MDFN_PixelFormat &format, const uint8* CustomPalette) { mMikie->DisplaySetAttributes(format, CustomPalette); };

		void	ComLynxCable(int status) { mMikie->ComLynxCable(status); };
		void	ComLynxRxData(int data)  { mMikie->ComLynxRxData(data); };
		void	ComLynxTxCallback(void (*function)(int data,uint32 objref),uint32 objref) { mMikie->ComLynxTxCallback(function,objref); };

// Suzy system interfacing

		uint32	PaintSprites(void) {return mSusie->PaintSprites();};

// Miscellaneous

		void	SetButtonData(uint32 data) {mSusie->SetButtonData(data);};
		uint32	GetButtonData(void) {return mSusie->GetButtonData();};
		void	SetCycleBreakpoint(uint32 breakpoint) {mCycleCountBreakpoint=breakpoint;};
		uint8*	GetRamPointer(void) {return mRam->GetRamPointer();};

	public:
		uint32			mCycleCountBreakpoint;
		CLynxBase		*mMemoryHandlers[SYSTEM_SIZE];
		std::unique_ptr<CCart>	mCart;
		std::unique_ptr<CRom>	mRom;
		std::unique_ptr<CMemMap> mMemMap;
		std::unique_ptr<CRam>	mRam;
		std::unique_ptr<C65C02>	mCpu;
		std::unique_ptr<CMikie>	mMikie;
		std::unique_ptr<CSusie>	mSusie;

		uint32			mFileType;
};

void lynx_decrypt(unsigned char * result,
                  const unsigned char * encrypted,
                  const int length);

#endif
