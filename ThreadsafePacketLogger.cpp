#include "ThreadsafePacketLogger.h"
#include <string.h>

ThreadsafePacketLogger::ThreadsafePacketLogger()
{

}
ThreadsafePacketLogger::~ThreadsafePacketLogger()
{
	char **msg;
	while ((msg = logMessages.ReadLock()) != 0)
	{
		rakFree((*msg));
	}
}
#ifdef _MSC_VER
#pragma warning( disable : 4100 ) // 'peer' : unreferenced formal parameter
#endif
void ThreadsafePacketLogger::Update(RakPeerInterface *peer)
{
	char **msg;
	while ((msg = logMessages.ReadLock()) != 0)
	{
		WriteLog(*msg);
		rakFree((*msg));
	}
}
void ThreadsafePacketLogger::AddToLog(const char *str)
{
	char **msg = logMessages.WriteLock();
	*msg = (char*) rakMalloc( strlen(str)+1 );
	strcpy(*msg, str);
	logMessages.WriteUnlock();
}
