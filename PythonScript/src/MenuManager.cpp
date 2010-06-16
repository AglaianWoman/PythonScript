#include "stdafx.h"

#include "MenuManager.h"
#include "Notepad_Plus_Msgs.h"
#include "WcharMbcsConverter.h"

using namespace std;

// Static instance
MenuManager* MenuManager::m_menuManager;

WNDPROC MenuManager::s_origWndProc;

int MenuManager::s_startCommandID;
int MenuManager::s_endCommandID;

MenuManager* MenuManager::create(HWND hNotepad, int aboutCommandID, int aboutCommandIndex, void(*runScript)(const char *))
{
	m_menuManager = new MenuManager(hNotepad, aboutCommandID, aboutCommandIndex, runScript);
	return m_menuManager;
}


MenuManager::~MenuManager()
{
	// Free the Scripts menu HMENUs
	for(map<string, HMENU>::iterator iter = m_submenus.begin(); iter != m_submenus.end(); ++iter)
	{
		DestroyMenu((*iter).second);
	}
}


MenuManager* MenuManager::getInstance()
{
	return m_menuManager;
}


MenuManager::MenuManager(HWND hNotepad, int aboutCommandID, int aboutCommandIndex, void(*runScript)(const char *))
	:
	m_hNotepad (hNotepad),
	m_aboutCommandID (aboutCommandID),
	m_aboutCommandIndex (aboutCommandIndex),
	m_runScript (runScript)
{
}



/* This code was shamefully robbed from NppExec */
HMENU MenuManager::getOurMenu()
{
	
	HMENU hPluginMenu = (HMENU)::SendMessage(m_hNotepad, NPPM_GETMENUHANDLE, 0, 0);
	HMENU hPythonMenu = NULL;
	int iMenuItems = GetMenuItemCount(hPluginMenu);
    for ( int i = 0; i < iMenuItems; i++ )
    {
        HMENU hSubMenu = ::GetSubMenu(hPluginMenu, i);
        // does our About menu command exist here?
        if ( ::GetMenuState(hSubMenu, m_aboutCommandID, MF_BYCOMMAND) != -1 )
        {
            // this is our "Python Script" sub-menu
            hPythonMenu = hSubMenu;
            break;
        }
    }

	return hPythonMenu;
}

bool MenuManager::populateScriptsMenu()
{
	HMENU pythonPluginMenu = getOurMenu();
	if (!pythonPluginMenu)
	{
		//g_console.writeText("Error: Unable to find Python Plugin Menu\n");
		return false;
	}
	else
	{
		HMENU hScriptsMenu = CreateMenu();
		//funcItem[g_aboutFuncIndex]._cmdID + 1000
		s_startCommandID = m_aboutCommandID + ADD_CMD_ID;

		InsertMenu(pythonPluginMenu, m_aboutCommandIndex - 1, MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(hScriptsMenu), _T("Scripts"));
		m_submenus.insert(pair<string, HMENU>("\\", hScriptsMenu));
		
		TCHAR pluginDir[MAX_PATH];
		TCHAR configDir[MAX_PATH];
		::SendMessage(m_hNotepad, NPPM_GETNPPDIRECTORY, MAX_PATH, reinterpret_cast<LPARAM>(pluginDir));
		::SendMessage(m_hNotepad, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(configDir));
		shared_ptr<char> path = WcharMbcsConverter::tchar2char(pluginDir);
		string machineScriptsPath(path.get());
		machineScriptsPath.append("\\plugins\\PythonScript\\scripts");
		
		path = WcharMbcsConverter::tchar2char(configDir);
		string userScriptsPath(path.get());
		userScriptsPath.append("\\PythonScript\\scripts");
		
		int nextID = findScripts(hScriptsMenu, machineScriptsPath.size(), s_startCommandID, machineScriptsPath);

		s_endCommandID = findScripts(hScriptsMenu, userScriptsPath.size(), nextID, userScriptsPath);

		subclassNotepadPlusPlus();
		
	}

	return true;
}

int MenuManager::findScripts(HMENU hBaseMenu, int basePathLength, int startID, string& path)
{
	WIN32_FIND_DATAA findData;
	string indexPath;
	string searchPath(path);
	searchPath.append("\\*");
	HANDLE hFound = FindFirstFileA(searchPath.c_str(), &findData);
	BOOL found = (hFound != INVALID_HANDLE_VALUE) ? TRUE : FALSE;
	int position = 0;
	

	while (found)
	{
		if (FILE_ATTRIBUTE_DIRECTORY == (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			&& strcmp(findData.cFileName, ".") 
			&& strcmp(findData.cFileName, ".."))
		{
			searchPath = path;
			searchPath.append("\\");
			searchPath.append(findData.cFileName);
			HMENU hSubmenu = CreateMenu();
			// Add the submenu handle and path to the map
			indexPath.assign(searchPath.substr(basePathLength));
			
			pair< map<string, HMENU>::iterator, bool> result = m_submenus.insert(pair<string, HMENU>(indexPath, hSubmenu));
			
			// If the path has already been added, use the original HMENU
			if (!result.second)
			{
				DestroyMenu(hSubmenu);
				hSubmenu = (*result.first).second;
			}

			int nextID = findScripts(hSubmenu, basePathLength, startID, searchPath);
			if (nextID > startID)
			{
				startID = nextID;
				// Insert the submenu if it's new
				if (result.second)
				{
					InsertMenuA(hBaseMenu, position, MF_BYCOMMAND | MF_POPUP, reinterpret_cast<UINT_PTR>(hSubmenu), findData.cFileName);
				}
				
			}
			else
			{
				DestroyMenu(hSubmenu);
			}
			++position;
		}

		found = FindNextFileA(hFound, &findData);
	}
	FindClose(hFound);


	searchPath = path;
	searchPath.append("\\*.py");
	hFound = FindFirstFileA(searchPath.c_str(), &findData);
	found = (hFound != INVALID_HANDLE_VALUE) ? TRUE : FALSE;
	
	
	while(found)
	{
		string fullFilename(path);
		fullFilename.append("\\");
		fullFilename.append(findData.cFileName);
		m_scriptCommands.insert(pair<int, string>(startID, fullFilename));
		
		
		string indexedName = fullFilename.substr(basePathLength);
		
		pair<set<string>::iterator, bool> indexResult = m_machineScriptNames.insert(indexedName);
		
		char displayName[MAX_PATH];
		strcpy_s(displayName, MAX_PATH, indexedName.c_str());
		char *filename = PathFindFileNameA(displayName);
		PathRemoveExtensionA(filename);
		
		// If script name is already in the index
		// then the script name is already shown, so append " (User)" to the display name
		// as the first one must be a machine script
		if (!indexResult.second)
		{
			string sFilename(filename);
			sFilename.append(" (User)");
			InsertMenuA(hBaseMenu, position, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED, startID, sFilename.c_str());
		}
		else
		{
			InsertMenuA(hBaseMenu, position, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED, startID, filename);
		}
		
		++position;
		++startID;

		found = FindNextFileA(hFound, &findData);
	}
	FindClose(hFound);

	return startID;

}

void MenuManager::menuCommand(int commandID)
{
	m_runScript(m_scriptCommands[commandID].c_str());
}




LRESULT CALLBACK notepadWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (WM_COMMAND == message)
	{
		if (LOWORD(wParam) >= MenuManager::s_startCommandID && LOWORD(wParam) < MenuManager::s_endCommandID && HIWORD(wParam) == 0)
		{
			MenuManager::getInstance()->menuCommand(LOWORD(wParam));
			return TRUE;
		}
	}
	
	return MenuManager::s_origWndProc(hWnd, message, wParam, lParam);
	
}

void MenuManager::subclassNotepadPlusPlus()
{
	s_origWndProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(m_hNotepad, GWLP_WNDPROC, (LONG_PTR)(notepadWndProc)));
}