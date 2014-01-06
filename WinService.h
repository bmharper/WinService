/*
	Helper functions for working as a Windows Service

	We only support SERVICE_WIN32_OWN_PROCESS. I simply haven't tried anything else.

	See the example WinServiceExample.cpp for how to use these helpers.

*/

#pragma once
#include <WinSvc.h>

#ifdef PAPI
#	define WINSERVICE_HELPER_API PAPI
#else
#	define WINSERVICE_HELPER_API
#endif

enum WinService_StartupType
{
	WinService_Startup_NoChange			= SERVICE_NO_CHANGE,
	WinService_Startup_Boot				= SERVICE_BOOT_START,
	WinService_Startup_System			= SERVICE_SYSTEM_START,
	WinService_Startup_Auto				= SERVICE_AUTO_START,
	WinService_Startup_Demand			= SERVICE_DEMAND_START,
	WinService_Startup_Disabled			= SERVICE_DISABLED,
};

enum WinService_Status
{
	WinService_Status_Stopped			= SERVICE_STOPPED,
	WinService_Status_Start_Pending		= SERVICE_START_PENDING,
	WinService_Status_Stop_Pending		= SERVICE_STOP_PENDING,
	WinService_Status_Running			= SERVICE_RUNNING,
	WinService_Status_Continue_Pending	= SERVICE_CONTINUE_PENDING,
	WinService_Status_Pause_Pending		= SERVICE_PAUSE_PENDING,
	WinService_Status_Paused			= SERVICE_PAUSED,
};

/* All of the state of your running service

	Instructions
	------------

	* Create an instance of WinService_State
	* Populate SvcMain
		* Inside your SvcMain, call SvcMain_Start() upon entry. Return immediately if SvcMain_Start() returns false.
		* Inside your SvcMain, call SvcMain_End() upon exit.
	* Populate SvcCtrlHandler
	* Run()

*/
class WINSERVICE_HELPER_API WinService_State
{
public:

#ifdef UNICODE
	typedef std::wstring	nstring;
#else
	typedef std::string		nstring;
#endif

	// Callback function that you must provide
	void (WINAPI *SvcMain)( DWORD dwArgc, TCHAR** lpszArgv );

	// Called by SCM whenever a control code is sent to the service using the ControlService function.
	// If you do not set this, then it defaults to DefaultSvcCtrlHandler.
	// lpContext points to the WinService_State object
	DWORD (WINAPI *SvcCtrlHandler)( DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext );

	nstring					SvcName;			// You can leave this blank for a SERVICE_WIN32_OWN_PROCESS
	SERVICE_STATUS_HANDLE   SvcStatusHandle;	// Populated by SvcMain_Start(). ReportSvcStatus uses this to inform the OS of the state of the service.
	HANDLE                  SvcStopEvent;		// Created by SvcMain_Start(). DefaultSvcCtrlHandler() will toggle this upon receipt of SERVICE_CONTROL_STOP.
	DWORD					Win32ExitCode;		// Sent every time we call ReportSvcStatus. Default = 0.
	std::string				LastError;			// If an error occurs inside any function in here, it gets written to LastError

			WinService_State();
			~WinService_State();
	bool	Run();																	// If this returns false, then the error is in LastError.
	bool	SvcMain_Start( DWORD dwArgc, TCHAR** lpszArgv );						// Call this at the start of your SvcMain. If it returns false, then return immediately from your SvcMain().
	void	SvcMain_End();															// Call this at the end of your SvcMain
	void	ReportSvcStatus( WinService_Status currentState, DWORD dwWaitHintMS );	// Use this to inform the OS of the status of your service

	// Default SvcCtrlHandler. The only logic this performs is when receiving SERVICE_CONTROL_STOP, it will toggle SvcStopEvent.
	static DWORD WINAPI DefaultSvcCtrlHandler( DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext );

protected:
	DWORD	StatusCheckPoint;
	void	ResetStatus();
};

/* Install or remove a windows service.

	install			true to install, false to remove
	svcName			Name of the service
	fullPath		Full path to execute
	startupType		Startup mode of the service
	outputMsg		Output message. Always contains something, whether the function succeeded or failed.
					You will typically write this message to stdout.

*/
bool WINSERVICE_HELPER_API WinService_Configure( bool install, LPCTSTR svcName, LPCTSTR fullPath, WinService_StartupType startupType, std::string& outputMsg );

