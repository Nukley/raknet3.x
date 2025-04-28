#ifndef __RAK_STRING_H
#define __RAK_STRING_H 

#include "Export.h"
#include "DS_List.h"
#include <stdio.h>

namespace RakNet
{
/// \brief String class
/// Has the following improvements over std::string
/// Reference counting: Suitable to store in lists
/// Varidic assignment operator
/// Doesn't cause linker errors
class RAK_DLL_EXPORT RakString
{
public:
	/// Constructors
	RakString();
	RakString(char input);
	RakString(unsigned char input);
	RakString(const char *format, ...);
	~RakString();
	RakString( const RakString & rhs);

	/// Implicit return of const char*
	operator const char* () const {return sharedString->c_str;}

	/// Same as std::string::c_str
	const char *C_String(void) const {return sharedString->c_str;}

	/// Assigment operator
	RakString& operator = ( const RakString& rhs );

	/// Concatenation
	RakString& operator +=( const RakString& rhs);

	/// Character index. Do not use to change the string however.
	unsigned char operator[] ( const unsigned int position ) const;

	/// Equality
	bool operator==(const RakString &rhs) const;

	/// Inequality
	bool operator!=(const RakString &rhs) const;

	/// Change all characters to lowercase
	void ToLower(void);

	/// Change all characters to uppercase
	void ToUpper(void);

	/// Set the value of the string
	void Set(const char *format, ...);

	/// Returns if the string is empty. Also, C_String() would return ""
	bool IsEmpty(void) const;

	/// Returns the length of the string
	size_t GetLength(void) const;

	/// Replace character(s) in starting at index, for count, with c
	void Replace(unsigned index, unsigned count, unsigned char c);

	/// Erase characters out of the string at index for count
	void Erase(unsigned index, unsigned count);

	/// Compare strings (case sensitive)
	int StrCmp(const RakString &rhs) const;

	/// Compare strings (not case sensitive)
	int StrICmp(const RakString &rhs) const;

	/// Clear the string
	void Clear(void);

	/// Print the string to the screen
	void Printf(void);

	/// Print the string to a file
	void FPrintf(FILE *fp);
	
	/// Means undefined position
	static unsigned int nPos;

	/// \internal
	static size_t GetSizeToAllocate(size_t bytes)
	{
		const int smallStringSize = 128-sizeof(unsigned int)-sizeof(size_t)-sizeof(char*)*2;
		if (bytes<=smallStringSize)
			return smallStringSize;
		else
			return bytes*2;
	}

	/// \internal
	struct SharedString
	{
		unsigned int refCount;
		size_t bytesUsed;
		char *bigString;
		char *c_str;
		char smallString[128-sizeof(unsigned int)-sizeof(size_t)-sizeof(char*)*2];		
	};

	/// \internal
	RakString( SharedString *_sharedString );
//	static SimpleMutex poolMutex;
//	static DataStructures::MemoryPool<SharedString> pool;
	/// \internal
	static SharedString emptyString;
	/// \internal
	SharedString *sharedString;

	//static SharedString *sharedStringFreeList;
	//static unsigned int sharedStringFreeListAllocationCount;
	/// \internal
	/// List of free objects to reduce memory reallocations
	static DataStructures::List<SharedString*> freeList;

protected:
	void Assign(const char *str);
	void Clone(void);
	void Free(void);
	unsigned char ToLower(unsigned char c);
	unsigned char ToUpper(unsigned char c);
	void Realloc(SharedString *sharedString, size_t bytes);
};

}

const RakNet::RakString operator+(const RakNet::RakString &lhs, const RakNet::RakString &rhs);


#endif
