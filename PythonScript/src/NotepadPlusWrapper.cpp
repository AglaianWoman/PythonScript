#include "stdafx.h"

#include "Scintilla.h"
#include "WcharMbcsConverter.h"
#include "NotepadPlusWrapper.h"
#include "PromptDialog.h"
#include "MenuManager.h"
#include "PluginInterface.h"
#include "ScintillaWrapper.h"

bool NotepadPlusWrapper::s_inEvent;

NotepadPlusWrapper::NotepadPlusWrapper(HINSTANCE hInst, HWND nppHandle)
	: m_nppHandle(nppHandle),
      m_hInst(hInst),
	  m_notificationsEnabled(false)	  
{
	s_inEvent = false;
}
	
NotepadPlusWrapper::~NotepadPlusWrapper()
{
	try
	{
		callbackT::iterator iter = m_callbacks.begin();
		while (iter != m_callbacks.end())
		{
			Py_XDECREF(iter->second);
		}

		m_callbacks.clear();
		m_notificationsEnabled = false;
	}
	catch (...)
	{
		// I don't know what to do with that, but a destructor should never throw, so...
	}

	// To please Lint, let's NULL these handles and pointers
	m_nppHandle = NULL;
	m_hInst = NULL;
}

void NotepadPlusWrapper::notify(SCNotification *notifyCode)
{
	if (!m_notificationsEnabled)
		return;

	std::pair<callbackT::iterator, callbackT::iterator> callbackIter 
		= m_callbacks.equal_range(notifyCode->nmhdr.code);
	
	if (callbackIter.first != callbackIter.second)
	{
		// Create the parameters for the callback
		boost::python::dict params;
		params["code"] = notifyCode->nmhdr.code;
		switch(notifyCode->nmhdr.code)
		{
		case NPPN_FILEBEFORECLOSE:
		case NPPN_FILEOPENED:
		case NPPN_FILECLOSED:
		case NPPN_FILEBEFOREOPEN:
		case NPPN_FILEBEFORESAVE:
		case NPPN_FILESAVED:
		case NPPN_BUFFERACTIVATED:
		case NPPN_LANGCHANGED:
		case NPPN_WORDSTYLESUPDATED:
		case NPPN_FILELOADFAILED:
			params["bufferID"] = notifyCode->nmhdr.idFrom;
			break;

		case NPPN_FILEBEFORELOAD:
		case NPPN_READY:
		case NPPN_SHUTDOWN:
		case NPPN_TBMODIFICATION:  // I hope no-one ever has to use this!
			// No additional parameters
			break;

		case NPPN_READONLYCHANGED:
			params["bufferID"] = notifyCode->nmhdr.hwndFrom;
			params["readonly"] = (notifyCode->nmhdr.idFrom & 1) ? true : false;
			params["dirty"] = (notifyCode->nmhdr.idFrom & 2) ? true : false;
			break;
	

		case NPPN_SHORTCUTREMAPPED:
			{
				params["commandID"] = notifyCode->nmhdr.idFrom;
				ShortcutKey *key = reinterpret_cast<ShortcutKey*>(notifyCode->nmhdr.hwndFrom);

				params["key"] = key->_key;
				params["isCtrl"] = key->_isCtrl;
				params["isAlt"] = key->_isAlt;
				params["isShift"] = key->_isShift;
			}
			break;

		
		default:
			// Unknown notification, so just fill in the parameters as integers.
			params["idFrom"] = notifyCode->nmhdr.idFrom;
			params["hwndFrom"] = notifyCode->nmhdr.hwndFrom;
			break;
		}


		std::list<PyObject*> callbacks;
		
		while (callbackIter.first != callbackIter.second)
		{
			callbacks.push_back(callbackIter.first->second);		
			++callbackIter.first;
		}


		for (std::list<PyObject*>::iterator listIter = callbacks.begin(); listIter != callbacks.end(); ++listIter)
		{
			PyGILState_STATE state = PyGILState_Ensure();
			try
			{
				s_inEvent = true;
				boost::python::call<PyObject*>((*listIter), params);	
				s_inEvent = false;
			} 
			catch(...)
			{
				PyErr_Print();	
			}
			PyGILState_Release(state);
		}

	}
}


bool NotepadPlusWrapper::addCallback(PyObject* callback, boost::python::list events)
{
	if (PyCallable_Check(callback))
	{
		size_t eventCount = _len(events);
		for(idx_t i = 0; i < eventCount; i++)
		{
			m_callbacks.insert(std::pair<int, PyObject*>(boost::python::extract<int>(events[i]), callback));
			Py_INCREF(callback);
		}
		
		m_notificationsEnabled = true;

	    return true;
	}
	else
	{
		return false;
	}
}

void NotepadPlusWrapper::save()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SAVECURRENTFILE);
	Py_END_ALLOW_THREADS
}


void NotepadPlusWrapper::newDocument()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::newDocumentWithFilename(const char *filename)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
	std::shared_ptr<TCHAR> tFilename = WcharMbcsConverter::char2tchar(filename);
	callNotepad(NPPM_SAVECURRENTFILEAS, 0, reinterpret_cast<LPARAM>(tFilename.get()));
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::saveAs(const char *filename)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SAVECURRENTFILEAS, FALSE, reinterpret_cast<LPARAM>(WcharMbcsConverter::char2tchar(filename).get()));
	Py_END_ALLOW_THREADS
}
	
void NotepadPlusWrapper::saveAsCopy(const char *filename)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SAVECURRENTFILEAS, TRUE, reinterpret_cast<LPARAM>(WcharMbcsConverter::char2tchar(filename).get()));
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::open(const char *filename)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(WcharMbcsConverter::char2tchar(filename).get()));
	Py_END_ALLOW_THREADS
}

bool NotepadPlusWrapper::activateFile(const char *filename)
{
	bool retVal;
	Py_BEGIN_ALLOW_THREADS
	retVal = 0 != callNotepad(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(WcharMbcsConverter::char2tchar(filename).get()));
	Py_END_ALLOW_THREADS
	return retVal;
}

int NotepadPlusWrapper::getCurrentView()
{
	int currentView;
	callNotepad(NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&currentView));
	return currentView;
}

LangType NotepadPlusWrapper::getCurrentLangType()
{
	int lang;
	callNotepad(NPPM_GETCURRENTLANGTYPE, 0, reinterpret_cast<LPARAM>(&lang));
	return static_cast<LangType>(lang);
}


void NotepadPlusWrapper::setCurrentLangType(LangType lang)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SETCURRENTLANGTYPE, 0, static_cast<LPARAM>(lang));
	Py_END_ALLOW_THREADS
}

boost::python::list NotepadPlusWrapper::getFiles()
{
	idx_t count;
	int bufferID;

	boost::python::list files;

	for(int view = 0; view <= 1; view++)
	{
		count = (idx_t)callNotepad(NPPM_GETNBOPENFILES, 0, view);
	
		TCHAR **fileNames = (TCHAR **)new TCHAR*[count];
		for (idx_t i = 0 ; i < count ; i++)
		{
			fileNames[i] = new TCHAR[MAX_PATH];
		}

		if (callNotepad(view ? NPPM_GETOPENFILENAMESSECOND : NPPM_GETOPENFILENAMESPRIMARY, 
							reinterpret_cast<WPARAM>(fileNames), static_cast<LPARAM>(count)))
		{
			for(idx_t pos = 0; pos < count; pos++)
			{
				bufferID = callNotepad(NPPM_GETBUFFERIDFROMPOS, pos, view);
				if (bufferID)
				{
#ifdef UNICODE
					std::shared_ptr<char> mbFilename = WcharMbcsConverter::tchar2char(fileNames[pos]);
					files.append(boost::python::make_tuple(const_cast<const char*>(mbFilename.get()), bufferID, pos, view));
#else
					files.append(boost::python::make_tuple(const_cast<const char*>(fileNames[pos]), bufferID, pos, view));
#endif
				}
			}
		}

		for (idx_t i = 0 ; i < count ; i++)
		{
			delete [] fileNames[i];
		}

		delete [] fileNames;
	}

	return files;
}



boost::python::list NotepadPlusWrapper::getSessionFiles(const char *sessionFilename)
{
	boost::python::list result;
#ifdef UNICODE
	std::shared_ptr<TCHAR> converted = WcharMbcsConverter::char2tchar(sessionFilename);
	const TCHAR *convertedSessionFilename = converted.get();
#else
	const TCHAR *convertedSessionFilename = sessionFilename;
#endif
	idx_t count = (idx_t)callNotepad(NPPM_GETNBSESSIONFILES, 0, reinterpret_cast<LPARAM>(convertedSessionFilename));
	if (count > 0)
	{
		TCHAR **fileNames = (TCHAR **)new TCHAR*[count];
		for (idx_t pos = 0 ; pos < count ; pos++)
		{
			fileNames[pos] = new TCHAR[MAX_PATH];
		}

		if (callNotepad(NPPM_GETSESSIONFILES, reinterpret_cast<WPARAM>(fileNames), reinterpret_cast<LPARAM>(convertedSessionFilename)))
		{

			for(idx_t pos = 0; pos < count; pos++)
			{
#ifdef UNICODE
					std::shared_ptr<char> mbFilename = WcharMbcsConverter::tchar2char(fileNames[pos]);
					result.append(const_cast<const char*>(mbFilename.get()));
#else
					result.append(const_cast<const char*>(fileNames[pos]));
#endif
			}

		}

		for (idx_t i = 0 ; i < count ; i++)
		{
			delete [] fileNames[i];
		}

		delete [] fileNames;
	}

	return result;

}


void NotepadPlusWrapper::saveSession(const char *sessionFilename, boost::python::list files)
{
	
	sessionInfo si;
	std::shared_ptr<TCHAR> tsessionFilename = WcharMbcsConverter::char2tchar(sessionFilename);
	si.sessionFilePathName = tsessionFilename.get();
	
	size_t filesCount = _len(files);
	si.files = (TCHAR **)new TCHAR*[filesCount];

	// Vector to store temporary list of shared_ptr 
	std::vector< std::shared_ptr<TCHAR> > filesList(filesCount);

	for(idx_t pos = 0; pos < filesCount; pos++)
	{
		filesList[pos] = WcharMbcsConverter::char2tchar(static_cast<const char*>(boost::python::extract<const char *>(files[pos])));
		si.files[pos] = filesList[pos].get();
	}
	
	si.nbFile = (int)filesCount;

	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SAVESESSION, 0, reinterpret_cast<LPARAM>(&si));
	Py_END_ALLOW_THREADS

	for(idx_t pos = 0; pos < filesCount; pos++)
	{
		filesList[pos].reset();
	}
	
}


void NotepadPlusWrapper::saveCurrentSession(const char *filename)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SAVECURRENTSESSION, 0, reinterpret_cast<LPARAM>(WcharMbcsConverter::char2tchar(filename).get()));
	Py_END_ALLOW_THREADS
}

ScintillaWrapper NotepadPlusWrapper::createScintilla()
{
	LRESULT handle = callNotepad(NPPM_CREATESCINTILLAHANDLE, 0, NULL);
	
	// return copy
	return ScintillaWrapper((HWND)handle);
}

void NotepadPlusWrapper::destroyScintilla(ScintillaWrapper& buffer)
{
	callNotepad(NPPM_DESTROYSCINTILLAHANDLE, 0, reinterpret_cast<LPARAM>(buffer.getHandle()));
	buffer.invalidateHandle();
}

int NotepadPlusWrapper::getCurrentDocIndex(int view)
{
	return callNotepad(NPPM_GETCURRENTDOCINDEX, view);
}


void NotepadPlusWrapper::setStatusBar(StatusBarSection section, const char *text)
{
	Py_BEGIN_ALLOW_THREADS
#ifdef UNICODE
		std::shared_ptr<TCHAR> s = WcharMbcsConverter::char2tchar(text);
	callNotepad(NPPM_SETSTATUSBAR, static_cast<WPARAM>(section), reinterpret_cast<LPARAM>(s.get()));
#else
	callNotepad(NPPM_SETSTATUSBAR, static_cast<WPARAM>(section), reinterpret_cast<LPARAM>(text));
#endif
	Py_END_ALLOW_THREADS
}


long NotepadPlusWrapper::getPluginMenuHandle()
{
	return callNotepad(NPPM_GETMENUHANDLE, 0, 0);
}

void NotepadPlusWrapper::activateIndex(int view, int index)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_ACTIVATEDOC, view, index);
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::loadSession(boost::python::str filename)
{
	
#ifdef UNICODE
	std::shared_ptr<TCHAR> s = WcharMbcsConverter::char2tchar((const char*)boost::python::extract<const char*>(filename));
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_LOADSESSION, 0, reinterpret_cast<LPARAM>(s.get()));
#else
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_LOADSESSION, 0, reinterpret_cast<LPARAM>((const char*)boost::python::extract<const char*>(filename)));
#endif
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::activateFileString(boost::python::str filename)
{
	
	#ifdef UNICODE
	std::shared_ptr<TCHAR> s = WcharMbcsConverter::char2tchar((const char*)boost::python::extract<const char*>(filename));
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(s.get()));
#else
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>((const char*)boost::python::extract<const char*>(filename)));
#endif
	Py_END_ALLOW_THREADS
}


void NotepadPlusWrapper::reloadFile(boost::python::str filename, bool alert)
{
	Py_BEGIN_ALLOW_THREADS
#ifdef UNICODE
	callNotepad(NPPM_RELOADFILE, alert ? 1 : 0, reinterpret_cast<LPARAM>(static_cast<const TCHAR *>(WcharMbcsConverter::char2tchar(boost::python::extract<const char *>(filename)).get())));
#else
	callNotepad(NPPM_RELOADFILE, alert ? 1 : 0, reinterpret_cast<LPARAM>(static_cast<const char *>(boost::python::extract<const char *>(filename))));
#endif
	Py_END_ALLOW_THREADS
}


void NotepadPlusWrapper::saveAllFiles()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SAVEALLFILES);
	Py_END_ALLOW_THREADS
}

boost::python::str NotepadPlusWrapper::getPluginConfigDir()
{
	TCHAR temp[MAX_PATH];
	callNotepad(NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, reinterpret_cast<LPARAM>(temp));
	return boost::python::str(const_cast<const char *>(WcharMbcsConverter::tchar2char(temp).get()));
}

void NotepadPlusWrapper::menuCommand(int commandID)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_MENUCOMMAND, 0, commandID);
	Py_END_ALLOW_THREADS
}

boost::python::tuple NotepadPlusWrapper::getVersion()
{
	LPARAM nppVersion = callNotepad(NPPM_GETNPPVERSION);
	
	int majorVersion = HIWORD(nppVersion);
	int minorVersion = LOWORD(nppVersion);

	std::vector<int> version(4);
	
	version[0] = majorVersion;

	char tmp[10];
	_itoa_s(minorVersion, tmp, 10, 10);
	
	idx_t i;
	for(i = 0; tmp[i] && i < 4; i++)
	{
		version[i+1] = tmp[i] - 48;
	}

	//lint -e864 Expression involving variable 'version' possibly depends on order of evaluation
	// It's perfectly safe the way it's used here.
	switch(i)
	{
		case 3:
			return boost::python::make_tuple<int, int, int, int>(version[0], version[1], version[2], version[3]);

		case 2:
			return boost::python::make_tuple<int, int, int>(version[0], version[1], version[2]);

		case 1:
			return boost::python::make_tuple<int, int>(version[0], version[1]);
		
		case 0:
		default:
			return boost::python::make_tuple<int>(version[0]);
	}
	//lint +e864
}


void NotepadPlusWrapper::hideTabBar()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_HIDETABBAR, 0, TRUE);
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::showTabBar()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_HIDETABBAR, 0, FALSE);
	Py_END_ALLOW_THREADS
}

int NotepadPlusWrapper::getCurrentBufferID()
{
	return callNotepad(NPPM_GETCURRENTBUFFERID);
}

void NotepadPlusWrapper::reloadBuffer(int bufferID, bool withAlert)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_RELOADBUFFERID, bufferID, static_cast<LPARAM>(withAlert));
	Py_END_ALLOW_THREADS
}

LangType NotepadPlusWrapper::getLangType()
{
	return getBufferLangType(callNotepad(NPPM_GETCURRENTBUFFERID));
}

LangType NotepadPlusWrapper::getBufferLangType(int bufferID)
{
	return static_cast<LangType>(callNotepad(NPPM_GETBUFFERLANGTYPE, bufferID));
}

void NotepadPlusWrapper::setLangType(LangType language)
{
	Py_BEGIN_ALLOW_THREADS
	setBufferLangType(language, callNotepad(NPPM_GETCURRENTBUFFERID));
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::setBufferLangType(LangType language, int bufferID)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SETBUFFERLANGTYPE, bufferID, static_cast<LPARAM>(language));
	Py_END_ALLOW_THREADS
}

BufferEncoding NotepadPlusWrapper::getEncoding()
{
	return getBufferEncoding(callNotepad(NPPM_GETCURRENTBUFFERID));
}

BufferEncoding NotepadPlusWrapper::getBufferEncoding(int bufferID)
{
	return static_cast<BufferEncoding>(callNotepad(NPPM_GETBUFFERENCODING, bufferID));
}

void NotepadPlusWrapper::setEncoding(BufferEncoding encoding)
{
	Py_BEGIN_ALLOW_THREADS
	setBufferEncoding(encoding, callNotepad(NPPM_GETCURRENTBUFFERID));
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::setBufferEncoding(BufferEncoding encoding, int bufferID)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SETBUFFERENCODING, bufferID, static_cast<LPARAM>(encoding));
	Py_END_ALLOW_THREADS
}

FormatType NotepadPlusWrapper::getFormatType()
{
	return getBufferFormatType(callNotepad(NPPM_GETCURRENTBUFFERID));
}


FormatType NotepadPlusWrapper::getBufferFormatType(int bufferID)
{
	return static_cast<FormatType>(callNotepad(NPPM_GETBUFFERFORMAT, bufferID));
}

void NotepadPlusWrapper::setFormatType(FormatType format)
{
	Py_BEGIN_ALLOW_THREADS
	setBufferFormatType(format, callNotepad(NPPM_GETCURRENTBUFFERID));
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::setBufferFormatType(FormatType format, int bufferID)
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_SETBUFFERFORMAT, bufferID, static_cast<LPARAM>(format));
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::closeDocument()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::closeAllDocuments()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSEALL);
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::closeAllButCurrentDocument()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSEALL_BUT_CURRENT);
	Py_END_ALLOW_THREADS
}

void NotepadPlusWrapper::reloadCurrentDocument()
{
	Py_BEGIN_ALLOW_THREADS
	callNotepad(NPPM_MENUCOMMAND, 0, IDM_FILE_RELOAD);
	Py_END_ALLOW_THREADS
}


int NotepadPlusWrapper::messageBox(const char *message, const char *title, UINT flags)
{
	int retVal;
	Py_BEGIN_ALLOW_THREADS
	retVal = ::MessageBoxA(m_nppHandle, message, title, flags);
	Py_END_ALLOW_THREADS

	return retVal;
}



boost::python::object NotepadPlusWrapper::prompt(boost::python::object promptObj, boost::python::object title, boost::python::object initial)
{
	PromptDialog promptDlg(m_hInst, m_nppHandle);
	const char *cPrompt = NULL;
	const char *cTitle = NULL;
	const char *cInitial = NULL;
	if (!promptObj.is_none())
		cPrompt = (const char *)boost::python::extract<const char *>(promptObj.attr("__str__")());
	if (!title.is_none())
		cTitle= (const char *)boost::python::extract<const char *>(title.attr("__str__")());
	
	if (!initial.is_none())
		cInitial = (const char *)boost::python::extract<const char *>(initial.attr("__str__")());

	PromptDialog::PROMPT_RESULT result;
	Py_BEGIN_ALLOW_THREADS
	result = promptDlg.showPrompt(cPrompt, cTitle, cInitial);
	Py_END_ALLOW_THREADS

	if (PromptDialog::RESULT_OK == result)
	{
		return boost::python::str(promptDlg.getText());
	}
	else
	{
		return boost::python::object();
	}

}



void NotepadPlusWrapper::clearCallbackFunction(PyObject* callback)
{
	for(callbackT::iterator it = m_callbacks.begin(); it != m_callbacks.end();)
	{
		if (callback == it->second)
		{
			Py_DECREF(it->second);
			it = m_callbacks.erase(it);
		}
		else
		{
			++it;
		}
	}

	if (m_callbacks.empty())
	{
		m_notificationsEnabled = false;
	}
}
	
void NotepadPlusWrapper::clearCallbackEvents(boost::python::list events)
{
	for(callbackT::iterator it = m_callbacks.begin(); it != m_callbacks.end(); )
	{
		if(boost::python::extract<bool>(events.contains(it->first)))
		{
			Py_DECREF(it->second);
			it = m_callbacks.erase(it);
		}
		else
		{
			++it;
		}
	}

	if (m_callbacks.empty())
	{
		m_notificationsEnabled = false;
	}
}
	

void NotepadPlusWrapper::clearCallback(PyObject* callback, boost::python::list events)
{
	for(callbackT::iterator it = m_callbacks.begin(); it != m_callbacks.end(); )
	{
		if(it->second == callback && boost::python::extract<bool>(events.contains(it->first)))
		{
			Py_DECREF(it->second);
			it = m_callbacks.erase(it);
		}
		else 
		{
			++it;
		}
	}
	if (m_callbacks.empty())
	{
		m_notificationsEnabled = false;
	}
}

void NotepadPlusWrapper::clearAllCallbacks()
{
	for(callbackT::iterator it = m_callbacks.begin(); it != m_callbacks.end(); )
	{
		Py_DECREF(it->second);
		it = m_callbacks.erase(it);
	}
	
	
	if (m_callbacks.empty())
	{
		m_notificationsEnabled = false;
	}
}


void NotepadPlusWrapper::activateBufferID(int bufferID)
{
	Py_BEGIN_ALLOW_THREADS
	idx_t index = (idx_t)callNotepad(NPPM_GETPOSFROMBUFFERID, bufferID);
	UINT view = (index & 0xC0000000) >> 30;
	index = index & 0x3FFFFFFF;
	
	callNotepad(NPPM_ACTIVATEDOC, view, (LPARAM)index);
	Py_END_ALLOW_THREADS
}
boost::python::str NotepadPlusWrapper::getBufferFilename(int bufferID)
{ 
	TCHAR buffer[MAX_PATH];
	callNotepad(NPPM_GETFULLPATHFROMBUFFERID, bufferID, reinterpret_cast<LPARAM>(buffer));
	std::shared_ptr<char> filename = WcharMbcsConverter::tchar2char(buffer);
	return boost::python::str(const_cast<const char *>(filename.get()));
}

boost::python::str NotepadPlusWrapper::getCurrentFilename()
{
	int bufferID = callNotepad(NPPM_GETCURRENTBUFFERID);
	TCHAR buffer[MAX_PATH];
	callNotepad(NPPM_GETFULLPATHFROMBUFFERID, bufferID, reinterpret_cast<LPARAM>(buffer));
	std::shared_ptr<char> filename = WcharMbcsConverter::tchar2char(buffer);
	return boost::python::str(const_cast<const char *>(filename.get()));
}


bool NotepadPlusWrapper::runPluginCommand(boost::python::str pluginName, boost::python::str menuOption, bool refreshCache)
{
	bool retVal = false;

	MenuManager *menuManager = MenuManager::getInstance();
	if (!pluginName.is_none() && !menuOption.is_none())
	{
		std::shared_ptr<TCHAR> tpluginName = WcharMbcsConverter::char2tchar(boost::python::extract<const char *>(pluginName));
		std::shared_ptr<TCHAR> tmenuOption = WcharMbcsConverter::char2tchar(boost::python::extract<const char *>(menuOption));
		Py_BEGIN_ALLOW_THREADS
		idx_t commandID = menuManager->findPluginCommand(tpluginName.get(), tmenuOption.get(), refreshCache);
		if (commandID)
		{
			::SendMessage(m_nppHandle, WM_COMMAND, commandID, 0);
			retVal = true;
		}
		Py_END_ALLOW_THREADS
	}
	return retVal;

}

bool NotepadPlusWrapper::runMenuCommand(boost::python::str menuName, boost::python::str menuOption, bool refreshCache)
{
	bool retVal = false;
	MenuManager *menuManager = MenuManager::getInstance();
	if (!menuName.is_none() && !menuOption.is_none())
	{
		std::shared_ptr<TCHAR> tmenuName = WcharMbcsConverter::char2tchar(boost::python::extract<const char *>(menuName));
		std::shared_ptr<TCHAR> tmenuOption = WcharMbcsConverter::char2tchar(boost::python::extract<const char *>(menuOption));
		Py_BEGIN_ALLOW_THREADS
		idx_t commandID = menuManager->findMenuCommand(tmenuName.get(), tmenuOption.get(), refreshCache);
		if (commandID)
		{
			::SendMessage(m_nppHandle, WM_COMMAND, commandID, 0);
			retVal = true;
		}
		Py_END_ALLOW_THREADS
	}
	return retVal;

}


boost::python::str NotepadPlusWrapper::getNppDir()
{
	TCHAR buffer[MAX_PATH];
	::SendMessage(m_nppHandle, NPPM_GETNPPDIRECTORY, MAX_PATH, reinterpret_cast<LPARAM>(buffer));
	return boost::python::str(const_cast<const char *>(WcharMbcsConverter::tchar2char(buffer).get()));
}

boost::python::str NotepadPlusWrapper::getCommandLine()
{
	return boost::python::str(const_cast<const char *>(WcharMbcsConverter::tchar2char(::GetCommandLine()).get()));
}

bool NotepadPlusWrapper::allocateSupported()
{
	return 1 == ::SendMessage(m_nppHandle, NPPM_ALLOCATESUPPORTED, 0, 0);
}

boost::python::object NotepadPlusWrapper::allocateCmdID(int quantity)
{
	int startID;
	bool result = 1 == ::SendMessage(m_nppHandle, NPPM_ALLOCATECMDID, quantity, reinterpret_cast<LPARAM>(&startID));
	if (result)
	{
		return boost::python::object(startID);
	}
	else
	{
		return boost::python::object();
	}
}

boost::python::object NotepadPlusWrapper::allocateMarker(int quantity)
{
	int startID;
	bool result = 1 == ::SendMessage(m_nppHandle, NPPM_ALLOCATEMARKER, quantity, reinterpret_cast<LPARAM>(&startID));
	if (result)
	{
		return boost::python::object(startID);
	}
	else
	{
		return boost::python::object();
	}
}
