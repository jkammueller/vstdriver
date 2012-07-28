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

#include "VSTDriver.h"

#include "../../common/settings.h"

#define BUFFER_SIZE 4096

static VstIntPtr VSTCALLBACK audioMaster(AEffect *effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt)
{
	switch (opcode)
	{
	case audioMasterVersion:
		return 2400;

	case audioMasterCurrentId:
		if (effect) return effect->uniqueID;
		break;

	case audioMasterGetVendorString:
		strncpy((char *)ptr, "Chris Moeller and mudlord", 64);
		//strncpy((char *)ptr, "YAMAHA", 64);
		break;

	case audioMasterGetProductString:
		strncpy((char *)ptr, "VSTiDriver", 64);
		//strncpy((char *)ptr, "SOL/SQ01", 64);
		break;

	case audioMasterGetVendorVersion:
		return 1337; // uhuhuhuhu
		//return 0;

	case audioMasterGetLanguage:
		return kVstLangEnglish;
	}

	return 0;
}

VSTDriver::VSTDriver() {
	szPluginPath = NULL;
	hDll = NULL;
	pEffect = NULL;
	blState = NULL;
	buffer_size = 0;
	evChain = NULL;
	evTail = NULL;
}

VSTDriver::~VSTDriver() {
	CloseVSTDriver();
}

void VSTDriver::load_settings() {
	HKEY hKey;
	long lResult;
	DWORD dwType=REG_SZ;
	DWORD dwSize=0;
	if ( RegOpenKey(HKEY_CURRENT_USER, _T("Software\\VSTi Driver"),&hKey) == ERROR_SUCCESS ) {
		lResult = RegQueryValueEx(hKey, _T("plugin"), NULL, &dwType, NULL, &dwSize);
		if ( lResult == ERROR_SUCCESS && dwType == REG_SZ ) {
			szPluginPath = (TCHAR*) calloc( dwSize + sizeof(TCHAR), 1 );
			RegQueryValueEx(hKey, _T("plugin"), NULL, &dwType, (LPBYTE) szPluginPath, &dwSize);

			blChunk.resize( 0 );

			const TCHAR * dot = _tcsrchr( szPluginPath, _T('.') );
			if ( !dot ) dot = szPluginPath + _tcslen( szPluginPath );
			TCHAR * szSettingsPath = ( TCHAR * ) _alloca( ( dot - szPluginPath + 5 ) * sizeof( TCHAR ) );
			_tcsncpy( szSettingsPath, szPluginPath, dot - szPluginPath );
			_tcscpy( szSettingsPath + ( dot - szPluginPath ), _T(".set") );

			FILE * f;
			errno_t err = _tfopen_s( &f, szSettingsPath, _T("rb") );
			if ( !err )
			{
				fseek( f, 0, SEEK_END );
				size_t chunk_size = ftell( f );
				fseek( f, 0, SEEK_SET );
				blChunk.resize( chunk_size );
				fread( blChunk.data(), 1, chunk_size, f );
				fclose( f );
			}
		}
		RegCloseKey( hKey);
	}
}

void VSTDriver::CloseVSTDriver() {
	if ( pEffect ) {
		if ( buffer_size ) {
			pEffect->dispatcher( pEffect, effStopProcess, 0, 0, 0, 0 );

			free( blState );
			blState = NULL;
			buffer_size = 0;
		}

		pEffect->dispatcher( pEffect, effClose, 0, 0, 0, 0 );

		pEffect = NULL;
	}

	if ( hDll ) {
		FreeLibrary( hDll );

		hDll = NULL;
	}

	FreeChain();

	if ( szPluginPath ) {
		free( szPluginPath );

		szPluginPath = NULL;
	}
}

void VSTDriver::FreeChain() {
	myVstEvent * ev = evChain;
	while ( ev ) {
		myVstEvent * next = ev->next;
		if ( ev->ev.sysexEvent.type == kVstSysExType ) {
			free( ev->ev.sysexEvent.sysexDump );
		}
		free( ev );
		ev = next;
	}
	evChain = NULL;
	evTail = NULL;
}

typedef AEffect* (*PluginEntryProc) (audioMasterCallback audioMaster);

BOOL VSTDriver::OpenVSTDriver() {
	CloseVSTDriver();

	load_settings();

	hDll = LoadLibrary( szPluginPath );

	if ( hDll ) {
		PluginEntryProc pMain = (PluginEntryProc) GetProcAddress( hDll, "main" );

		if ( pMain ) {
			pEffect = (*pMain)(&audioMaster);

			if ( pEffect ) {
				uNumOutputs = min( pEffect->numOutputs, 2 );

				pEffect->dispatcher( pEffect, effOpen, 0, 0, 0, 0 );

				if ( pEffect->dispatcher( pEffect, effGetPlugCategory, 0, 0, 0, 0 ) == kPlugCategSynth &&
					pEffect->dispatcher( pEffect, effCanDo, 0, 0, "sendVstMidiEvent", 0 ) ) {
						setChunk( pEffect, blChunk );

						return TRUE;
				}
			}
		}
	}

	return FALSE;	
}

void VSTDriver::ProcessMIDIMessage(DWORD dwParam1) {
	myVstEvent * ev = ( myVstEvent * ) calloc( sizeof( myVstEvent ), 1 );
	if ( evTail ) evTail->next = ev;
	evTail = ev;
	if ( !evChain ) evChain = ev;
	ev->ev.midiEvent.type = kVstMidiType;
	ev->ev.midiEvent.byteSize = sizeof( ev->ev.midiEvent );
	memcpy( &ev->ev.midiEvent.midiData, &dwParam1, sizeof( dwParam1 ) );
}

void VSTDriver::ProcessSysEx(const unsigned char *sysexbuffer,int exlen) {
	myVstEvent * ev = ( myVstEvent * ) calloc( sizeof( myVstEvent ), 1 );
	if ( evTail ) evTail->next = ev;
	evTail = ev;
	if ( !evChain ) evChain = ev;
	ev->ev.sysexEvent.type = kVstSysExType;
	ev->ev.sysexEvent.byteSize = sizeof( ev->ev.sysexEvent );
	ev->ev.sysexEvent.dumpBytes = exlen;
	ev->ev.sysexEvent.sysexDump = ( char * ) malloc( exlen );
	memcpy( ev->ev.sysexEvent.sysexDump, sysexbuffer, exlen );
}

void VSTDriver::RenderFloat(float * samples, int len) {
	if ( !buffer_size ) {
		pEffect->dispatcher(pEffect, effSetSampleRate, 0, 0, 0, 44100);
		pEffect->dispatcher(pEffect, effSetBlockSize, 0, BUFFER_SIZE, 0, 0);
		pEffect->dispatcher(pEffect, effMainsChanged, 0, 1, 0, 0);
		pEffect->dispatcher(pEffect, effStartProcess, 0, 0, 0, 0);

		buffer_size = sizeof(float*) * (pEffect->numInputs + pEffect->numOutputs); // float lists
		buffer_size += sizeof(float) * BUFFER_SIZE;                                // null input
		buffer_size += sizeof(float) * BUFFER_SIZE * pEffect->numOutputs;          // outputs

		blState = ( unsigned char * ) calloc( buffer_size, 1 );

		float_list_in  = (float**) blState;
		float_list_out = float_list_in + pEffect->numInputs;
		float_null     = (float*) (float_list_out + pEffect->numOutputs);
		float_out      = float_null + BUFFER_SIZE;

		for (VstInt32 i = 0; i < pEffect->numInputs; i++)
		{
			float_list_in[i] = float_null;
		}

		for (VstInt32 i = 0; i < pEffect->numOutputs; i++)
		{
			float_list_out[i] = float_out + BUFFER_SIZE * i;
		}
	}

	VstEvents * events = NULL;

	if ( evChain ) {
		unsigned event_count = 0;
		myVstEvent * ev = evChain;
		while ( ev ) {
			ev = ev->next;
			event_count++;
		}

		events = ( VstEvents * ) malloc( sizeof(VstInt32) + sizeof(VstIntPtr) + sizeof(VstEvent*) * event_count );

		events->numEvents = event_count;
		events->reserved = 0;

		ev = evChain;

		for ( unsigned i = 0; ev; ++i ) {
			events->events[ i ] = (VstEvent*) &ev->ev;
			ev = ev->next;
		}

		pEffect->dispatcher( pEffect, effProcessEvents, 0, 0, events, 0 );
	}

	while ( len ) {
		unsigned len_to_do = min( len, BUFFER_SIZE );

		pEffect->processReplacing( pEffect, float_list_in, float_list_out, len_to_do );

		if ( uNumOutputs == 2 ) {
			for (unsigned i = 0; i < len_to_do; i++) {
				float sample = float_out[i];
				samples[0] = sample;
				sample = float_out[i + BUFFER_SIZE];
				samples[1] = sample;
				samples += 2;
			}
		} else {
			for (unsigned i = 0; i < len_to_do; i++) {
				float sample = float_out[i];
				samples[0] = sample;
				samples[1] = sample;
				samples += 2;
			}
		}

		len -= len_to_do;
	}

	if ( events ) {
		free( events );
	}

	FreeChain();
}

void VSTDriver::Render(short * samples, int len)
{
	float * float_out = (float *) _alloca( 512 * 2 * sizeof(*float_out) );
	while ( len > 0 )
	{
		int len_todo = len > 512 ? 512 : len;
		RenderFloat( float_out, len_todo );
		for ( int i = 0; i < len_todo * 2; i++ )
		{
			int sample = ( float_out[i] * 32768.f );
			if ( sample != (short)sample ) sample = 0x7FFF ^ (sample >> 31);
			samples[0] = sample;
			samples++;
		}
	}
}
