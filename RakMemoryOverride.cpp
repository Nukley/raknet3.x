#include "RakMemoryOverride.h"

using namespace RakNet;

void* (*rakMalloc) (size_t size) = RakMemoryOverride::RakMalloc;
void* (*rakRealloc) (void *p, size_t size) = RakMemoryOverride::RakRealloc;
void (*rakFree) (void *p) = RakMemoryOverride::RakFree;

#ifdef _USE_RAK_MEMORY_OVERRIDE
void* RakMemoryOverride::operator new (size_t size)
{
	return rakMalloc(size); 
}
void RakMemoryOverride::operator delete (void *p)
{
	return rakFree(p);
}
void* RakMemoryOverride::operator new[] (size_t size)
{ 
	return rakMalloc(size); 
}
void RakMemoryOverride::operator delete[] (void *p)
{
	return rakFree(p);
}
#endif

void* RakMemoryOverride::RakMalloc (size_t size)
{
	return malloc(size);
}

void* RakMemoryOverride::RakRealloc (void *p, size_t size)
{
	return realloc(p,size);
}

void RakMemoryOverride::RakFree (void *p)
{
	free(p);
}
