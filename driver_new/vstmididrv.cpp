/*
    VSTi MIDI Driver
*/

//#define DEBUG
#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#if __DMC__
unsigned long _beginthreadex( void *security, unsigned stack_size,
		unsigned ( __stdcall *start_address )( void * ), void *arglist,
		unsigned initflag, unsigned *thrdaddr );
void _endthreadex( unsigned retval );
#endif

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <windows.h>
#include "mmddk.h"  
#include <mmsystem.h>
#include <tchar.h>
#include <Shlwapi.h>

extern "C" {
BOOL WINAPI DriverCallback( DWORD dwCallBack, DWORD dwFlags, HDRVR hdrvr, DWORD msg, DWORD dwUser, DWORD dwParam1, DWORD dwParam2 );	
}

#include "sound_out.h"
#include "../driver/src/VSTDriver.h"

#define MAX_DRIVERS 1
#define MAX_CLIENTS 1 // Per driver

struct Driver_Client {
	int allocated;
	DWORD instance;
	DWORD flags;
	DWORD_PTR callback;
};

struct Driver {
	int open;
	int clientCount;
	HDRVR hdrvr;
	struct Driver_Client clients[MAX_CLIENTS];
} drivers[MAX_DRIVERS];

int driverCount;

static volatile int OpenCount = 0;
static volatile int modm_closed = 1;

static CRITICAL_SECTION mim_section;
static volatile int stop_thread = 0;
static HANDLE hCalcThread = NULL;
static DWORD processPriority;
static HANDLE load_vstevent = NULL; 


static sound_out * sound_driver = NULL;
static VSTDriver * vst_driver = NULL;

static void DoStartDriver();
static void DoStopDriver();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved ){
	if (fdwReason == DLL_PROCESS_ATTACH){
		DisableThreadLibraryCalls(hinstDLL);
	}else if(fdwReason == DLL_PROCESS_DETACH){
		;
		DoStopDriver();
	}
	return TRUE;    
}

STDAPI_(LONG) DriverProc(DWORD dwDriverId, HDRVR hdrvr, UINT msg, LONG lParam1, LONG lParam2){
 
	switch(msg) {
	case DRV_FREE: // XXX never called
		return DRV_OK;
	case DRV_LOAD:
		memset(drivers, 0, sizeof(drivers));
		driverCount = 0;
		return DRV_OK;
	case DRV_OPEN:
		{
			int driverNum;
			if (driverCount == MAX_DRIVERS) {
				return 0;
			} else {
				for (driverNum = 0; driverNum < MAX_DRIVERS; driverNum++) {
					if (!drivers[driverNum].open) {
						break;
					}
					if (driverNum == MAX_DRIVERS) {
						return 0;
					}
				}
			}
			drivers[driverNum].open = 1;
			drivers[driverNum].clientCount = 0;
			drivers[driverNum].hdrvr = hdrvr;
			driverCount++;
		}
		return DRV_OK;
	case DRV_CLOSE: // XXX never called
		{
			int i;
			for (i = 0; i < MAX_DRIVERS; i++) {
				if (drivers[i].open && drivers[i].hdrvr == hdrvr) {
					drivers[i].open = 0;
					--driverCount;
					return DRV_OK;
				}
			}
		}
		return DRV_CANCEL;
	case DRV_CONFIGURE:
	case DRV_DISABLE:
	case DRV_ENABLE:
	case DRV_EXITSESSION:
	case DRV_REMOVE:
	case DRV_INSTALL:
	case DRV_POWER:
	case DRV_QUERYCONFIGURE:
		return DRV_OK;
	}
	return DRV_OK;
}

HRESULT modGetCaps(PVOID capsPtr, DWORD capsSize) {
	MIDIOUTCAPSA * myCapsA;
	MIDIOUTCAPSW * myCapsW;
	MIDIOUTCAPS2A * myCaps2A;
	MIDIOUTCAPS2W * myCaps2W;

	CHAR synthName[] = "VST MIDI Synth\0";
	WCHAR synthNameW[] = L"VST MIDI Synth\0";

	switch (capsSize) {
	case (sizeof(MIDIOUTCAPSA)):
		myCapsA = (MIDIOUTCAPSA *)capsPtr;
		myCapsA->wMid = 0xffff;
		myCapsA->wPid = 0xffff;
		memcpy(myCapsA->szPname, synthName, sizeof(synthName));
		myCapsA->wTechnology = MOD_MIDIPORT;
		myCapsA->vDriverVersion = 0x0090;
		myCapsA->wVoices = 0;
		myCapsA->wNotes = 0;
		myCapsA->wChannelMask = 0xffff;
		myCapsA->dwSupport = 0;
		return MMSYSERR_NOERROR;

	case (sizeof(MIDIOUTCAPSW)):
		myCapsW = (MIDIOUTCAPSW *)capsPtr;
		myCapsW->wMid = 0xffff;
		myCapsW->wPid = 0xffff;
		memcpy(myCapsW->szPname, synthNameW, sizeof(synthNameW));
		myCapsW->wTechnology = MOD_MIDIPORT;
		myCapsW->vDriverVersion = 0x0090;
		myCapsW->wVoices = 0;
		myCapsW->wNotes = 0;
		myCapsW->wChannelMask = 0xffff;
		myCapsW->dwSupport = 0;
		return MMSYSERR_NOERROR;

	case (sizeof(MIDIOUTCAPS2A)):
		myCaps2A = (MIDIOUTCAPS2A *)capsPtr;
		myCaps2A->wMid = 0xffff;
		myCaps2A->wPid = 0xffff;
		memcpy(myCaps2A->szPname, synthName, sizeof(synthName));
		myCaps2A->wTechnology = MOD_MIDIPORT;
		myCaps2A->vDriverVersion = 0x0090;
		myCaps2A->wVoices = 0;
		myCaps2A->wNotes = 0;
		myCaps2A->wChannelMask = 0xffff;
		myCaps2A->dwSupport = 0;
		return MMSYSERR_NOERROR;

	case (sizeof(MIDIOUTCAPS2W)):
		myCaps2W = (MIDIOUTCAPS2W *)capsPtr;
		myCaps2W->wMid = 0xffff;
		myCaps2W->wPid = 0xffff;
		memcpy(myCaps2W->szPname, synthNameW, sizeof(synthNameW));
		myCaps2W->wTechnology = MOD_MIDIPORT;
		myCaps2W->vDriverVersion = 0x0090;
		myCaps2W->wVoices = 0;
		myCaps2W->wNotes = 0;
		myCaps2W->wChannelMask = 0xffff;
		myCaps2W->dwSupport = 0;
		return MMSYSERR_NOERROR;

	default:
		return MMSYSERR_ERROR;
	}
}


struct evbuf_t{
	UINT uMsg;
	DWORD	dwParam1;
	DWORD	dwParam2;
	int exlen;
	unsigned char *sysexbuffer;
};
#define EVBUFF_SIZE 512
static struct evbuf_t evbuf[EVBUFF_SIZE];
static UINT  evbwpoint=0;
static UINT  evbrpoint=0;
static UINT evbsysexpoint;

int vstsyn_buf_check(void){
	int retval;
	EnterCriticalSection(&mim_section);
	retval = (evbrpoint != evbwpoint) ? ~0 :  0;
	LeaveCriticalSection(&mim_section);
	return retval;
}

static const unsigned char sysex_gm_reset[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };
static const unsigned char sysex_gm2_reset[]= { 0xF0, 0x7E, 0x7F, 0x09, 0x03, 0xF7 };
static const unsigned char sysex_gs_reset[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7 };
static const unsigned char sysex_xg_reset[] = { 0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7 };

static BOOL is_gs_reset(const unsigned char * data, unsigned size)
{
	if ( size != _countof( sysex_gs_reset ) ) return FALSE;

	if ( memcmp( data, sysex_gs_reset, 5 ) != 0 ) return FALSE;
	if ( memcmp( data + 7, sysex_gs_reset + 7, 2 ) != 0 ) return  FALSE;
	if ( ( ( data[ 5 ] + data[ 6 ] + 1 ) & 127 ) != data[ 9 ] ) return  FALSE;
	if ( data[ 10 ] != sysex_gs_reset[ 10 ] ) return  FALSE;

	return TRUE;
}

int vstsyn_play_some_data(void){
	UINT uMsg;
	DWORD	dwParam1;
	DWORD	dwParam2;
	
	UINT evbpoint;
	MIDIHDR *IIMidiHdr;
	int exlen;
	unsigned char *sysexbuffer;
	int played;
		
	played=0;
		if( !vstsyn_buf_check() ){ 
			played=~0;
			return played;
		}
		do{
			EnterCriticalSection(&mim_section);
			evbpoint=evbrpoint;
			if (++evbrpoint >= EVBUFF_SIZE)
					evbrpoint -= EVBUFF_SIZE;

			uMsg=evbuf[evbpoint].uMsg;
			dwParam1=evbuf[evbpoint].dwParam1;
			dwParam2=evbuf[evbpoint].dwParam2;
		    exlen=evbuf[evbpoint].exlen;
			sysexbuffer=evbuf[evbpoint].sysexbuffer;
			
			LeaveCriticalSection(&mim_section);
			switch (uMsg) {
			case MODM_DATA:
				vst_driver->ProcessMIDIMessage(dwParam1);
				break;
			case MODM_LONGDATA:
#ifdef DEBUG
	FILE * logfile;
	logfile = fopen("c:\\dbglog2.log","at");
	if(logfile!=NULL) {
		for(int i = 0 ; i < exlen ; i++)
			fprintf(logfile,"%x ", sysexbuffer[i]);
		fprintf(logfile,"\n");
	}
	fclose(logfile);
#endif
				vst_driver->ProcessSysEx(sysexbuffer, exlen);
				free(sysexbuffer);
				break;
			}
		}while(vstsyn_buf_check());	
	return played;
}

BOOL IsVistaOrNewer() {
	static bool initialized = false;
	static BOOL is_vista;
	if (!initialized) {
		OSVERSIONINFOEX osvi;
		BOOL bOsVersionInfoEx;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*) &osvi);
		if(bOsVersionInfoEx == FALSE) is_vista = FALSE;
		else if ( VER_PLATFORM_WIN32_NT==osvi.dwPlatformId && 
			osvi.dwMajorVersion > 5 )
			is_vista = TRUE;
		initialized = true;
	}
	return is_vista;
}


unsigned __stdcall threadfunc(LPVOID lpV){
	unsigned i;
	int opend=0;

	while(opend == 0 && stop_thread == 0) {
		Sleep(100);
		sound_driver = create_sound_out_ds();
		const char * err = sound_driver->open(GetDesktopWindow(), 44100, 2, !!IsVistaOrNewer(), 44 * 2, 70);
		if (err) {
			delete sound_driver;
			sound_driver = NULL;
			continue;
		}
		vst_driver = new VSTDriver;
		if (!vst_driver->OpenVSTDriver()) {
			delete vst_driver;
			vst_driver = NULL;
			delete sound_driver;
			sound_driver = NULL;
			continue;
		}
		SetEvent(load_vstevent);
		opend = 1;
	}

	while(stop_thread == 0){
		vstsyn_play_some_data();
		if (IsVistaOrNewer()) {
			float sound_buffer[44 * 2];
			vst_driver->RenderFloat( sound_buffer, 44 );
			sound_driver->write_frame( sound_buffer, 44 * 2, true );
		} else {
			short sound_buffer[44 * 2];
			vst_driver->Render( sound_buffer, 44 );
			sound_driver->write_frame( sound_buffer, 44 * 2, true );
		}
	}
	stop_thread=0;
	delete vst_driver;
	vst_driver = NULL;
	delete sound_driver;
	sound_driver = NULL;
	_endthreadex(0);
	return 0;
}

void DoCallback(int driverNum, int clientNum, DWORD msg, DWORD param1, DWORD param2) {
	struct Driver_Client *client = &drivers[driverNum].clients[clientNum];
	DriverCallback(client->callback, client->flags, drivers[driverNum].hdrvr, msg, client->instance, param1, param2);
}

void DoStartDriver() {
	if (modm_closed  == 1) {
		DWORD result;
		unsigned int thrdaddr;
		InitializeCriticalSection(&mim_section);
		processPriority = GetPriorityClass(GetCurrentProcess());
		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
		load_vstevent = CreateEvent( 
			NULL,               // default security attributes
			TRUE,               // manual-reset event
			FALSE,              // initial state is nonsignaled
			TEXT("VSTiEvent")  // object name
			); 
		hCalcThread=(HANDLE)_beginthreadex(NULL,0,threadfunc,0,0,&thrdaddr);
		SetPriorityClass(hCalcThread, REALTIME_PRIORITY_CLASS);
		SetThreadPriority(hCalcThread, THREAD_PRIORITY_TIME_CRITICAL);
		result = WaitForSingleObject(load_vstevent,INFINITE);
		if (result == WAIT_OBJECT_0)
		{
			CloseHandle(load_vstevent);
		}
		modm_closed = 0;
	}
}

void DoStopDriver() {
	if (modm_closed == 0){
		stop_thread = 1;
		WaitForSingleObject(hCalcThread, INFINITE);
		CloseHandle(hCalcThread);
		modm_closed = 1;
		SetPriorityClass(GetCurrentProcess(), processPriority);
	}
	DeleteCriticalSection(&mim_section);
}

void DoResetDriver(DWORD dwParam1, DWORD dwParam2) {
	UINT evbpoint;
	int exlen = _countof(sysex_gm_reset);
	unsigned char *sysexbuffer = (unsigned char*) malloc(exlen);
	memcpy(sysexbuffer, sysex_gm_reset, exlen);
	EnterCriticalSection(&mim_section);
	evbpoint = evbwpoint;
	if (++evbwpoint >= EVBUFF_SIZE)
		evbwpoint -= EVBUFF_SIZE;
	evbuf[evbpoint].uMsg = MODM_LONGDATA;
	evbuf[evbpoint].dwParam1 = dwParam1;
	evbuf[evbpoint].dwParam2 = dwParam2;
	evbuf[evbpoint].exlen=exlen;
	evbuf[evbpoint].sysexbuffer=sysexbuffer;
	LeaveCriticalSection(&mim_section);
}

LONG DoOpenDriver(struct Driver *driver, UINT uDeviceID, UINT uMsg, DWORD dwUser, DWORD dwParam1, DWORD dwParam2) {
	int clientNum;
	MIDIOPENDESC *desc;
	if (driver->clientCount == 0) {
		DoStartDriver();
		DoResetDriver(dwParam1, dwParam2);
		clientNum = 0;
	} else if (driver->clientCount == MAX_CLIENTS) {
		return MMSYSERR_ALLOCATED;
	} else {
		int i;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (!driver->clients[i].allocated) {
				break;
			}
		}
		if (i == MAX_CLIENTS) {
			return MMSYSERR_ALLOCATED;
		}
		clientNum = i;
	}
	desc = (MIDIOPENDESC *)dwParam1;
	driver->clients[clientNum].allocated = 1;
	driver->clients[clientNum].flags = HIWORD(dwParam2);
	driver->clients[clientNum].callback = desc->dwCallback;
	driver->clients[clientNum].instance = desc->dwInstance;
	*(LONG *)dwUser = clientNum;
	driver->clientCount++;
	SetPriorityClass(GetCurrentProcess(), processPriority);
	DoCallback(uDeviceID, clientNum, MOM_OPEN, 0, 0);
	return MMSYSERR_NOERROR;
}

LONG DoCloseDriver(struct Driver *driver, UINT uDeviceID, UINT uMsg, DWORD dwUser, DWORD dwParam1, DWORD dwParam2) {
	if (!driver->clients[dwUser].allocated) {
		return MMSYSERR_INVALPARAM;
	}

	driver->clients[dwUser].allocated = 0;
	driver->clientCount--;
	if(driver->clientCount <= 0) {
		DoResetDriver(dwParam1, dwParam2);
		driver->clientCount = 0;
	}
	DoCallback(uDeviceID, dwUser, MOM_CLOSE, 0, 0);
	return MMSYSERR_NOERROR;
}

STDAPI_(LONG) midMessage(UINT uDeviceID, UINT uMsg, DWORD dwUser, DWORD dwParam1, DWORD dwParam2) {
	struct Driver *driver = &drivers[uDeviceID];
	switch (uMsg) {
	case MIDM_OPEN:
		return DoOpenDriver(driver, uDeviceID, uMsg, dwUser, dwParam1, dwParam2);

	case MIDM_CLOSE:
		return DoCloseDriver(driver, uDeviceID, uMsg, dwUser, dwParam1, dwParam2);

	default:
		return MMSYSERR_NOERROR;
		break;
	}
}

STDAPI_(LONG) modMessage(UINT uDeviceID, UINT uMsg, DWORD dwUser, DWORD dwParam1, DWORD dwParam2){
	MIDIHDR *IIMidiHdr;	
	UINT evbpoint;
	struct Driver *driver = &drivers[uDeviceID];
	int exlen = 0;
	unsigned char *sysexbuffer = NULL ;
	DWORD result = 0;
	switch (uMsg) {
	case MODM_OPEN:
		return DoOpenDriver(driver, uDeviceID, uMsg, dwUser, dwParam1, dwParam2);
		break;
	case MODM_PREPARE:
		return MMSYSERR_NOTSUPPORTED;
		break;
	case MODM_UNPREPARE:
		return MMSYSERR_NOTSUPPORTED;
		break;
	case MODM_GETDEVCAPS:
		return modGetCaps((PVOID)dwParam1, dwParam2);
		break;
	case MODM_RESET:
		DoResetDriver(dwParam1, dwParam2);
		return MMSYSERR_NOERROR;
	case MODM_LONGDATA:
		IIMidiHdr = (MIDIHDR *)dwParam1;
		if( !(IIMidiHdr->dwFlags & MHDR_PREPARED) ) return MIDIERR_UNPREPARED;
		IIMidiHdr->dwFlags &= ~MHDR_DONE;
		IIMidiHdr->dwFlags |= MHDR_INQUEUE;
		IIMidiHdr = (MIDIHDR *) dwParam1;
		exlen=(int)IIMidiHdr->dwBufferLength;
		if( NULL == (sysexbuffer = (unsigned char *)malloc(exlen * sizeof(char)))){
			return MMSYSERR_NOMEM;
		}else{
			memcpy(sysexbuffer,IIMidiHdr->lpData,exlen);
#ifdef DEBUG
	FILE * logfile;
	logfile = fopen("c:\\dbglog.log","at");
	if(logfile!=NULL) {
		fprintf(logfile,"sysex %d byete\n", exlen);
		for(int i = 0 ; i < exlen ; i++)
			fprintf(logfile,"%x ", sysexbuffer[i]);
		fprintf(logfile,"\n");
	}
	fclose(logfile);
#endif
		}
		IIMidiHdr->dwFlags &= ~MHDR_INQUEUE;
		IIMidiHdr->dwFlags |= MHDR_DONE;
		DoCallback(uDeviceID, dwUser, MOM_DONE, dwParam1, 0);
	case MODM_DATA:
		EnterCriticalSection(&mim_section);
		evbpoint = evbwpoint;
		if (++evbwpoint >= EVBUFF_SIZE)
			evbwpoint -= EVBUFF_SIZE;
		evbuf[evbpoint].uMsg = uMsg;
		evbuf[evbpoint].dwParam1 = dwParam1;
		evbuf[evbpoint].dwParam2 = dwParam2;
		evbuf[evbpoint].exlen=exlen;
		evbuf[evbpoint].sysexbuffer=sysexbuffer;
		LeaveCriticalSection(&mim_section);
		return MMSYSERR_NOERROR;
		break;		
	case MODM_GETNUMDEVS:
		return 0x1;
		break;
	case MODM_CLOSE:
		return DoCloseDriver(driver, uDeviceID, uMsg, dwUser, dwParam1, dwParam2);
		break;
	default:
		return MMSYSERR_NOERROR;
		break;
	}
}

