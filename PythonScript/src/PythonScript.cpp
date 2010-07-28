// Plugin Template
//

#include "stdafx.h"
#include "PluginInterface.h"
#include "PythonScript.h"
#include "AboutDialog.h"
#include "ConsoleDialog.h"
#include "MenuManager.h"

#include "WcharMbcsConverter.h"
#include "PythonHandler.h"
#include "PythonConsole.h"
#include "NotepadPlusWrapper.h"
#include "ShortcutDlg.h"
#include "Python.h"
#include "ConfigFile.h"
#include <boost/python.hpp>
#include <Commdlg.h>

using namespace boost::python;

using namespace std;

#define CHECK_INITIALISED()  g_initialised ? 0 : initialisePython()


/* Info for Notepad++ */
CONST TCHAR PLUGIN_NAME[]	= _T("Python Script");

FuncItem	*funcItem = NULL;

/* Global data */
NppData				nppData;
HANDLE				g_hModule			= NULL;
TCHAR				iniFilePath[MAX_PATH];

/* Dialogs */
AboutDialog		aboutDlg;
ShortcutDlg     *g_shortcutDlg = NULL;

PythonConsole   *g_console = 0;
// Paths
char g_pluginDir[MAX_PATH];
char g_configDir[MAX_PATH];

bool g_infoSet = false;
int g_scriptsMenuIndex = 0;
int g_stopScriptIndex = 0;
bool g_initialised = false;

MenuManager *g_menuManager;

// Scripts on the menu
vector<string*> g_menuScripts;

// Scripts on the toolbar
vector< pair<string*, HICON>* > g_toolbarScripts;



void doAbout();

void newScript();
void showConsole();
void showShortcutDlg();
void stopScript();
void runScript(int);
void runScript(const char*);
void shutdown(void *);
void clearConsole();
FuncItem* getGeneratedFuncItemArray(int *nbF);



// Run script functions




HWND getCurrentHScintilla(int which);

// Main python handler/wrapper
PythonHandler *pythonHandler;

BOOL APIENTRY DllMain( HMODULE hModule,
					   DWORD  ul_reason_for_call,
					   LPVOID /* lpReserved */
					 )
{
	g_hModule = hModule;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		  break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}




extern "C" __declspec(dllexport) void setInfo(NppData notepadPlusData)
{
	nppData = notepadPlusData;
	
	// Get the two key directories (plugins config and the Npp dir)
	TCHAR pluginConfig[MAX_PATH];
	::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(pluginConfig));
	strcpy_s(g_configDir, MAX_PATH, WcharMbcsConverter::tchar2char(pluginConfig).get());

	TCHAR pluginDir[MAX_PATH];
	::SendMessage(nppData._nppHandle, NPPM_GETNPPDIRECTORY, MAX_PATH, reinterpret_cast<LPARAM>(pluginDir));
	_tcscat_s(pluginDir, MAX_PATH, _T("\\plugins"));
	strcpy_s(g_pluginDir, MAX_PATH, WcharMbcsConverter::tchar2char(pluginDir).get());
	
#ifdef DEBUG_STARTUP
	MessageBox(NULL, _T("setInfo"), _T("Python Script"), 0);
#endif

	ConfigFile::create(pluginConfig, pluginDir, reinterpret_cast<HINSTANCE>(g_hModule));
	MenuManager::create(nppData._nppHandle, reinterpret_cast<HINSTANCE>(g_hModule), runScript);
	g_infoSet = true;
}

extern "C" __declspec(dllexport) CONST TCHAR * getName()
{
	return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{

	if (g_infoSet)
	{
#ifdef DEBUG_STARTUP
		MessageBox(NULL, _T("Python GetFuncsArray"), _T("Python Script"), 0);
#endif

		funcItem = getGeneratedFuncItemArray(nbF);
	}
	else
	{
		MessageBox(NULL, _T("A fatal error has occurred. Notepad++ has incorrectly called getFuncsArray() before setInfo().  No menu items will be available for PythonScript."), _T("Python Script"), 0);
		funcItem = (FuncItem*) malloc(sizeof(FuncItem));
		memset(funcItem, 0, sizeof(FuncItem));
		_tcscpy_s(funcItem[0]._itemName, 64, _T("About - Python Script Disabled"));
		funcItem[0]._pFunc = doAbout;
		*nbF = 1;
	}

	return funcItem;
}

HWND getCurrentHScintilla(int which)
{
	return (which == 0)?nppData._scintillaMainHandle:nppData._scintillaSecondHandle;
};


FuncItem* getGeneratedFuncItemArray(int *nbF)
{
	
	MenuManager* menuManager = MenuManager::getInstance();
	
	MenuManager::ItemVectorTD items;
	items.reserve(8);
	int stopScriptIndex;
	int dynamicStartIndex;
	int scriptsMenuIndex;

	items.push_back(pair<tstring, void(*)()>(_T("New Script"), newScript));
	items.push_back(pair<tstring, void(*)()>(_T("Show Console"), showConsole));
	
	items.push_back(pair<tstring, void(*)()>(_T("--"), reinterpret_cast<void(*)()>(NULL)));
	
	items.push_back(pair<tstring, void(*)()>(_T("Stop Script"), stopScript));
	stopScriptIndex = items.size() - 1;

	items.push_back(pair<tstring, void(*)()>(_T("--"), reinterpret_cast<void(*)()>(NULL)));
	

	items.push_back(pair<tstring, void(*)()>(_T("--"), reinterpret_cast<void(*)()>(NULL)));
	scriptsMenuIndex = items.size() - 1;

	items.push_back(pair<tstring, void(*)()>(_T("Clear Console"), clearConsole));
	// Add dynamic scripts right above "Clear Console" - a separator will automatically
	// be added to the end of the list, if there are items in the dynamic menu
	dynamicStartIndex = items.size() - 1;

	items.push_back(pair<tstring, void(*)()>(_T("--"), reinterpret_cast<void(*)()>(NULL)));
	items.push_back(pair<tstring, void(*)()>(_T("Configuration"), showShortcutDlg));
	
	items.push_back(pair<tstring, void(*)()>(_T("About"), doAbout));
	


	FuncItem* funcItems = menuManager->getFuncItemArray(nbF, items, runScript, dynamicStartIndex, scriptsMenuIndex, stopScriptIndex);
	

	return funcItems;

}
	

void initialise()
{
	g_console = new PythonConsole();

	pythonHandler = new PythonHandler(g_pluginDir, g_configDir, (HINSTANCE)g_hModule, nppData._nppHandle, nppData._scintillaMainHandle, nppData._scintillaSecondHandle, g_console);
	
	aboutDlg.init((HINSTANCE)g_hModule, nppData);
	
	g_shortcutDlg = new ShortcutDlg((HINSTANCE)g_hModule, nppData, _T("\\PythonScript\\scripts"));


	g_console->init((HINSTANCE)g_hModule, nppData);
	

	
	MenuManager* menuManager = MenuManager::getInstance();
	menuManager->populateScriptsMenu();
	menuManager->stopScriptEnabled(false);

	
}

void initialisePython()
{
	g_initialised = true;
	DWORD startTicks = GetTickCount();
	
	
	pythonHandler->initPython();
	
	g_console->initPython(pythonHandler);
	
	pythonHandler->runStartupScripts();

	
	DWORD endTicks = GetTickCount();
	g_console->message("Python ");
	g_console->message(Py_GetVersion());
	
	char result[200];
	
	sprintf_s(result, 200, "\nInitialisation took %ldms\nReady.\n", endTicks-startTicks);
	g_console->message(result);
	
}

void registerToolbarIcons()
{
#ifdef DEBUG_STARTUP
	MessageBox(NULL, _T("Register toolbar icons"), _T("Python Script"), 0); 
#endif
	MenuManager::getInstance()->configureToolbarIcons();
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
	/* This switch is split into two
	 * 1. Notifications that must be run BEFORE any registered Python callbacks, and
	 * 2. Notifications that must be run AFTER any registered Python callbacks
	 */

	switch(notifyCode->nmhdr.code)
	{
		case NPPN_READY:
			{
#ifdef DEBUG_STARTUP
				MessageBox(NULL, _T("NPPN_READY"), _T("Python Script"), 0);
#endif
				initialise();
				ConfigFile *config = ConfigFile::getInstance();
				if (config->getSetting(_T("STARTUP")) == _T("ATSTARTUP"))
				{
					initialisePython();
				}
			}
			break;

		case NPPN_FILESAVED:
			{
				TCHAR filename[MAX_PATH];
				::SendMessage(nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, notifyCode->nmhdr.idFrom, reinterpret_cast<LPARAM>(filename));
				ConfigFile *configFile = ConfigFile::getInstance();
				const tstring machineScripts = configFile->getMachineScriptsDir().c_str();
				const tstring userScripts = configFile->getUserScriptsDir().c_str();

				if (_tcsnicmp(filename, machineScripts.c_str(), machineScripts.size()) == 0
					|| _tcsnicmp(filename, userScripts.c_str(), userScripts.size()) == 0)
				{
					MenuManager::getInstance()->refreshScriptsMenu();
				}
			}
			break;

		case NPPN_TBMODIFICATION:
			registerToolbarIcons();
			break;

	}
	
	// Notify the scripts
	if (pythonHandler)
		pythonHandler->notify(notifyCode);

	// Post notifications handlers
	switch(notifyCode->nmhdr.code)
	{
		case NPPN_SHUTDOWN:
			{
				DWORD shutdownThreadID;
				CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)shutdown, NULL, NULL, &shutdownThreadID);
			}
			break;		
	}

	
}



extern "C" __declspec(dllexport) LRESULT messageProc(UINT /* Message */, WPARAM /* wParam */, LPARAM /* lParam */)
{
	return TRUE;
}


#ifdef _UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}
#endif


void doAbout()
{
	aboutDlg.doDialog();
}





void stopScript()
{
	if (pythonHandler)
	{
		pythonHandler->stopScript();
	}
}


void runScript(int number)
{
	runScript(ConfigFile::getInstance()->getMenuScript(number).c_str());
}


void runScript(const char *filename)
{
	BYTE keyState[256];
	::GetKeyboardState(keyState);

	// If either control held down, then edit the file
	if (MenuManager::s_menuItemClicked && ((keyState[VK_LCONTROL] & 0x80) || (keyState[VK_RCONTROL] & 0x80)))
	{
		NotepadPlusWrapper wrapper((HINSTANCE)g_hModule, nppData._nppHandle);
		if (!wrapper.activateFile(filename))
		{
			wrapper.open(filename);
		}
	}
	else
	{
		CHECK_INITIALISED();
		MenuManager::getInstance()->stopScriptEnabled(true);
		if (!pythonHandler->runScript(filename, false))
		{
			MessageBox(NULL, _T("Another script is currently running.  Running two scripts at the same time could produce unpredicable results, and is therefore disabled."), _T("Python Script"), 0);
		}
	}

	MenuManager::s_menuItemClicked = false;

}

void showShortcutDlg()
{
	if (g_shortcutDlg)
	{
		g_shortcutDlg->doDialog();
	}
}

void showConsole()
{
	if (g_console)
	{
		CHECK_INITIALISED();
		g_console->showDialog();
	}
}

void newScript()
{
	
	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(OPENFILENAMEA));

	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nppData._nppHandle;
	shared_ptr<char> userScriptsDir = WcharMbcsConverter::tchar2char(ConfigFile::getInstance()->getUserScriptsDir().c_str());
	ofn.lpstrInitialDir = userScriptsDir.get();
	//ofn.lpstrFileTitle = "Choose filename for new script";
	ofn.lpstrFile = new char[MAX_PATH];
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "py";
	ofn.lpstrFilter = "Python Source Files (*.py)\0*.py\0All Files (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;

	ofn.Flags = OFN_OVERWRITEPROMPT;
	

	if (GetSaveFileNameA(&ofn))
	{
		NotepadPlusWrapper wrapper((HINSTANCE)g_hModule, nppData._nppHandle);
		HANDLE hFile = CreateFileA(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		CloseHandle(hFile);
		wrapper.open(ofn.lpstrFile);
		wrapper.setLangType(L_PYTHON);
	}
	

	delete [] ofn.lpstrFile;


}



void shutdown(void* /* dummy */)
{
	if (pythonHandler)
	{
		delete pythonHandler;
		pythonHandler = NULL;
	}

	if (g_console)
	{
		delete g_console;
		g_console = NULL;
	}

	for(vector<string*>::iterator it = g_menuScripts.begin(); it != g_menuScripts.end(); ++it)
	{
		delete *it;
	}

	for(vector< pair<string*, HICON>* >::iterator it = g_toolbarScripts.begin(); it != g_toolbarScripts.end(); ++it)
	{
		// Delete the string
		delete (*(*it)).first;

		// Destroy the HICON
		DestroyIcon((*(*it)).second);

		// Delete the pair
		delete *it;
	}

	MenuManager::deleteInstance();
	
}

void clearConsole()
{
	if (g_console)
	{
		g_console->clear();
	}
} 