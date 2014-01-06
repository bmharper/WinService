/*

Example of a windows service that uses the WinService.cpp/WinService.h helpers.

This .exe serves a dual purpose:

	1. It can install/remove services
	2. It can act as a service itself

This is not necessarily the model you would always use, but there's nothing wrong with doing it this way.

*/

#include <string>
#include <windows.h>
#include "WinService.h"

void  WINAPI SvcMain( DWORD dwArgc, TCHAR** lpszArgv );
DWORD WINAPI SvcCtrlHandler( DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext );

const TCHAR* SvcName = TEXT("MyExampleService");
WinService_State Service;
int Ticker;

#ifdef UNICODE
	typedef std::wstring	nstring;
#else
	typedef std::string		nstring;
#endif

int main(int argc, char** argv)
{
	bool showhelp = false;

	TCHAR path[MAX_PATH];
	GetModuleFileName( NULL, path, MAX_PATH );
	nstring svcCmdLine = path;
	svcCmdLine += TEXT(" run");

	if ( argc >= 2 )
	{
		std::string msg;
		std::string p1 = argv[1];
		if ( p1 == "install" || p1 == "remove" )
		{
			WinService_Configure( p1 == "install", SvcName, svcCmdLine.c_str(), WinService_Startup_Demand, msg );
		}
		else if ( p1 == "run" )
		{
			// redirect stdout to this file, so that we can debug service startup issues
			//freopen( "c:\\temp\\example_service_debug.txt", "a", stdout );
			//setvbuf( stdout, NULL, _IONBF, 0 );

			Service.SvcMain = SvcMain;
			Service.SvcCtrlHandler = SvcCtrlHandler;
			printf( "Starting service dispatcher\n" );
			if ( !Service.Run() )
				printf( "svc.Run failed: %s\n", (const char*) Service.LastError.c_str() );
		}
		else 
			showhelp = true;
		fputs( msg.c_str(), stdout );
	}
	else 
		showhelp = true;

	if ( showhelp )
		printf( "WinServiceExample install|remove\n" );
}

void DoExampleServiceStuff()
{
	FILE* f = fopen( "c:\\temp\\example_service_output.txt", "a" );
	if ( f )
	{
		fprintf( f, "another line %d\n", Ticker++ );
		fclose( f );
	}
}

void WINAPI SvcMain( DWORD dwArgc, TCHAR** lpszArgv )
{
	printf( "Inside SvcMain\n" );

	if ( !Service.SvcMain_Start( dwArgc, lpszArgv ) )
	{
		printf( "Abort: %s\n", (const char*) Service.LastError.c_str() );
		return;
	}
	
	// Perform any non-trivial startup code here.
	// Periodically call
	//   Service.ReportSvcStatus( WinService_Status_Start_Pending, 0 );
	// to let the SCM know that you haven't gone into an infinite loop.

	printf( "Service is running\n" );

	Service.ReportSvcStatus( WinService_Status_Running, 0 );

	while ( true )
	{
		// Perform your real service work here. Don't block for too long (a few seconds) inside this section.
		// You should be performing light tasks here, such as creating jobs for a thread pool, or launching processes.
		DoExampleServiceStuff();

		if ( WaitForSingleObject( Service.SvcStopEvent, 3000 ) == WAIT_OBJECT_0 )
		{
			// Perform shutdown here...
			Service.ReportSvcStatus( WinService_Status_Stopped, 0 );
			break;
		}
	}

	printf( "Service is quitting\n" );

	Service.SvcMain_End();
}

DWORD WINAPI SvcCtrlHandler( DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext )
{
	switch( dwCtrl )
	{
	case SERVICE_CONTROL_STOP:
		// Tell Windows that it will take us 1000ms to shutdown
		Service.ReportSvcStatus( WinService_Status_Stop_Pending, 1000 );
		SetEvent( Service.SvcStopEvent );
		return NO_ERROR;

	case SERVICE_CONTROL_INTERROGATE:
		return NO_ERROR;
	}

	return ERROR_CALL_NOT_IMPLEMENTED;
}

