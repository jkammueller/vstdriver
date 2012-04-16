/* Copyright (C) 2011 Chris Moeller, Brad Miller
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __VSTDRIVER_H__
#define __VSTDRIVER_H__

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "inc/aeffect.h"
#include "inc/aeffectx.h"

class VSTDriver {
private:
	TCHAR      * szPluginPath;

	HMODULE      hDll;
	AEffect    * pEffect;

	unsigned char * blState;
	unsigned buffer_size;

	unsigned uNumOutputs;

	struct myVstEvent
	{
		struct myVstEvent * next;
		union
		{
			VstMidiEvent midiEvent;
			VstMidiSysexEvent sysexEvent;
		} ev;
	} * evChain, * evTail;

	float     ** float_list_in;
	float     ** float_list_out;
	float      * float_null;
	float      * float_out;

	void FreeChain();

	void load_settings();
public:
	VSTDriver();
	~VSTDriver();
	void CloseVSTDriver();
	BOOL OpenVSTDriver();
	void ProcessMIDIMessage(DWORD dwParam1);
	void ProcessSysEx(unsigned char *sysexbuffer,int exlen);
	void Render(short * samples, int len);
};

#endif