#include <windows.h>
#include <string>
//#include "pch.h"

#include "WinService.h"

#pragma comment(lib, "advapi32.lib")

static std::string SysErrMsg( DWORD err )
{ 
	char szBuf[1024];
	LPVOID lpMsgBuf;

	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR) &lpMsgBuf,
		0, NULL );

	sprintf( szBuf, "(%u) %s", err, (const char*) lpMsgBuf );
	LocalFree( lpMsgBuf );

	// chop off trailing carriage returns
	std::string r = szBuf;
	while ( r.length() > 0 && (r[r.length() - 1] == 10 || r[r.length() - 1] == 13) )
		r.resize( r.length() - 1 );

	return r;
}

static std::string SysLastErrMsg()
{
	return SysErrMsg( GetLastError() );
}

bool WINSERVICE_HELPER_API WinService_Configure( bool install, LPCTSTR svcName, LPCTSTR fullPath, WinService_StartupType startupType, std::string& outputMsg )
{
	SC_HANDLE svcman = NULL;
	SC_HANDLE svc = NULL;

	svcman = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	if ( !svcman )
	{
		outputMsg = "OpenSCManager failed " + SysLastErrMsg();
		return false;
	}

	bool ok = false;

	if ( install )
	{
		svc = OpenService( svcman, svcName, SERVICE_ALL_ACCESS  );
		if ( svc )
		{
			if ( TRUE == ChangeServiceConfig( svc, SERVICE_NO_CHANGE, startupType, SERVICE_NO_CHANGE, fullPath, NULL, NULL, NULL, NULL, NULL, NULL ) )
			{
				ok = true;
				outputMsg = "ChangeServiceConfig succeeded";
			}
			else
				outputMsg = "ChangeServiceConfig failed " + SysLastErrMsg(); 
		}
		else
		{
			svc = CreateService( 
					svcman,                    // SCM database 
					svcName,                   // name of service 
					svcName,                   // service name to display 
					SERVICE_ALL_ACCESS,        // desired access 
					SERVICE_WIN32_OWN_PROCESS, // service type 
					startupType,               // start type 
					SERVICE_ERROR_NORMAL,      // error control type 
					fullPath,                  // path to service's binary 
					NULL,                      // no load ordering group 
					NULL,                      // no tag identifier 
					NULL,                      // no dependencies 
					NULL,                      // LocalSystem account 
					NULL);                     // no password 
			if ( svc != NULL )
			{
				ok = true;
				outputMsg = "CreateService succeeded";
			}
			else
			{
				wprintf( L"Error: %s\n", fullPath );
				outputMsg = "CreateService failed " + SysLastErrMsg();
			}
		}
	}
	else // remove
	{
		svc = OpenService( svcman, svcName, DELETE );
		if ( svc == NULL )
		{
			outputMsg = "Unable to open service for deletion " + SysLastErrMsg();
		}
		else
		{
			if ( DeleteService( svc ) )
			{
				ok = true;
				outputMsg = "Service deleted";
			}
			else
				outputMsg = "Unable to delete service";
		}
	}

	if ( svc )
		CloseServiceHandle(svc); 
	CloseServiceHandle(svcman);
	return ok;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WinService_State::WinService_State()
{
	ResetStatus();
}

WinService_State::~WinService_State()
{
}

void WinService_State::ResetStatus()
{
	SvcStatusHandle = NULL;
	SvcStopEvent = NULL;
	StatusCheckPoint = 0;
	Win32ExitCode = 0;
}

bool WinService_State::Run()
{
	if ( SvcMain == NULL )
	{
		LastError = "SvcMain must be set";
		return false;
	}

	SERVICE_TABLE_ENTRY dispatchTable[] =
	{
			{ TEXT(""), (LPSERVICE_MAIN_FUNCTION) SvcMain },  // name is ignored for SERVICE_WIN32_OWN_PROCESS
			{ NULL, NULL }
	};

	if ( !StartServiceCtrlDispatcher( dispatchTable ) )
	{ 
		LastError = "StartServiceCtrlDispatcher failed with " + SysLastErrMsg();
		//SvcReportEvent(TEXT("StartServiceCtrlDispatcher")); 
		return false;
	}
	
	return true;
}

// The arguments here are NOT the regular command-line arguments.
// It is special arguments that the user can type in on the services control panel, or that another process can send us via StartService().
bool WinService_State::SvcMain_Start( DWORD dwArgc, TCHAR** lpszArgv )
{
	if ( dwArgc == 1 )
		SvcName = lpszArgv[0];

	SvcStopEvent = CreateEvent( NULL, true, false, NULL );
	if ( SvcStopEvent == NULL )
	{
		LastError = "Unable to create Service Stop Event: " + SysLastErrMsg();
		return false;
	}

	LPHANDLER_FUNCTION_EX ctrlHandler = SvcCtrlHandler ? SvcCtrlHandler : DefaultSvcCtrlHandler;
	SvcStatusHandle = RegisterServiceCtrlHandlerEx( SvcName.c_str(), ctrlHandler, this );		// name is ignored for SERVICE_WIN32_OWN_PROCESS
	if ( SvcStatusHandle == NULL )
	{
		LastError = "Unable to do RegisterServiceCtrlHandlerEx: " + SysLastErrMsg();
		SetEvent( SvcStopEvent );
		return false;
	}

	ReportSvcStatus( WinService_Status_Start_Pending, 1000 );

	return true;
}

void WinService_State::SvcMain_End()
{
	CloseHandle( SvcStopEvent );
	CloseHandle( SvcStatusHandle );
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWaitHintMS - Estimated time for pending operation, in milliseconds
// 
void WinService_State::ReportSvcStatus( WinService_Status currentState, DWORD dwWaitHintMS )
{
	SERVICE_STATUS status;
	memset( &status, 0, sizeof(status) );

	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = currentState;
	status.dwServiceSpecificExitCode = 0;
	status.dwWin32ExitCode = Win32ExitCode;
	status.dwWaitHint = dwWaitHintMS;

	if ( currentState == SERVICE_START_PENDING )
		status.dwControlsAccepted = 0;
	else
		status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ( currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED )
		StatusCheckPoint = 0;
	else
		StatusCheckPoint++;

	status.dwCheckPoint = StatusCheckPoint;

	// Report the status of the service to the SCM.
	SetServiceStatus( SvcStatusHandle, &status );
}

DWORD WINAPI WinService_State::DefaultSvcCtrlHandler( DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext )
{
	WinService_State* self = (WinService_State*) lpContext;
	switch( dwCtrl )
	{
	case SERVICE_CONTROL_STOP:
		self->ReportSvcStatus( WinService_Status_Stop_Pending, 0 );
		SetEvent( self->SvcStopEvent );
		return NO_ERROR;

	case SERVICE_CONTROL_INTERROGATE:
		return NO_ERROR;
	}

	return ERROR_CALL_NOT_IMPLEMENTED;
}
