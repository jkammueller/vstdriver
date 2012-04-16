/* Copyright (C) 2003, 2004, 2005 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
 * Copyright (C) 2011 Chris Moeller, Brad Miller
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

#include "stdafx.h"

//#define BENCHMARK

#ifdef BENCHMARK
#include <time.h>
#endif
#include <ks.h>
#include <initguid.h>
#include <dmusics.h>
#include <dmksctrl.h>
#include "inc/dmusprop.h"

#include "../vst_win32drv.h"
#include "VSTDirectMusicSynth.h"

#ifdef _DEBUG
void LOG_MSG(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	va_end(ap);
}
#endif

static const KSIDENTIFIER ksSynthProperties[] = {
 {GUID_DMUS_PROP_XG_Hardware, 0,                               KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT},
 {GUID_DMUS_PROP_Effects,     0,                               KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT},
 {KSPROPSETID_Synth,          KSPROPERTY_SYNTH_CAPS,           KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT},
 {KSPROPSETID_Synth,          KSPROPERTY_SYNTH_PORTPARAMETERS, KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT},
 {KSPROPSETID_Synth,          KSPROPERTY_SYNTH_VOLUME,         KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT},
 {KSPROPSETID_Synth,          KSPROPERTY_SYNTH_VOLUMEBOOST,    KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT},
 {KSPROPSETID_Synth,          KSPROPERTY_SYNTH_CHANNELGROUPS,  KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT},
 {KSPROPSETID_Synth,          KSPROPERTY_SYNTH_VOICEPRIORITY,  KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT},
 {KSPROPSETID_Synth,          KSPROPERTY_SYNTH_RUNNINGSTATS,   KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT},
 {KSPROPSETID_Synth,          KSPROPERTY_SYNTH_LATENCYCLOCK,   KSPROPERTY_TYPE_GET                       | KSPROPERTY_TYPE_BASICSUPPORT}
};

VSTMIDIDirectMusicSynth::VSTMIDIDirectMusicSynth() {
	//LOG_MSG("VSTMIDIDirectMusicSynth::VSTMIDIDirectMusicSynth()");
	enabled = false;
	open = false;
	eventsMutex = CreateMutex( NULL, FALSE, NULL );
	events = NULL;

	sink = NULL;
	drv = NULL;
}

VSTMIDIDirectMusicSynth::~VSTMIDIDirectMusicSynth() {
	//LOG_MSG("VSTMIDIDirectMusicSynth::~VSTMIDIDirectMusicSynth()");
	if (drv != NULL)
	{
		drv->CloseVSTDriver();
		delete drv;
	}
	if (sink != NULL)
		sink->Release();
	while (events) {
		MidiEvent *next = events->getNext();
		delete events;
		events = next;
	}
	CloseHandle(eventsMutex);
}

STDMETHODIMP VSTMIDIDirectMusicSynth::InterfaceSupportsErrorInfo(REFIID riid) {
	//LOG_MSG("VSTMIDIDirectMusicSynth::InterfaceSupportsErrorInfo()");
	static const IID* arr[] = {
		&IID_IVSTMIDIDirectMusicSynth,
	};

	for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++) {
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}

HRESULT VSTMIDIDirectMusicSynth::Open(THIS_ LPDMUS_PORTPARAMS pPortParams) {
	if(open) {
		return DMUS_E_ALREADYOPEN;
	}
	portParams = pPortParams;

	//init synth
	if (drv == NULL) {
		drv = new VSTDriver();
	}
	if(!drv->OpenVSTDriver())
		return DMUS_E_DRIVER_FAILED;
	open = true;
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::Close(THIS) {
	if (!open) {
		return DMUS_E_ALREADYCLOSED;
	}
	open = false;
    drv->CloseVSTDriver();
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::GetAppend(THIS_ DWORD *pdwAppend) {
	return E_NOTIMPL;
}

HRESULT VSTMIDIDirectMusicSynth::Download(THIS_ LPHANDLE phDownload, LPVOID pvData, LPBOOL pbFree) {
	return E_NOTIMPL;
}

HRESULT VSTMIDIDirectMusicSynth::Unload(THIS_ HANDLE hDownload,HRESULT(CALLBACK *lpFreeHandle)(HANDLE,HANDLE),HANDLE hUserData) {
	return E_NOTIMPL;
}

HRESULT VSTMIDIDirectMusicSynth::GetFormat(THIS_ LPWAVEFORMATEX pWaveFormatEx, LPDWORD pdwWaveFormatExSize) {
	if(pWaveFormatEx==NULL) {
		return sizeof(LPWAVEFORMATEX);
	}
	pWaveFormatEx->cbSize = 0;
	pWaveFormatEx->nAvgBytesPerSec = 44100 * 2 * 2;
	pWaveFormatEx->nBlockAlign = (2*16)/8;
	pWaveFormatEx->wFormatTag = WAVE_FORMAT_PCM;
	pWaveFormatEx->nChannels = 2;
	pWaveFormatEx->nSamplesPerSec = 44100;
	pWaveFormatEx->wBitsPerSample = 16;
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::SetChannelPriority(THIS_ DWORD dwChannelGroup, DWORD dwChannel, DWORD dwPriority) {
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::GetChannelPriority(THIS_ DWORD dwChannelGroup, DWORD dwChannel, LPDWORD pdwPriority) {
	return E_NOTIMPL;
}

HRESULT VSTMIDIDirectMusicSynth::SetNumChannelGroups(THIS_ DWORD dwGroups) {
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::GetLatencyClock(THIS_ IReferenceClock **pClock) {
	if (sink == NULL) {
		return DMUS_E_NOSYNTHSINK;
	}
	return sink->GetLatencyClock(pClock);
}

class BufferReader {
public:
	LPBYTE pbBufPtr;
	signed long dwLen;
	REFERENCE_TIME sTime;

	BufferReader(REFERENCE_TIME startTime, LPBYTE pbBuffer, DWORD len) {
		sTime = startTime;
		pbBufPtr = pbBuffer;
		dwLen = len;
	}

	~BufferReader() {
	}

	HRESULT GetNextEvent(LPREFERENCE_TIME prt, LPDWORD pdwChannelGroup, LPDWORD pdwLength, LPBYTE* ppData) {
		if (dwLen<=0)
			return S_FALSE;

		DMUS_EVENTHEADER *evtPtr = (DMUS_EVENTHEADER *)pbBufPtr;
		DWORD blockSize = DMUS_EVENT_SIZE(evtPtr->cbEvent);

		*prt = sTime + evtPtr->rtDelta;
		*pdwChannelGroup = evtPtr->dwChannelGroup;
		*pdwLength = evtPtr->cbEvent;
		*ppData = (pbBufPtr + sizeof(DMUS_EVENTHEADER));

		dwLen -= blockSize;
		pbBufPtr += blockSize;

		return S_OK;
	}
};

long long VSTMIDIDirectMusicSynth::PeekNextMidiEventTime(long long def) {
	WaitForSingleObject(eventsMutex, INFINITE);
	LONGLONG nextTime;
	if (events) {
		nextTime = events->getTime();
	} else {
		nextTime = def;
	}
	ReleaseMutex(eventsMutex);
	return nextTime;
}

MidiEvent *VSTMIDIDirectMusicSynth::DequeueMidiEvent(long long maxTime) {
	WaitForSingleObject(eventsMutex, INFINITE);
	MidiEvent *result;
	if (events != NULL && events->getTime() <= maxTime) {
		result = events;
		events = events->getNext();
	} else {
		result = NULL;
	}
	ReleaseMutex(eventsMutex);
	return result;
}

void VSTMIDIDirectMusicSynth::EnqueueMidiEvent(MidiEvent *event) {
	WaitForSingleObject(eventsMutex, INFINITE);
	MidiEvent *lastEvent = NULL;
	MidiEvent *nextEvent = events;
	for (;;) {
		if (nextEvent == NULL || nextEvent->getTime() > event->getTime()) {
			event->chainEvent(nextEvent);
			if (lastEvent == NULL) {
				events = event;
			} else {
				lastEvent->chainEvent(event);
			}
			break;
		}
		lastEvent = nextEvent;
		nextEvent = nextEvent->getNext();
	}
	ReleaseMutex(eventsMutex);
}

HRESULT VSTMIDIDirectMusicSynth::PlayBuffer(THIS_ REFERENCE_TIME rt, LPBYTE pbBuffer, DWORD cbBuffer) {
	if (!open) {
		return DMUS_E_SYNTHNOTCONFIGURED;
	}
	if (sink == NULL) {
		return DMUS_E_NOSYNTHSINK;
	}
#ifdef BENCHMARK
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq)) {
		LOG_MSG("QueryPerformanceFrequency() failed!");
		freq.QuadPart = 0;
	}
	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);
#endif
	LONGLONG stTime;
	MidiEvent *tmpEvent;
	BufferReader *myReader = new BufferReader(rt, pbBuffer, cbBuffer);

	REFERENCE_TIME bufRt;
	DWORD chanGroup;
	DWORD bufLen;
	LPBYTE bufData;

	while(myReader->GetNextEvent(&bufRt, &chanGroup, &bufLen, &bufData) != S_FALSE) {
		HRESULT res = sink->RefTimeToSample(bufRt, &stTime);
		if (res != S_OK)
			return res;
		if(bufLen <= sizeof(DWORD)) {
			tmpEvent = new MidiEvent();
			tmpEvent->assignMsg(*(DWORD *)bufData, stTime);
			EnqueueMidiEvent(tmpEvent);
		} else {
			tmpEvent = new MidiEvent();
			tmpEvent->assignSysex(bufData,bufLen,stTime);
			EnqueueMidiEvent(tmpEvent);
		}
	}

	delete myReader;

#ifdef BENCHMARK
	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);
	long long duration = end.QuadPart - start.QuadPart;
	duration /= freq.QuadPart / 1000;
	if (duration > 0)
		LOG_MSG("PlayBuffer took %I64d", duration);
#endif
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::SetMasterClock(THIS_ IReferenceClock *pClock) {
	this->masterClock = pClock;
	if (sink != NULL) {
		return sink->SetMasterClock(masterClock);
	}
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::GetPortCaps(THIS_ LPDMUS_PORTCAPS pCaps) {
	pCaps->dwClass = DMUS_PC_OUTPUTCLASS;
	pCaps->dwEffectFlags = DMUS_EFFECT_REVERB;
	pCaps->dwFlags = DMUS_PC_GMINHARDWARE | DMUS_PC_SOFTWARESYNTH | DMUS_PC_DIRECTSOUND /*|  DMUS_PC_SHAREABLE*/;
	pCaps->dwMaxAudioChannels = 2;
	pCaps->dwMaxChannelGroups = 1;
	pCaps->dwMaxVoices = 256;
	pCaps->dwMemorySize = 0;
	pCaps->dwSize = sizeof(DMUS_PORTCAPS);
	pCaps->dwType = DMUS_PORT_USER_MODE_SYNTH;
	//pCaps->dwType = DMUS_PORT_WINMM_DRIVER;
	pCaps->guidPort = CLSID_VSTMIDIDirectMusicSynth;
	lstrcpyW(&pCaps->wszDescription[0], L"VST MIDI Synth");
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::GetRunningStats(THIS_ LPDMUS_SYNTHSTATS pStats) {
	return E_NOTIMPL;
}

HRESULT VSTMIDIDirectMusicSynth::Activate(THIS_ BOOL fEnable) {
	if (sink == NULL) {
		return DMUS_E_NOSYNTHSINK;
	}
	if (!fEnable && !enabled) {
		return S_FALSE;
	}
	if (fEnable && enabled) {
		return DMUS_E_SYNTHACTIVE;
	}
	HRESULT res = sink->Activate(fEnable);
	if (res == S_OK) {
		enabled = fEnable;
	}
	return res;
}

HRESULT VSTMIDIDirectMusicSynth::Render(THIS_ short *pBuffer, DWORD dwLength, LONGLONG llPosition) {
	if (!open) {
		return DMUS_E_SYNTHNOTCONFIGURED;
	}
#ifdef BENCHMARK
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq)) {
		LOG_MSG("QueryPerformanceFrequency() failed!");
		freq.QuadPart = 0;
	}
	double div = (double)(freq.QuadPart / 1000);
#endif
	while (dwLength > 0) {
		DWORD thisLength = dwLength;

#ifdef BENCHMARK
		double durationMIDI;
		double durationSample;
		LARGE_INTEGER start;
		LARGE_INTEGER end;
		QueryPerformanceCounter(&start);
#endif
		MidiEvent *event;
		while ((event = DequeueMidiEvent(llPosition)) != NULL) {
			switch(event->getType()) {
			case ShortMsg:
				drv->ProcessMIDIMessage(event->midiMsg);
				break;
			case SysexData:
				drv->ProcessSysEx(event->sysexInfo, event->sysexLen);
				break;
			default:
				// Shouldn't ever get here
				LOG_MSG("*** Error: Render() hit an unknown message type");
				break;
			}
			delete event;
		}
		LONGLONG nextMidiEventIn = PeekNextMidiEventTime(llPosition + thisLength) - llPosition;
		if (nextMidiEventIn <= 0) {
			LOG_MSG("*** Warning: nextMidiEventIn==0");
		}
		else if (nextMidiEventIn < thisLength) {
			LOG_MSG("*** Warning: MIDI required in %ld, but samples want to play for %ld", (DWORD)nextMidiEventIn, thisLength);
			thisLength = (DWORD)nextMidiEventIn;
		}
#ifdef BENCHMARK
		QueryPerformanceCounter(&end);
		durationMIDI = (end.QuadPart - start.QuadPart) / div;
		QueryPerformanceCounter(&start);
#endif
		drv->Render(pBuffer, thisLength);
		pBuffer += thisLength * 2; // One for each channel
		llPosition += thisLength;
		dwLength -= thisLength;
#ifdef BENCHMARK
		QueryPerformanceCounter(&end);
		durationSample = (end.QuadPart - start.QuadPart) / div;
		double durationTotal = durationMIDI + durationSample;
		if (durationTotal > 0.0) {
			double samplesPerMilli = thisLength / durationTotal;
			if (samplesPerMilli < 70.0)
				LOG_MSG("%04ld samples at %.2f s/milli (MIDI: %.2f millis, Sample: %.2f millis)", thisLength, samplesPerMilli, durationMIDI, durationSample);
		}
#endif
	}
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::SetSynthSink(THIS_ IDirectMusicSynthSink *pSynthSink){
	if (sink != NULL)
		sink->Release();
	sink = pSynthSink;
	if (sink != NULL) {
		sink->AddRef();
		sink->Init((IDirectMusicSynth *)this);
		if (masterClock != NULL) {
			sink->SetMasterClock(masterClock);
		}
	}
	enabled = false;
	return S_OK;
}

HRESULT VSTMIDIDirectMusicSynth::KsProperty(THIS_ IN PKSPROPERTY Property, IN ULONG PropertyLength, IN OUT LPVOID PropertyData, IN ULONG DataLength, OUT ULONG* BytesReturned) {
#if 1 //FIXME: Below code is pretty bogus
	LOG_MSG("KsProperty called!!!!!!!!!!!!");
	const WORD c_wMaxProps = SIZEOF_ARRAY(ksSynthProperties);
	WORD wPropIdx;

	for (wPropIdx = 0; wPropIdx < c_wMaxProps; wPropIdx++) {
		if ((ksSynthProperties[wPropIdx].Set == Property->Set) && (ksSynthProperties[wPropIdx].Id == Property->Id)) {
			// if return buffer can hold a ULONG, return the access flags
			PULONG AccessFlags = (PULONG)PropertyData;
			*AccessFlags = ksSynthProperties[wPropIdx].Flags;
			// set the return value size
			*BytesReturned = sizeof(ULONG);
			break;
		}
	}

	if (wPropIdx == c_wMaxProps) {
		*BytesReturned = 0;
		return DMUS_E_UNKNOWN_PROPERTY;
	}
	return S_OK;
#endif
	//return DMUS_E_UNKNOWN_PROPERTY;
}

HRESULT VSTMIDIDirectMusicSynth::KsEvent(THIS_ IN PKSEVENT Event OPTIONAL, IN ULONG EventLength, IN OUT LPVOID EventData, IN ULONG DataLength, OUT ULONG* BytesReturned) {
	return E_NOTIMPL;
}

HRESULT VSTMIDIDirectMusicSynth::KsMethod(THIS_ IN PKSMETHOD Method, IN ULONG MethodLength, IN OUT LPVOID MethodData, IN ULONG DataLength, OUT ULONG* BytesReturned) {
	return E_NOTIMPL;
}
