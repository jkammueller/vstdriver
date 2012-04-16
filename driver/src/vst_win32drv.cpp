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

// Implementation of DLL Exports (vstmidi_winmm.cpp contains DriverProc and modMessage)

#include "stdafx.h"

#include "resource.h"
#include "../vst_win32drv.h"
#include "VSTDirectMusicSynth.h"

HRESULT RegisterSynth(REFGUID guid, const TCHAR szDescription[]);
HRESULT UnregisterSynth(REFGUID guid);

const TCHAR cszSynthRegRoot[] = _T( REGSTR_PATH_SOFTWARESYNTHS ) _T( "\\" );
const TCHAR cszDescriptionKey[] = _T( "Description" );
const int CLSID_STRING_SIZE = 39;

LONG driverCount;

class CVSTMIDIDrvModule : public CAtlDllModuleT< CVSTMIDIDrvModule >
{
public :
	DECLARE_LIBID(LIBID_VSTMIDILib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_VSTMIDI, "{DBA099CA-820E-476B-B646-DCDE6177E058}")
};

CVSTMIDIDrvModule _AtlModule;

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
	LOG_MSG("DllMain, dwReason=%d", dwReason);
	return _AtlModule.DllMain(dwReason, lpReserved); 
}

// Used to determine whether the DLL can be unloaded by OLE
STDAPI DllCanUnloadNow(void) {
	HRESULT rc = _AtlModule.DllCanUnloadNow();
	LOG_MSG("DllCanUnloadNow(): Returning 0x%08x", rc);
	return rc;
}

// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
	HRESULT hr = _AtlModule.DllGetClassObject(rclsid, riid, ppv);
	LOG_MSG("DllGetClassObject(): Returning 0x%08x", hr);
	return hr;
}

// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer(void) {
	HRESULT hr = RegisterSynth(CLSID_VSTMIDIDirectMusicSynth, _T("VST MIDI Synth"));
	// registers object, typelib and all interfaces in typelib
	LOG_MSG("DllRegisterServer(): Returning 0x%08x", hr);
	return hr;
}

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer(void) {
	HRESULT hr = UnregisterSynth(CLSID_VSTMIDIDirectMusicSynth);
	LOG_MSG("DllUnregisterServer(): Returning 0x%08x");
	return hr;
}

HRESULT ClassIDCat(TCHAR *str, const CLSID &rclsid) {
	str = str + _tcslen(str);
	LPWSTR lplpsz;
	HRESULT hr = StringFromCLSID(rclsid, &lplpsz);
	if (SUCCEEDED(hr)) {
#ifdef UNICODE
		_tcscpy( str, lplpsz );
#else
		WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, lplpsz, CLSID_STRING_SIZE, str, CLSID_STRING_SIZE+1, NULL, NULL);
#endif
		CoTaskMemFree(lplpsz);
	} else {
		LOG_MSG("RegisterSynth() failed: StringFromCLSID() returned 0x%08x", hr);
	}
	return hr;
}

HRESULT RegisterSynth(REFGUID guid, const TCHAR szDescription[]) {
	HKEY hk;
	TCHAR szRegKey[256];

	_tcscpy(szRegKey, cszSynthRegRoot);

	HRESULT hr = ClassIDCat(szRegKey, guid);
	if (!SUCCEEDED(hr)) {
		LOG_MSG("RegisterSynth() failed transforming class ID");
		return hr;
	}
	hr = RegCreateKey(HKEY_LOCAL_MACHINE, szRegKey, &hk);
	if (!SUCCEEDED(hr)) {
		LOG_MSG("RegisterSynth() failed creating registry key");
		return hr;
	}

	hr = RegSetValueEx(hk, cszDescriptionKey, 0L, REG_SZ, (const unsigned char *)szDescription, (DWORD)((_tcslen(szDescription) + 1) * sizeof(TCHAR)));
	RegCloseKey(hk);
	if (!SUCCEEDED(hr)) {
		LOG_MSG("RegisterSynth() failed creating registry value");
		return hr;
	}

	hr = _AtlModule.DllRegisterServer(false);
	return hr;
}

HRESULT UnregisterSynth(REFGUID guid) {
	TCHAR szRegKey[256];

	_tcscpy(szRegKey, cszSynthRegRoot);

	HRESULT hr = ClassIDCat(szRegKey, guid);
	if (!SUCCEEDED(hr)) {
		LOG_MSG("UnregisterSynth() failed transforming class ID");
		return hr;
	}

	hr = RegDeleteKey(HKEY_LOCAL_MACHINE, szRegKey);
	if (!SUCCEEDED(hr)) {
		LOG_MSG("UnregisterSynth() failed deleting registry key");
		return hr;
	}
	hr = _AtlModule.DllUnregisterServer(false);
	return hr;
}
