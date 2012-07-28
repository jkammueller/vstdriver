#if !defined(AFX_VIEWS_H__20020629_8D64_963C_A351_0080AD509054__INCLUDED_)
#define AFX_VIEWS_H__20020629_8D64_963C_A351_0080AD509054__INCLUDED_

#include <iostream>
#include <fstream> 
#include "utf8conv.h"
using namespace std;
using namespace utf8util;
#include "../driver/src/inc/aeffect.h"
#include "../driver/src/inc/aeffectx.h"
#include "../common/settings.h"
typedef AEffect* (*PluginEntryProc) (audioMasterCallback audioMaster);
static INT_PTR CALLBACK EditorProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct MyDLGTEMPLATE: DLGTEMPLATE
{
	WORD ext[3];
	MyDLGTEMPLATE ()
	{ memset (this, 0, sizeof(*this)); };
};

std::wstring stripExtension(const std::wstring& fileName)
{
	const int length = fileName.length();
	for (int i=0; i!=length; ++i)
	{
		if (fileName[i]=='.') 
		{
			return fileName.substr(0,i);
		}
	}
	return fileName;
}

void settings_load(AEffect * effect)
{
	ifstream file;
	long lResult;
	TCHAR vst_path[256] = {0};
	ULONG size;
	CRegKeyEx reg;
	wstring fname;
	lResult = reg.Create(HKEY_CURRENT_USER, L"Software\\VSTi Driver");
	if (lResult == ERROR_SUCCESS){
		lResult = reg.QueryStringValue(L"plugin",NULL,&size);
		if (lResult == ERROR_SUCCESS) {
			reg.QueryStringValue(L"plugin",vst_path,&size);
			wstring ext = vst_path;
			fname = stripExtension(ext);
			fname += L".set";
			file.open(fname,ifstream::binary);
			if (file.good())
			{
				file.seekg(0,ifstream::end);
				unsigned int chunk_size = file.tellg();
				file.seekg(0);
				vector<uint8_t> chunk;
				chunk.resize( chunk_size );
				file.read( (char*) chunk.data(), chunk_size );
				setChunk( effect, chunk );
			}
			file.close();
		}
		reg.Close();
	}
}

void settings_save(AEffect * effect)
{
	ofstream file;
	long lResult;
	TCHAR vst_path[256] = {0};
	ULONG size;
	CRegKeyEx reg;
	wstring fname;
	lResult = reg.Create(HKEY_CURRENT_USER, L"Software\\VSTi Driver");
	if (lResult == ERROR_SUCCESS){
		lResult = reg.QueryStringValue(L"plugin",NULL,&size);
		if (lResult == ERROR_SUCCESS) {
			reg.QueryStringValue(L"plugin",vst_path,&size);
			wstring ext = vst_path;
			fname = stripExtension(ext);
			fname += L".set";
			file.open(fname,ofstream::binary);
			if (file.good())
			{
				vector<uint8_t> chunk;
				getChunk( effect, chunk );
				file.write( ( const char * ) chunk.data(), chunk.size() );
			}
			file.close();
		}
		reg.Close();
	}
}

INT_PTR CALLBACK EditorProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	
	AEffect* effect;
	switch(msg)
	{
	case WM_INITDIALOG :
		{
			SetWindowLongPtr(hwnd,GWLP_USERDATA,lParam);
			effect = (AEffect*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
			SetWindowText (hwnd, L"VST Editor");
			SetTimer (hwnd, 1, 20, 0);
			if(effect)
			{
				settings_load(effect);
				effect->dispatcher (effect, effEditOpen, 0, 0, hwnd, 0);
				ERect* eRect = 0;
				effect->dispatcher (effect, effEditGetRect, 0, 0, &eRect, 0);
				if(eRect)
				{
					int width = eRect->right - eRect->left;
					int height = eRect->bottom - eRect->top;
					if(width < 100)
						width = 100;
					if(height < 100)
						height = 100;
					RECT wRect;
					SetRect (&wRect, 0, 0, width, height);
					AdjustWindowRectEx (&wRect, GetWindowLong (hwnd, GWL_STYLE), FALSE, GetWindowLong (hwnd, GWL_EXSTYLE));
					width = wRect.right - wRect.left;
					height = wRect.bottom - wRect.top;
					SetWindowPos (hwnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE);
				}
			}
		}
		break;
	case WM_TIMER :
		effect = (AEffect*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
		if(effect)
			effect->dispatcher (effect, effEditIdle, 0, 0, 0, 0);
		break;
	case WM_CLOSE :
		{
			effect = (AEffect*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
			KillTimer (hwnd, 1);
			if(effect)
			{
				settings_save(effect);
				effect->dispatcher (effect, effEditClose, 0, 0, 0, 0);
			}

			EndDialog (hwnd, IDOK);
		}
		break;
	}

	return 0;
}



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
using namespace std;
using namespace utf8util;
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CView1 : public CDialogImpl<CView1>
{
	CEdit vst_info;
	CButton vst_load, vst_configure;
	CStatic vst_vendor, vst_effect, vst_product;
	TCHAR vst_path[MAX_PATH];
	HMODULE hDll;
	AEffect    * effect;
public:
   enum { IDD = IDD_MAIN };
   BEGIN_MSG_MAP(CView1)
	   MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialogView1)
	   COMMAND_ID_HANDLER(IDC_VSTLOAD,OnButtonAdd)
	   COMMAND_ID_HANDLER(IDC_VSTCONFIG,OnButtonConfig)
   END_MSG_MAP()

   CView1() { effect = NULL; }
   ~CView1() { free_vst(); }

   void load_settings()
   {
	   long lResult;
	   vst_path[0] = 0;
	   ULONG size;
	   CRegKeyEx reg;
	   lResult = reg.Create(HKEY_CURRENT_USER, L"Software\\VSTi Driver");
	   if (lResult == ERROR_SUCCESS){
		   lResult = reg.QueryStringValue(L"plugin",NULL,&size);
		   if (lResult == ERROR_SUCCESS) {
			   reg.QueryStringValue(L"plugin",vst_path,&size);
		   }
		   reg.Close();
		   vst_info.SetWindowText(vst_path);
		  load_vst(vst_path);
		  vst_configure.EnableWindow(!!(effect->flags & effFlagsHasEditor));
	   }
	   
   }

   LRESULT OnButtonAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
   {
	   TCHAR szFileName[MAX_PATH];
	   LPCTSTR sFiles = 
		   L"VSTi instruments (*.dll)\0*.dll\0"
		   L"All Files (*.*)\0*.*\0\0";
	   CFileDialog dlg( TRUE, NULL, vst_path, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, sFiles);
	   if (dlg.DoModal() == IDOK)
	   {
		   lstrcpy(szFileName,dlg.m_szFileName);
		   if (load_vst(szFileName))
		   {
			   HKEY hKey, hSubKey;
			   long lResult;
			   CRegKeyEx reg;
			   lResult = reg.Create(HKEY_CURRENT_USER, L"Software\\VSTi Driver");
			   reg.SetStringValue(L"plugin",szFileName);
			   reg.Close();
			   vst_info.SetWindowText(szFileName);
			   vst_configure.EnableWindow(!!(effect->flags & effFlagsHasEditor));
		   }
		   // do stuff
	   }
	   return 0;
   }

   LRESULT OnButtonConfig(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
   {
	   if(effect && effect->flags & effFlagsHasEditor)
	   {
		   MyDLGTEMPLATE t;	
		   t.style = WS_POPUPWINDOW|WS_DLGFRAME|DS_MODALFRAME|DS_CENTER;
		   DialogBoxIndirectParam (GetModuleHandle (0), &t, 0, (DLGPROC)EditorProc, (LPARAM)effect);
	   }
	   return 0;
   }

   void free_vst()
   {
	   if (effect)
	   {
		   effect->dispatcher( effect, effClose, 0, 0, 0, 0 );
		   effect = NULL;
		   FreeLibrary( hDll );
		   hDll = NULL;
	   }
   }

   BOOL load_vst(TCHAR * szPluginPath)
   {
	   free_vst();
	   hDll = LoadLibrary( szPluginPath );
	   if ( hDll ) {
		   PluginEntryProc pMain = (PluginEntryProc) GetProcAddress( hDll, "main" );
		   if ( pMain ) {
			   effect = (*pMain)(&audioMaster);
			   if ( effect ) {
				   effect->dispatcher( effect, effOpen, 0, 0, 0, 0 );
				   if ( !effect->dispatcher( effect, effGetPlugCategory, 0, 0, 0, 0 ) == kPlugCategSynth &&
					   !effect->dispatcher( effect, effCanDo, 0, 0, "sendVstMidiEvent", 0 ) )
				   {
					   effect->dispatcher( effect, effClose, 0, 0, 0, 0 );
					   effect = NULL;
					   FreeLibrary( hDll );
					   hDll = NULL;
					   MessageBox(L"This is NOT a VSTi synth!");
					   vst_effect.SetWindowText(L"No VSTi loaded");
					   vst_vendor.SetWindowText(L"No VSTi loaded");
					   vst_product.SetWindowText(L"No VSTi loaded");
					   vst_info.SetWindowText(L"No VSTi loaded");
					   return FALSE;
				   }
				   else
				   {
					   char effectName[256] = {0};
					   char  vendorString[256] = {0};
					   char  productString[256] = {0};
					   string conv;  
					   effect->dispatcher (effect, effGetEffectName, 0, 0, effectName, 0);
					   conv = effectName;
					   wstring effect_str = utf16_from_ansi(conv);
					   vst_effect.SetWindowText(effect_str.c_str());
					   effect->dispatcher (effect, effGetVendorString, 0, 0, vendorString, 0);
					   conv = vendorString;
					   wstring vendor_str = utf16_from_ansi(conv);
					   vst_vendor.SetWindowText(vendor_str.c_str());
					   effect->dispatcher (effect, effGetProductString, 0, 0, productString, 0);
					   conv = productString;
					   wstring product_str = utf16_from_ansi(conv);
					   vst_product.SetWindowText(product_str.c_str());
					    return TRUE;
				   }

			   }
		   }
		
		}
	   
   }

	LRESULT OnInitDialogView1(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		hDll = NULL;
		effect = NULL;
		vst_load= GetDlgItem(IDC_VSTLOAD);
		vst_info = GetDlgItem(IDC_VSTLOADED);
		vst_configure = GetDlgItem(IDC_VSTCONFIG);
		vst_effect = GetDlgItem(IDC_EFFECT);
		vst_vendor = GetDlgItem(IDC_VENDOR);
		vst_product = GetDlgItem(IDC_PRODUCT);
		vst_effect.SetWindowText(L"No VSTi loaded");
		vst_vendor.SetWindowText(L"No VSTi loaded");
		vst_product.SetWindowText(L"No VSTi loaded");
		vst_info.SetWindowText(L"No VSTi loaded");
		load_settings();
		return TRUE;
	}
};




class CView2 : public CDialogImpl<CView2>
{
	CComboBox synthlist;
	CButton apply;


public:
   enum { IDD = IDD_ADVANCED };
   BEGIN_MSG_MAP(CView1)
	   MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialogView2)
	    COMMAND_ID_HANDLER(IDC_SNAPPLY,OnButtonApply)
   END_MSG_MAP()

   LRESULT OnInitDialogView2(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
	   synthlist = GetDlgItem(IDC_SYNTHLIST);
	   apply = GetDlgItem(IDC_SNAPPLY);
	   load_midisynths();
	   return TRUE;
   }

   LRESULT OnButtonApply( WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
   {
	   set_midisynth();
	   return 0;

   }


   void set_midisynth()
   {
	   CRegKeyEx reg;
	   CRegKeyEx subkey;
	   CString device_name;
	   long lRet;
	   int selection = synthlist.GetCurSel();
	   int n = synthlist.GetLBTextLen(selection);
	   synthlist.GetLBText(selection,device_name.GetBuffer(n));
	   lRet = reg.Create(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Multimedia");
	   lRet = reg.DeleteSubKey(L"MIDIMap");
	   lRet = subkey.Create(reg,L"MIDIMap");
	   lRet = subkey.SetStringValue(L"szPname",device_name);
	   if (lRet == ERROR_SUCCESS)
	   {
		   MessageBox(L"MIDI synth set!",L"Notice.",MB_ICONINFORMATION);
	   }
	   else
	   {
		   MessageBox(L"Can't set MIDI registry key",L"Damn!",MB_ICONSTOP);
	   }
	   device_name.ReleaseBuffer(n);
	   subkey.Close();
	   reg.Close();
   }

	void load_midisynths()
	{
		LONG lResult;
		CRegKeyEx reg;
		CString device_name;
		ULONG size;
		lResult = reg.Create(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Multimedia\\MIDIMap");
		if (lResult == ERROR_SUCCESS){
			lResult = reg.QueryStringValue(L"szPname",NULL,&size);
			if (lResult == ERROR_SUCCESS) {
				reg.QueryStringValue(L"plugin",device_name.GetBuffer(size),&size);
		}
		}
		reg.Close();
		int device_count = midiOutGetNumDevs();
		for (int i = 0; i < device_count; ++i) {
			MIDIOUTCAPS Caps;
			ZeroMemory(&Caps, sizeof(Caps));
			MMRESULT Error = midiOutGetDevCaps(i, &Caps, sizeof(Caps));
			if (Error != MMSYSERR_NOERROR)
				continue;
			synthlist.AddString(Caps.szPname);
		}
		int index = 0;
		index = synthlist.FindStringExact(-1,device_name);
		if (index == CB_ERR) index = 0;
		synthlist.SetCurSel(index);
		device_name.ReleaseBuffer(size);
	}
};



#endif // !defined(AFX_VIEWS_H__20020629_8D64_963C_A351_0080AD509054__INCLUDED_)

