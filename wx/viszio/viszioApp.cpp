/*
  SZARP: SCADA software


  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
/* $Id: viszioApp.cpp 1 2009-11-17 21:44:30Z asmyk $
 *
 * viszio program
 * SZARP
 *
 * asmyk@praterm.com.pl
 */

#ifdef WX_PRECOMP
#include "wx_pch.h"
#endif

#ifdef __BORLANDC__
#pragma hdrstop
#endif //__BORLANDC__

#include "szapp.h"
#include "viszioApp.h"
#include "viszioFetchFrame.h"
#include "libpar.h"
#include <wx/tokenzr.h>
#ifndef MINGW32
#include <libwnck/libwnck.h>
#include <gdk/gdkx.h>
#endif
IMPLEMENT_APP(viszioApp);

bool viszioApp::OnCmdLineError(wxCmdLineParser &parser)
{
    return true;
}

bool viszioApp::OnCmdLineHelp(wxCmdLineParser &parser)
{
    parser.Usage();
    return false;
}

bool viszioApp::OnCmdLineParsed(wxCmdLineParser &parser)
{
    m_loadAll = false;
    m_deleteAll = false;
    m_loadOne = false;
    m_deleteOne = false;
    m_showAll = false;
    m_createNew = false;
    m_configuration = _T("");
    m_configurationToDelete = _T("");
    m_configurationToCreate = _T("");

   if (parser.Found(_T("H")))
    {
    	printf("To start work with viszio, you have to pass through several following steps:\n");
		printf("\t 1. Creating of a new configuration [-c option].\n");
		printf("\t 2. Loading of a specified configuration [-l option].\n");
		//printf("\t 2. Loading of all available configurations [-L option] (works only on the Linux system).\n");
		printf("\t 3. If a new configuration is loaded, you will have to define a correct server name and \n\t    you have to choose parameter to display.\n");
		printf("\t 4. If some old configuration is loaded, all information will be read from configuration file.\n");
		printf("\t 5. All information concerning each displayed parameter will be stored in configuration file.\n");
		fflush(stdout);
        exit(0);
    }
    if (parser.Found(_T("S")))
    {
        m_showAll = true;
        return true;
    }
    if (parser.Found(_T("D")))
    {
        m_deleteAll = true;
        return true;
    }
#ifndef MINGW32
    if (parser.Found(_T("L")))
    {
        m_loadAll = true;
        return true;
    }
#endif
    if (parser.Found(_T("l"), &m_configuration))
    {
        m_loadOne = true;
        return true;
    }
    if (parser.Found(_T("d"), &m_configurationToDelete))
    {
        m_deleteOne = true;
        return true;
    }
    if (parser.Found(_T("c"), &m_configurationToCreate))
    {
        m_createNew = true;
        return true;
    }

    return true;
}

void viszioApp::OnInitCmdLine(wxCmdLineParser &parser)
{
    //szApp::OnInitCmdLine(parser);
    parser.SetLogo(_T("Szarp viszio v 1.1"));
    parser.AddOption(_T("c"), _("create"), _("new configuration 'str' will be created"));
    parser.AddOption(_T("l"), _("load"), _("configuration 'str' will be loaded"));
    parser.AddOption(_T("d"), _("delete"), _("configuration 'str' will be deleted"));
	parser.AddSwitch(_T("h"), _("help"), _("show help"), wxCMD_LINE_OPTION_HELP);
#ifndef MINGW32    
    parser.AddSwitch(_T("L"), _("Load"), _("load all configurations"), wxCMD_LINE_SWITCH);
#endif    
    parser.AddSwitch(_T("D"), _("Delete"), _("delete all configurations"), wxCMD_LINE_SWITCH);
	parser.AddSwitch(_T("S"), _("Show"), _("show all configurations"), wxCMD_LINE_SWITCH);    
	parser.AddSwitch(_T("H"), _("usage"), _("show how to use viszio"), wxCMD_LINE_SWITCH);
}


bool viszioApp::DeleteAllConfigurations()
{
	wxConfig *global_config = new wxConfig(_T("viszio"));
	wxString configurations;
	global_config->Read(_T("Configurations"), &configurations);
    if (configurations.IsEmpty() == false)
    {
        wxStringTokenizer st(configurations);
        wxString tmp; 
        while (st.HasMoreTokens())
        {
            tmp = st.GetNextToken();
			wxConfig *local_config = new wxConfig(_T("viszio_") + tmp);
			local_config->DeleteAll();
			local_config ->Flush();
			delete local_config ;
        }
    }		
    global_config->DeleteAll();
    global_config->Flush();
    delete global_config;
    return true;
}

bool viszioApp::DeleteConfiguration(wxString configurationName)
{
    wxConfig *local_config =  new wxConfig(_T("viszio_") + configurationName);
    local_config->DeleteAll();
    local_config->Flush();
	wxConfig *global_config = new wxConfig(_T("viszio"));
    
    wxString configurations = _T("");
    global_config->Read(_T("Configurations"), &configurations);
    if (configurations.IsEmpty() == false)
    {
        wxStringTokenizer st(configurations);
        wxString tmp;
        wxString newConfigurations(_T(""));

        while (st.HasMoreTokens())
        {
            tmp = st.GetNextToken();
            if (tmp.IsSameAs(configurationName) == false)
            {
                newConfigurations += tmp;
                newConfigurations += _T(" ");
            }
        }
        global_config->Write(_T("Configurations"), newConfigurations);
    }
    global_config->Flush();

    delete local_config;
    delete global_config;
    return true;
}

#ifndef MINGW32    
bool viszioApp::LoadAllConfiguration()
{
	//TODO: correct wxConfig with running many configurations
    wxString configurations = _T("");
    wxConfig::Get()->Read(_T("Configurations"), &configurations);
    if (configurations.IsEmpty() == false)
    {
        wxStringTokenizer st(configurations);
        wxString tmp;
        char str[1000];
        while (st.HasMoreTokens())
        {
            tmp = st.GetNextToken();
            int pid = fork();
            if (pid == 0)
            {
                strcpy(str, (const char*)tmp.mb_str(wxConvUTF8));
                execlp("./viszio", "./viszio", "-l", str, (char*)0);
                fprintf(stderr, "Cannot load configuration: %s\n",str);
                fflush(stdout);
            }
        }
    }
    wxConfig::Get()->Flush();
    return true;
}
#endif
	
	
void viszioApp::ShowConfigurations()
{
    char str[1024];
    wxConfig *global_config = new wxConfig(_T("viszio"));
    
    wxString configurations = _T("");
    global_config->Read(_T("Configurations"), &configurations);

    if (configurations.IsEmpty())
    {
        printf("Configurations: not available\n");
        return;
    }
    strcpy(str, (const char*)configurations.mb_str(wxConvUTF8));
    printf("Configurations: %s\n", str);

    wxString SerwerParam = _T("ServerString");
    wxString SerwerName;

    wxStringTokenizer st(configurations);
    
    while (st.HasMoreTokens())
    {
    	wxString configuration = st.GetNextToken();
    	wxConfig *local_config = new wxConfig(_T("viszio_") + configuration);
    	strcpy(str, (const char*)configuration.mb_str(wxConvUTF8));
        printf("Configuracja: %s\n", str);
        
        //wxConfig::Get()->SetPath(_T("/") + config);
        local_config->Read(SerwerParam, &SerwerName);
        strcpy(str, (const char*)SerwerName.mb_str(wxConvUTF8));
        printf("  Server name: %s\n", str);
        local_config->SetPath(_T("/Parameters"));
        size_t NumberOfParams = local_config->GetNumberOfEntries();
        printf("  Number of parameters: %d\n", NumberOfParams);

        wxString name = _T("");
        wxString value = _T("");
        long dummy = 0;
        
        bool cont = local_config->GetFirstEntry(name, dummy);
        while (cont)
        {
            strcpy(str, (const char*)name.mb_str(wxConvUTF8));
            printf("     Parameter name: %s ", str);
            local_config->Read(name, &value);
            strcpy(str, (const char*)value.mb_str(wxConvUTF8));
            printf("[%s]\n", str);
            cont = local_config->GetNextEntry(name, dummy);
        }
        delete local_config;
    }
    delete global_config;
}

bool viszioApp::CreateConfiguration(wxString configurationName)
{
    wxConfig *global_config = new wxConfig(_T("viszio"));
	
    wxString configurations = _T("");
    global_config->Read(_T("Configurations"), &configurations);
    if(configurations.IsEmpty())
		global_config->Write(_T("Configurations"), configurationName);
	else
	{
		wxStringTokenizer st(configurations);
        wxString tmp;
        	
        while (st.HasMoreTokens())
        {
            tmp = st.GetNextToken();
            if (tmp.IsSameAs(configurationName) == true)
            {
                delete global_config;
                return false;
            }
        }
		configurations += _T(" ") + configurationName;
		global_config->Write(_T("Configurations"), configurations);
	}	
	TransparentFrame::config = new Configuration(configurationName);	    
    wxString SerwerParam = _T("/ServerString");
    TransparentFrame::config->wxconfig->Write(SerwerParam, _T("localhost:8080"));
    global_config->Flush();
    TransparentFrame::config->wxconfig->Flush();    
    delete global_config;
    return true;
}

bool viszioApp::LoadConfiguration(wxString configurationName)
{
    bool ask_for_server = false;    
    TransparentFrame::config = new Configuration(configurationName);
    
    TransparentFrame::config->wxconfig->Read(_T("FontThreshold"), &TransparentFrame::config->m_fontThreshold); 
    wxString SerwerParam = _T("/ServerString");
    wxString SerwerName;
    TransparentFrame::config->wxconfig->Read(SerwerParam, &SerwerName);
	
	if (SerwerName.IsEmpty())
		exit(1);
    
    m_http = new szHTTPCurlClient();

	if(TransparentFrame::config->m_pfetcher != NULL && TransparentFrame::config->m_pfetcher->IsRunning())
		TransparentFrame::config->m_pfetcher->Pause();

    while (TransparentFrame::config->m_ipk == NULL)
    {
        SerwerName = szServerDlg::GetServer(SerwerName, _T("Viszio"), ask_for_server, false);
        if (SerwerName.IsEmpty() or SerwerName.IsSameAs(_T("Cancel")))
            exit(11);

        TransparentFrame::config->m_ipk = szServerDlg::GetIPK(SerwerName, m_http);
        if (TransparentFrame::config->m_ipk == NULL)
            ask_for_server = true;
    }

    TransparentFrame::config->wxconfig->Write(SerwerParam, SerwerName);
    TransparentFrame::config->wxconfig->SetPath(_T("/Parameters/"));
    TransparentFrame::config->wxconfig->Flush();
    size_t NumberOfParams = TransparentFrame::config->wxconfig->GetNumberOfEntries();
    if (NumberOfParams<=0)
    {
        szViszioAddParam* apd = new szViszioAddParam(TransparentFrame::config->m_ipk, NULL, wxID_ANY, _("Viszio->AddParam"));
        if ( apd->ShowModal() != wxID_OK )
            exit(0);
        FetchFrame* frame = new FetchFrame(0L, SerwerName, m_http, apd->g_data.m_probe.m_parname);        
		frame->SetFrameConfiguration(apd->g_data.m_probe.m_parname, true, 0, 0, *wxRED, *wxBLACK, 15, 1, 1);
		if (TransparentFrame::config->m_pfetcher->IsRunning())    
			TransparentFrame::config->m_pfetcher->Resume();
		else
			TransparentFrame::config->m_pfetcher->Run();        
#ifndef MINGW32		
		gtk_widget_show_now(GTK_WIDGET(frame->GetHandle()));
		frame->MoveToDesktop();		
#else
		frame->Show(true);
#endif
        return true;
    }
    wxString tmp;

#define ReadLong(what, name) \
		if(st.HasMoreTokens()) {\
			tmp = st.GetNextToken();\
			if(tmp.IsNumber())\
				tmp.ToLong(&what);\
			else\
			{\
				wxString message(_T("Configuration error: "));\
				message += name;\
				message += _T(" is not a number. Check your configuration.");\
				wxLog::OnLog(1, message, 0);\
				wxLogError(message);\
				exit(11);\
			}\
		}\
		else\
		{\
			wxString message(_T("Configuration error: "));\
			message += name;\
			message += _T(" does not exist.");\
			wxLog::OnLog(1, message, 0);\
			wxLogError(message);\
			exit(12);\
		}

    wxString name = _T("");
    wxString value = _T("");
    long dummy = 0;
    long locationX = -1;
    long locationY = -1;
    long frameColorR = -1;
    long frameColorG = -1;
    long frameColorB = -1;
    long fontSize = -1;
    long fontSizeAdjust = -1;
    long fontColorR = -1;
    long fontColorG = -1;
    long fontColorB = -1;
    long withFrame = 0;
    long desktopNumber = 1;
    int i = 0;
    bool cont = TransparentFrame::config->wxconfig->GetFirstEntry(name, dummy);

    TransparentFrame *frame;
    FetchFrame *start_frame;
    while (cont)
    {        
        if (name.StartsWith(_T("placeholder")))
        {
            TransparentFrame::config->wxconfig->DeleteEntry(name);
            TransparentFrame::config->wxconfig->Flush();
            cont = TransparentFrame::config->wxconfig->GetNextEntry(name, dummy);
            continue;
        }
        TransparentFrame::config->wxconfig->Read(name, &value);
        wxStringTokenizer st(value);
        ReadLong(withFrame, name + _T(" with frame [1]"))
        ReadLong(locationX, name + _T(" x location [2]"))
        ReadLong(locationY, name + _T(" y location [3]"))
        ReadLong(frameColorR, name + _T(" frame color R [4]"))
        ReadLong(frameColorG, name + _T(" frame color G [5]"))
        ReadLong(frameColorB, name + _T(" frame color B [6]"))
        ReadLong(fontColorR, name + _T(" font color R [7]"))
        ReadLong(fontColorG, name + _T(" font color G [8]"))
        ReadLong(fontColorB, name + _T(" font color B [9]"))
        ReadLong(fontSize, name + _T(" font size [10]"))
        ReadLong(fontSizeAdjust, name + _T(" font adjust [11]"))
        ReadLong(desktopNumber, name + _T(" desktopNumber [12]"))
        wxColour frameColor(frameColorR, frameColorG, frameColorB);
        wxColour fontColor(fontColorR, fontColorG, fontColorB);
        if (i == 0)
        {
            start_frame = new FetchFrame(0L, SerwerName, m_http, name);
            start_frame->SetFrameConfiguration(name, withFrame, locationX, locationY, frameColor, fontColor, fontSize, fontSizeAdjust, desktopNumber);
            //start_frame->Show();
        }
        else
        {
            if (TransparentFrame::config->m_current_amount_of_frames == TransparentFrame::config->m_max_number_of_frames) return false;
            frame = new TransparentFrame(TransparentFrame::config->m_all_frames[0], name);
            frame->SetFrameConfiguration(name, withFrame, locationX, locationY, frameColor, fontColor, fontSize, fontSizeAdjust, desktopNumber);
           // frame->Show();
        }
        i++;
        cont = TransparentFrame::config->wxconfig->GetNextEntry(name, dummy);
    }
	if (TransparentFrame::config->m_pfetcher->IsRunning())    
		TransparentFrame::config->m_pfetcher->Resume();
	else
		TransparentFrame::config->m_pfetcher->Run();
	
	for(int j = 0; j < TransparentFrame::config->m_max_number_of_frames; j++)
		if(TransparentFrame::config->m_all_frames[j] != NULL) {			
#ifndef MINGW32
			gtk_widget_show_now(GTK_WIDGET(TransparentFrame::config->m_all_frames[j]->GetHandle()));
			TransparentFrame::config->m_all_frames[j]->MoveToDesktop();
#else
			TransparentFrame::config->m_all_frames[j]->Show();
#endif
		}

    TransparentFrame::config->wxconfig->SetPath(_T("/"));
    return true;
}




bool viszioApp::OnInit()
{
    if (szApp::OnInit() == false)
        return false;

#if wxUSE_UNICODE
    libpar_read_cmdline_w(&argc, argv);
#else //wxUSE_UNICODE
    libpar_read_cmdline(&argc, argv);
#endif //wxUSE_UNICODE

/*	if (m_configuration.IsEmpty() == false)
		m_configuration = _T("_") + m_configuration;
	*/	
    //this->SetProgName(_T("viszio") + m_configuration);
    //this->SetAppName(_T("viszio") + m_configuration);
    this->SetProgName(_T("viszio"));
    this->SetAppName(_T("viszio"));
    
    wxLog *logger=new wxLogStderr();
    wxLog::SetActiveTarget(logger);

#ifndef __WXMSW__
    locale.Init();
#else //__WXMSW__
    locale.Init(wxLANGUAGE_POLISH);
#endif //__WXMSW__

#ifndef __WXMSW__
    locale.AddCatalogLookupPathPrefix(wxGetApp().GetSzarpDir() + _T("resources/locales"));
#else //__WXMSW__
    locale.AddCatalogLookupPathPrefix(wxGetApp().GetSzarpDir() + _T("."));
#endif //__WXMSW__

    wxArrayString catalogs;
    catalogs.Add(_T("viszio"));
    catalogs.Add(_T("common"));
    catalogs.Add(_T("wx"));
    this->InitializeLocale(catalogs, locale);

    if (!locale.IsOk())
    {
        wxLogError(_T("Could not initialize locale."));
        return false;
    }

    if (m_showAll)
    {
        ShowConfigurations();
        exit(0);
    }

    if (m_deleteAll)
    {
        DeleteAllConfigurations();
        exit(0);
    }

    if (m_deleteOne)
    {
        DeleteConfiguration(m_configurationToDelete);
        exit(0);
    }

    if (m_loadOne)
    {
        xmlInitParser();
        xmlSubstituteEntitiesDefault(1);
        szHTTPCurlClient::Init();
        LoadConfiguration(m_configuration);
        return true;
    }
    
#ifndef MINGW32        
    if (m_loadAll)
    {
        LoadAllConfiguration();
        exit(0);
    }
#endif
    
    if (m_createNew)
    {
        CreateConfiguration(m_configurationToCreate);
        exit(0);
    }

    exit(0);
}

int viszioApp::OnExit()
{
	szHTTPCurlClient::Cleanup();
	szParList::Cleanup();
	xmlCleanupParser();
	wxConfigBase* conf = wxConfig::Get();
	if (conf) delete conf;
	return 0;
}
