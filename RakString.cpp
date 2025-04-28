#include "RakString.h"
#include "RakAssert.h"
#include "RakMemoryOverride.h"
#include "SimpleMutex.h"
#include <string.h>
#include <stdarg.h>

#if defined(__GNUC__)
#define _vsnprintf vsnprintf
#endif

#if (defined(__GNUC__)  || defined(__GCCXML__)) && !defined(_stricmp)
#define stricmp strcasecmp
#endif

using namespace RakNet;

//DataStructures::MemoryPool<RakString::SharedString> RakString::pool;
unsigned int RakString::nPos=(unsigned int) -1;
static SimpleMutex poolMutex;
RakString::SharedString RakString::emptyString={0,0,0,"",""};
//RakString::SharedString *RakString::sharedStringFreeList=0;
//unsigned int RakString::sharedStringFreeListAllocationCount=0;
DataStructures::List<RakString::SharedString*> RakString::freeList;

RakString::RakString()
{
	sharedString=&emptyString;
}
RakString::RakString( RakString::SharedString *_sharedString )
{
	sharedString=_sharedString;
}
RakString::RakString(char input)
{
	char str[2];
	str[0]=input;
	str[1]=0;
	Assign(str);
}
RakString::RakString(unsigned char input)
{
	char str[2];
	str[0]=(char) input;
	str[1]=0;
	Assign(str);
}
RakString::RakString( const RakString & rhs)
{
	sharedString=rhs.sharedString;
	rhs.sharedString->refCount++;
}
RakString::RakString(const char *format, ...)
{
	char text[8096];
	va_list ap;
	va_start(ap, format);
	_vsnprintf(text, 8096, format, ap);
	va_end(ap);
	text[8096-1]=0;
	Assign(text);
}
RakString::~RakString()
{
	Free();
}
RakString& RakString::operator = ( const RakString& rhs )
{
	Free();
	sharedString=rhs.sharedString;
	sharedString->refCount++;	
	return *this;
}
void RakString::Realloc(SharedString *sharedString, size_t bytes)
{
	if (bytes<=sharedString->bytesUsed)
		return;
	size_t oldBytes = sharedString->bytesUsed;
	size_t newBytes;
	const int smallStringSize = 128-sizeof(unsigned int)-sizeof(size_t)-sizeof(char*)*2;
	newBytes = GetSizeToAllocate(bytes);
	if (oldBytes <=smallStringSize && newBytes > smallStringSize)
	{
		sharedString->bigString=(char*) rakMalloc(newBytes);
		strcpy(sharedString->bigString, sharedString->smallString);
		sharedString->c_str=sharedString->bigString;
	}
	else if (oldBytes>smallStringSize)
	{
		sharedString->bigString=(char*) rakRealloc(sharedString->bigString,newBytes);
	}
	sharedString->bytesUsed=newBytes;
}
RakString& RakString::operator +=( const RakString& rhs)
{
	if (rhs.IsEmpty())
		return *this;

	if (IsEmpty())
	{
		Assign(rhs);
	}
	else
	{
		size_t strLen=rhs.GetLength()+GetLength()+1;
		Realloc(sharedString, strLen+GetLength());
		strcat(sharedString->c_str,rhs);
	}
	return *this;
}
unsigned char RakString::operator[] ( const unsigned int position ) const
{
	RakAssert(position<GetLength());
	return sharedString->c_str[position];
}
bool RakString::operator==(const RakString &rhs) const
{
	return strcmp(sharedString->c_str,rhs.sharedString->c_str)==0;
}
bool RakString::operator!=(const RakString &rhs) const
{
	return strcmp(sharedString->c_str,rhs.sharedString->c_str)!=0;
}
const RakNet::RakString operator+(const RakNet::RakString &lhs, const RakNet::RakString &rhs)
{
	if (lhs.IsEmpty() && rhs.IsEmpty())
		return RakString(&RakString::emptyString);
	if (lhs.IsEmpty())
	{
		rhs.sharedString->refCount++;
		return RakString(rhs.sharedString);
	}
	if (rhs.IsEmpty())
	{
		lhs.sharedString->refCount++;
		return RakString(lhs.sharedString);
	}

	size_t len1 = lhs.GetLength();
	size_t len2 = rhs.GetLength();
	size_t allocatedBytes = len1 + len2 + 1;
	allocatedBytes = RakString::GetSizeToAllocate(allocatedBytes);
	RakString::SharedString *sharedString;

	poolMutex.Lock();
	// sharedString = RakString::pool.Allocate();
	if (RakString::freeList.Size()==0)
	{
		//RakString::sharedStringFreeList=(RakString::SharedString*) rakRealloc(RakString::sharedStringFreeList,(RakString::sharedStringFreeListAllocationCount+1024)*sizeof(RakString::SharedString));
		unsigned i;
		for (i=0; i < 1024; i++)
		{
		//	RakString::freeList.Insert(RakString::sharedStringFreeList+i+RakString::sharedStringFreeListAllocationCount);
			RakString::freeList.Insert((RakString::SharedString*)rakMalloc(sizeof(RakString::SharedString)));

		}
		//RakString::sharedStringFreeListAllocationCount+=1024;
	}
	sharedString = RakString::freeList[RakString::freeList.Size()-1];
	RakString::freeList.RemoveAtIndex(RakString::freeList.Size()-1);
	poolMutex.Unlock();

	const int smallStringSize = 128-sizeof(unsigned int)-sizeof(size_t)-sizeof(char*)*2;
	sharedString->bytesUsed=allocatedBytes;
	sharedString->refCount=1;
	if (allocatedBytes <= smallStringSize)
	{
		sharedString->c_str=sharedString->smallString;
	}
	else
	{
		sharedString->bigString=(char*)rakMalloc(sharedString->bytesUsed);
		sharedString->c_str=sharedString->bigString;
	}

	strcpy(sharedString->c_str, lhs);
	strcat(sharedString->c_str, rhs);

	return RakString(sharedString);
}
void RakString::ToLower(void)
{
	Clone();

	size_t strLen = strlen(sharedString->c_str);
	unsigned i;
	for (i=0; i < strLen; i++)
		sharedString->c_str[i]=ToLower(sharedString->c_str[i]);
}
void RakString::ToUpper(void)
{
	Clone();

	size_t strLen = strlen(sharedString->c_str);
	unsigned i;
	for (i=0; i < strLen; i++)
		sharedString->c_str[i]=ToUpper(sharedString->c_str[i]);
}
void RakString::Set(const char *format, ...)
{
	char text[8096];
	va_list ap;
	va_start(ap, format);
	_vsnprintf(text, 8096, format, ap);
	va_end(ap);
	text[8096-1]=0;
	Clear();
	Assign(text);
}
bool RakString::IsEmpty(void) const
{
	return sharedString==&emptyString;
}
size_t RakString::GetLength(void) const
{
	return strlen(sharedString->c_str);
}
void RakString::Replace(unsigned index, unsigned count, unsigned char c)
{
	RakAssert(index+count < GetLength());
	Clone();
	while (index < count)
	{
		sharedString->c_str[index]=c;
		index++;
	}
}
void RakString::Erase(unsigned index, unsigned count)
{
	RakAssert(index+count < GetLength());
	Clone();
	unsigned i;
	for (i=index; i < count; i++)
	{
		sharedString->c_str[i]=sharedString->c_str[i+index];
	}
	sharedString->c_str[i]=0;
}
int RakString::StrCmp(const RakString &rhs) const
{
	return strcmp(sharedString->c_str, rhs);
}
int RakString::StrICmp(const RakString &rhs) const
{
	return stricmp(sharedString->c_str, rhs);
}
void RakString::Printf(void)
{
	printf(sharedString->c_str);
}
void RakString::FPrintf(FILE *fp)
{
	fprintf(fp,sharedString->c_str);
}
void RakString::Clear(void)
{
	Free();
}
void RakString::Assign(const char *str)
{
	if (str[0]==0)
	{
		sharedString=&emptyString;
		return;
	}

	size_t len = strlen(str)+1;
	poolMutex.Lock();
	// sharedString = RakString::pool.Allocate();
	if (RakString::freeList.Size()==0)
	{
		//RakString::sharedStringFreeList=(RakString::SharedString*) rakRealloc(RakString::sharedStringFreeList,(RakString::sharedStringFreeListAllocationCount+1024)*sizeof(RakString::SharedString));
		unsigned i;
		for (i=0; i < 1024; i++)
		{
			//	RakString::freeList.Insert(RakString::sharedStringFreeList+i+RakString::sharedStringFreeListAllocationCount);
			RakString::freeList.Insert((RakString::SharedString*)rakMalloc(sizeof(RakString::SharedString)));

		}
		//RakString::sharedStringFreeListAllocationCount+=1024;
	}
	sharedString = RakString::freeList[RakString::freeList.Size()-1];
	RakString::freeList.RemoveAtIndex(RakString::freeList.Size()-1);
	poolMutex.Unlock();

	const int smallStringSize = 128-sizeof(unsigned int)-sizeof(size_t)-sizeof(char*)*2;
	sharedString->refCount=1;
	if (len <= smallStringSize)
	{
		sharedString->bytesUsed=smallStringSize;
		sharedString->c_str=sharedString->smallString;
	}
	else
	{
		sharedString->bytesUsed=len<<1;
		sharedString->bigString=(char*)rakMalloc(sharedString->bytesUsed);
		sharedString->c_str=sharedString->bigString;
	}

	memcpy(sharedString->c_str, str, len);

}
void RakString::Clone(void)
{
	// Empty or solo then no point to cloning
	if (sharedString==&emptyString || sharedString->refCount==1)
		return;

	char *oldStr = sharedString->c_str;
	Realloc(sharedString, strlen(sharedString->c_str)+1);
	strcpy(sharedString->c_str,oldStr);
}
void RakString::Free(void)
{
	if (sharedString==&emptyString)
		return;
	sharedString->refCount--;
	if (sharedString->refCount==0)
	{
		const int smallStringSize = 128-sizeof(unsigned int)-sizeof(size_t)-sizeof(char*)*2;
		if (sharedString->bytesUsed>smallStringSize)
			rakFree(sharedString->bigString);
		/*
		poolMutex.Lock();
		pool.Release(sharedString);
		poolMutex.Unlock();
		*/

		poolMutex.Lock();
		RakString::freeList.Insert(sharedString);
		poolMutex.Unlock();

		sharedString=&emptyString;
	}
}
unsigned char RakString::ToLower(unsigned char c)
{
	if (c >= 'A' && c <= 'Z')
		return c-'A'+'a';
	return c;
}
unsigned char RakString::ToUpper(unsigned char c)
{
	if (c >= 'a' && c <= 'z')
		return c-'a'+'A';
	return c;
}
/*
int main(void)
{
	RakString s3("Hello world");
	RakString s5=s3;

	RakString s1;
	RakString s2('a');

	RakString s4("%i %f", 5, 6.0);
	MyFunc(s4);

	RakString s6=s3;
	RakString s7=s6;
	RakString s8=s6;
	RakString s9;
	s9=s9;
	RakString s10(s3);
	RakString s11=s10 + s4 + s9 + s2;
	s11+=RakString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	RakString s12("Test");
	s12+=s11;
	bool b1 = s12==s12;
	s11=s5;
	s12.ToUpper();
	s12.ToLower();
	RakString s13;
	bool b3 = s13.IsEmpty();
	s13.Set("blah %s", s12.C_String());
	bool b4 = s13.IsEmpty();
	size_t i1=s13.GetLength();
	s3.Clear();
	s4.Clear();
	s5.Clear();
	s5.Clear();
	MyFunc(s5);
	MyFunc(s6);
	s6.Printf();
	s7.Printf();
	printf("\n");

	static const int repeatCount=7500;
	DataStructures::List<RakString> rakStringList;
	DataStructures::List<std::string> stdStringList;
	DataStructures::List<char*> referenceStringList;
	char *c;
	unsigned i;
	RakNetTime beforeReferenceList, beforeRakString, beforeStdString, afterStdString;

	unsigned loop;
	for (loop=0; loop<2; loop++)
	{
		beforeReferenceList=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
		{
			c = new char [56];
			strcpy(c, "Aalsdkj alsdjf laksdjf ;lasdfj ;lasjfd");
			referenceStringList.Insert(c);
		}
		beforeRakString=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
			rakStringList.Insert("Aalsdkj alsdjf laksdjf ;lasdfj ;lasjfd");
		beforeStdString=RakNet::GetTime();

		for (i=0; i < repeatCount; i++)
			stdStringList.Insert("Aalsdkj alsdjf laksdjf ;lasdfj ;lasjfd");
		afterStdString=RakNet::GetTime();
		printf("Insertion 1 Ref=%i Rak=%i, Std=%i\n", beforeRakString-beforeReferenceList, beforeStdString-beforeRakString, afterStdString-beforeStdString);

		beforeReferenceList=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
		{
			delete referenceStringList[0];
			referenceStringList.RemoveAtIndex(0);
		}
		beforeRakString=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
			rakStringList.RemoveAtIndex(0);
		beforeStdString=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
			stdStringList.RemoveAtIndex(0);
		afterStdString=RakNet::GetTime();
		printf("RemoveHead Ref=%i Rak=%i, Std=%i\n", beforeRakString-beforeReferenceList, beforeStdString-beforeRakString, afterStdString-beforeStdString);

		beforeReferenceList=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
		{
			c = new char [56];
			strcpy(c, "Aalsdkj alsdjf laksdjf ;lasdfj ;lasjfd");
			referenceStringList.Insert(0);
		}
		beforeRakString=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
			rakStringList.Insert("Aalsdkj alsdjf laksdjf ;lasdfj ;lasjfd");
		beforeStdString=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
			stdStringList.Insert("Aalsdkj alsdjf laksdjf ;lasdfj ;lasjfd");
		afterStdString=RakNet::GetTime();
		printf("Insertion 2 Ref=%i Rak=%i, Std=%i\n", beforeRakString-beforeReferenceList, beforeStdString-beforeRakString, afterStdString-beforeStdString);

		beforeReferenceList=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
		{
			delete [] referenceStringList[referenceStringList.Size()-1];
			referenceStringList.RemoveAtIndex(referenceStringList.Size()-1);
		}
		beforeRakString=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
			rakStringList.RemoveAtIndex(rakStringList.Size()-1);
		beforeStdString=RakNet::GetTime();
		for (i=0; i < repeatCount; i++)
			stdStringList.RemoveAtIndex(stdStringList.Size()-1);
		afterStdString=RakNet::GetTime();
		printf("RemoveTail Ref=%i Rak=%i, Std=%i\n", beforeRakString-beforeReferenceList, beforeStdString-beforeRakString, afterStdString-beforeStdString);

	}

	char str[128];
	gets(str);
	return 1;
	*/
