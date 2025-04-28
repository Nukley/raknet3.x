/// \file
/// \brief Automatically serializing and deserializing RPC system. More advanced RPC, but possibly not cross-platform
///
/// This file is part of RakNet Copyright 2003 Kevin Jenkins.
///
/// Usage of RakNet is subject to the appropriate license agreement.
/// Creative Commons Licensees are subject to the
/// license found at
/// http://creativecommons.org/licenses/by-nc/2.5/
/// Single application licensees are subject to the license found at
/// http://www.jenkinssoftware.com/SingleApplicationLicense.html
/// Custom license users are subject to the terms therein.
/// GPL license users are subject to the GNU General Public
/// License as published by the Free
/// Software Foundation; either version 2 of the License, or (at your
/// option) any later version.

#ifndef __AUTO_RPC_H
#define __AUTO_RPC_H

class RakPeerInterface;
class NetworkIDManager;
#include "PluginInterface.h"
#include "DS_Map.h"
#include "PacketPriority.h"
#include "RakNetTypes.h"
#include "BitStream.h"

#ifdef _MSC_VER
#pragma warning( push )
#endif

/// \defgroup AUTO_RPC_GROUP AutoRPC
/// \ingroup PLUGINS_GROUP

namespace RakNet
{

/// Maximum amount of data that can be passed on the stack in a function call
#define ARPC_MAX_STACK_SIZE 65536

/// Easier way to get a pointer to a function member of a C++ class
/// \param[in] autoRPCInstance A pointer to an instance of AutoRPC
/// \param[in] _IDENTIFIER_ C string identifier to use on the remote system to call the function
/// \param[in] _RETURN_ Return value of the function
/// \param[in] _CLASS_ Base-most class of the containing class that contains your function
/// \param[in] _FUNCTION_ Name of the function
/// \param[in] _PARAMS_ Parameter list, include parenthesis
#define ARPC_REGISTER_CPP_FUNCTION(autoRPCInstance, _IDENTIFIER_, _RETURN_, _CLASS_, _FUNCTION_, _PARAMS_) \
{ \
union \
{ \
	_RETURN_ (__cdecl _CLASS_::*__memberFunctionPtr)_PARAMS_; \
	void* __voidFunc; \
}; \
	__memberFunctionPtr=&_CLASS_::_FUNCTION_; \
	(autoRPCInstance)->RegisterFunction(_IDENTIFIER_, __voidFunc, true); \
}

/// Error codes returned by a remote system as to why an RPC function call cannot execute
enum RPCErrorCodes
{
	/// AutoRPC::SetNetworkIDManager() was not called, and it must be called to call a C++ object member
	RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE,

	/// Cannot execute C++ object member call because the object specified by SetRecipientObject() does not exist on this system
	RPC_ERROR_OBJECT_DOES_NOT_EXIST,

	/// Internal error, index optimization for function lookup does not exist
	RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE,

	/// Named function was not registered with RegisterFunction(). Check your spelling.
	RPC_ERROR_FUNCTION_NOT_REGISTERED,

	/// Named function was registered, but later unregistered with UnregisterFunction() and can no longer be called.
	RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED,

	/// SetRecipientObject() was not called before Call(), but RegisterFunction() was called with isObjectMember=true
	/// If you intended to call a CPP function, call SetRecipientObject() with a valid object first.
	RPC_ERROR_CALLING_CPP_AS_C,

	/// SetRecipientObject() was called before Call(), but RegisterFunction() was called with isObjectMember=false
	/// If you intended to call a C function, call SetRecipientObject(UNASSIGNED_NETWORK_ID) first.
	RPC_ERROR_CALLING_C_AS_CPP,

	/// Internal error, passed stack is bigger than current stack. Check that the version is the same on both systems.
	RPC_ERROR_STACK_TOO_SMALL,

	/// Internal error, formatting error with how the stack was serialized
	RPC_ERROR_STACK_DESERIALIZATION_FAILED,
};

/// The AutoRPC plugin allows you to call remote functions as if they were local functions, using the standard function call syntax
/// No serialization or deserialization is needed.
/// Advantages are that this is easier to use than regular RPC system.
/// Disadvantages is that all parameters must be passable on the stack using memcpy (shallow copy). For other types of parameters, use SetOutgoingExtraData() and GetIncomingExtraData()
/// Use the old system, or regular message passing, if you need greater flexibility
/// \ingroup AUTO_RPC_GROUP
class AutoRPC : public PluginInterface
{
public:
	/// Constructor
	AutoRPC();

	/// Destructor
	virtual ~AutoRPC();

	/// Sets the network ID manager to use for object lookup
	/// Required to call C++ object member functions via SetRecipientObject()
	/// \param[in] idMan Pointer to the network ID manager to use
	void SetNetworkIDManager(NetworkIDManager *idMan);

	/// Registers a function pointer to be callable given an identifier for the pointer
	/// \param[in] uniqueIdentifier String identifying the function. Recommended that this is the name of the function
	/// \param[in] functionPtr Pointer to the function. For C, just pass the name of the function. For C++, use ARPC_REGISTER_CPP_FUNCTION
	/// \param[in] isObjectMember false if a C function. True if a member function of an object (C++)
	/// \return True on success, false on uniqueIdentifier already used
	bool RegisterFunction(const char *uniqueIdentifier, void *functionPtr, bool isObjectMember);

	/// Unregisters a function pointer to be callable given an identifier for the pointer
	/// \param[in] uniqueIdentifier String identifying the function.
	/// \param[in] isObjectMember false if a C function. True if a member function of an object (C++)
	/// \return True on success, false on function was not previously or is not currently registered.
	bool UnregisterFunction(const char *uniqueIdentifier, bool isObjectMember);
	
	/// Send or stop sending a timestamp with all following calls to Call()
	/// Use GetLastSenderTimestamp() to read the timestamp.
	/// \param[in] timeStamp Non-zero to pass this timestamp using the ID_TIMESTAMP system. 0 to clear passing a timestamp.
	void SetTimestamp(RakNetTime timeStamp);

	/// Set parameters to pass to RakPeer::Send() for all following calls to Call()
	/// Deafults to HIGH_PRIORITY, RELIABLE_ORDERED, ordering channel 0
	/// \param[in] priority See RakPeer::Send()
	/// \param[in] reliability See RakPeer::Send()
	/// \param[in] orderingChannel See RakPeer::Send()
	void SetSendParams(PacketPriority priority, PacketReliability reliability, char orderingChannel);

	/// Set system to send to for all following calls to Call()
	/// Defaults to UNASSIGNED_SYSTEM_ADDRESS, broadcast=true
	/// \param[in] systemAddress See RakPeer::Send()
	/// \param[in] broadcast See RakPeer::Send()
	void SetRecipientAddress(SystemAddress systemAddress, bool broadcast);

	/// Set the NetworkID to pass for all following calls to Call()
	/// Defaults to UNASSIGNED_NETWORK_ID (none)
	/// If set, the remote function will be considered a C++ function, e.g. an object member function
	/// If set to UNASSIGNED_NETWORK_ID (none), the remote function will be considered a C function
	/// If this is set incorrectly, you will get back either RPC_ERROR_CALLING_C_AS_CPP or RPC_ERROR_CALLING_CPP_AS_C
	/// \sa NetworkIDManager
	/// \param[in] networkID Returned from NetworkIDObject::GetNetworkID()
	void SetRecipientObject(NetworkID networkID);

	/// Write extra data to pass for all following calls to Call()
	/// Use BitStream::Reset to clear extra data. Don't forget to do this or you will waste bandwidth.
	/// \return A bitstream you can write to to send extra data with each following call to Call()
	RakNet::BitStream *SetOutgoingExtraData(void);

	/// If the last received function call has a timestamp included, it is stored and can be retrieved with this function.
	/// \return 0 if the last call did not have a timestamp, else non-zero
	RakNetTime GetLastSenderTimestamp(void) const;

	/// Returns the system address of the last system to send us a received function call
	/// Equivalent to the old system RPCParameters::sender
	/// \return Last system to send an RPC call using this system
	SystemAddress GetLastSenderAddress(void) const;

	/// Returns the instance of RakPeer this plugin was attached to
	RakPeerInterface *GetRakPeer(void) const;

	/// Returns the currently running RPC call identifier, set from RegisterFunction::uniqueIdentifier
	/// Returns an empty string "" if none
	/// \Return which RPC call is currently running
	const char *GetCurrentExecution(void) const;

	/// Gets the bitstream written to via SetOutgoingExtraData().
	/// Data is updated with each incoming function call
	/// \return A bitstream you can read from with extra data that was written with SetOutgoingExtraData();
	RakNet::BitStream *GetIncomingExtraData(void);

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	bool Call(const char *uniqueIdentifier){
		char stack[ARPC_MAX_STACK_SIZE];
		unsigned int writeOffset=0;
		SerializeHeader(stack, 0, &writeOffset);
		return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	/// \param[in] es1 Endian swap parameter 1..x if necessary. Requires __BITSTREAM_NATIVE_END is undefined in RakNetDefines.h
	template <class P1>
	bool Call(const char *uniqueIdentifier, P1 p1,
		bool es1=true)	{
		char stack[ARPC_MAX_STACK_SIZE];
		unsigned int writeOffset=0;
		SerializeHeader(stack, 1, &writeOffset);
		SerializeParamHeader(stack, sizeof(P1), &writeOffset, IsFloatOrDouble(p1), es1);
		SerializeParamData(stack, &p1, sizeof(P1), &writeOffset, es1);
		return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	/// \param[in] es1 Endian swap parameter 1..x if necessary. Requires __BITSTREAM_NATIVE_END is undefined in RakNetDefines.h
	template <class P1, class P2>
	bool Call(const char *uniqueIdentifier, P1 p1, P2 p2,
		bool es1=true, bool es2=true )	{
			char stack[ARPC_MAX_STACK_SIZE];
			unsigned int writeOffset=0;
			SerializeHeader(stack, 2, &writeOffset);
			SerializeParamHeader(stack, sizeof(P1), &writeOffset, IsFloatOrDouble(p1), es1);
			SerializeParamHeader(stack, sizeof(P2), &writeOffset, IsFloatOrDouble(p2), es2);
			SerializeParamData(stack, &p1, sizeof(P1), &writeOffset, es1);
			SerializeParamData(stack, &p2, sizeof(P2), &writeOffset, es2);
			return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	/// \param[in] es1 Endian swap parameter 1..x if necessary. Requires __BITSTREAM_NATIVE_END is undefined in RakNetDefines.h
	template <class P1, class P2, class P3>
	bool Call(const char *uniqueIdentifier, P1 p1, P2 p2, P3 p3,
		bool es1=true, bool es2=true, bool es3=true )	{
			char stack[ARPC_MAX_STACK_SIZE];
			unsigned int writeOffset=0;
			SerializeHeader(stack, 3, &writeOffset);
			SerializeParamHeader(stack, sizeof(P1), &writeOffset, IsFloatOrDouble(p1), es1);
			SerializeParamHeader(stack, sizeof(P2), &writeOffset, IsFloatOrDouble(p2), es2);
			SerializeParamHeader(stack, sizeof(P3), &writeOffset, IsFloatOrDouble(p3), es3);
			SerializeParamData(stack, &p1, sizeof(P1), &writeOffset, es1);
			SerializeParamData(stack, &p2, sizeof(P2), &writeOffset, es2);
			SerializeParamData(stack, &p3, sizeof(P3), &writeOffset, es3);
			return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	/// \param[in] es1 Endian swap parameter 1..x if necessary. Requires __BITSTREAM_NATIVE_END is undefined in RakNetDefines.h
	template <class P1, class P2, class P3, class P4>
	bool Call(const char *uniqueIdentifier, P1 p1, P2 p2, P3 p3, P4 p4,
		bool es1=true, bool es2=true, bool es3=true, bool es4=true )	{
			char stack[ARPC_MAX_STACK_SIZE];
			unsigned int writeOffset=0;
			SerializeHeader(stack, 4, &writeOffset);
			SerializeParamHeader(stack, sizeof(P1), &writeOffset, IsFloatOrDouble(p1), es1);
			SerializeParamHeader(stack, sizeof(P2), &writeOffset, IsFloatOrDouble(p2), es2);
			SerializeParamHeader(stack, sizeof(P3), &writeOffset, IsFloatOrDouble(p3), es3);
			SerializeParamHeader(stack, sizeof(P4), &writeOffset, IsFloatOrDouble(p4), es4);
			SerializeParamData(stack, &p1, sizeof(P1), &writeOffset, es1);
			SerializeParamData(stack, &p2, sizeof(P2), &writeOffset, es2);
			SerializeParamData(stack, &p3, sizeof(P3), &writeOffset, es3);
			SerializeParamData(stack, &p4, sizeof(P4), &writeOffset, es4);
			return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	/// \param[in] es1 Endian swap parameter 1..x if necessary. Requires __BITSTREAM_NATIVE_END is undefined in RakNetDefines.h
	template <class P1, class P2, class P3, class P4, class P5>
	bool Call(const char *uniqueIdentifier, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5,
		bool es1=true, bool es2=true, bool es3=true, bool es4=true, bool es5=true )	{
			char stack[ARPC_MAX_STACK_SIZE];
			unsigned int writeOffset=0;
			SerializeHeader(stack, 5, &writeOffset);
			SerializeParamHeader(stack, sizeof(P1), &writeOffset, IsFloatOrDouble(p1), es1);
			SerializeParamHeader(stack, sizeof(P2), &writeOffset, IsFloatOrDouble(p2), es2);
			SerializeParamHeader(stack, sizeof(P3), &writeOffset, IsFloatOrDouble(p3), es3);
			SerializeParamHeader(stack, sizeof(P4), &writeOffset, IsFloatOrDouble(p4), es4);
			SerializeParamHeader(stack, sizeof(P5), &writeOffset, IsFloatOrDouble(p5), es5);
			SerializeParamData(stack, &p1, sizeof(P1), &writeOffset, es1);
			SerializeParamData(stack, &p2, sizeof(P2), &writeOffset, es2);
			SerializeParamData(stack, &p3, sizeof(P3), &writeOffset, es3);
			SerializeParamData(stack, &p4, sizeof(P4), &writeOffset, es4);
			SerializeParamData(stack, &p5, sizeof(P5), &writeOffset, es5);
			return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	/// \param[in] es1 Endian swap parameter 1..x if necessary. Requires __BITSTREAM_NATIVE_END is undefined in RakNetDefines.h
	template <class P1, class P2, class P3, class P4, class P5, class P6>
	bool Call(const char *uniqueIdentifier, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6,
		bool es1=true, bool es2=true, bool es3=true, bool es4=true, bool es5=true, bool es6=true )	{
			char stack[ARPC_MAX_STACK_SIZE];
			unsigned int writeOffset=0;
			SerializeHeader(stack, 6, &writeOffset);
			SerializeParamHeader(stack, sizeof(P1), &writeOffset, IsFloatOrDouble(p1), es1);
			SerializeParamHeader(stack, sizeof(P2), &writeOffset, IsFloatOrDouble(p2), es2);
			SerializeParamHeader(stack, sizeof(P3), &writeOffset, IsFloatOrDouble(p3), es3);
			SerializeParamHeader(stack, sizeof(P4), &writeOffset, IsFloatOrDouble(p4), es4);
			SerializeParamHeader(stack, sizeof(P5), &writeOffset, IsFloatOrDouble(p5), es5);
			SerializeParamHeader(stack, sizeof(P6), &writeOffset, IsFloatOrDouble(p6), es6);
			SerializeParamData(stack, &p1, sizeof(P1), &writeOffset, es1);
			SerializeParamData(stack, &p2, sizeof(P2), &writeOffset, es2);
			SerializeParamData(stack, &p3, sizeof(P3), &writeOffset, es3);
			SerializeParamData(stack, &p4, sizeof(P4), &writeOffset, es4);
			SerializeParamData(stack, &p5, sizeof(P5), &writeOffset, es5);
			SerializeParamData(stack, &p6, sizeof(P6), &writeOffset, es6);
			return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	/// \param[in] es1 Endian swap parameter 1..x if necessary. Requires __BITSTREAM_NATIVE_END is undefined in RakNetDefines.h
	template <class P1, class P2, class P3, class P4, class P5, class P6, class P7>
	bool Call(const char *uniqueIdentifier, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7,
		bool es1=true, bool es2=true, bool es3=true, bool es4=true, bool es5=true, bool es6=true, bool es7=true )	{
			char stack[ARPC_MAX_STACK_SIZE];
			unsigned int writeOffset=0;
			SerializeHeader(stack, 7, &writeOffset);
			SerializeParamHeader(stack, sizeof(P1), &writeOffset, IsFloatOrDouble(p1), es1);
			SerializeParamHeader(stack, sizeof(P2), &writeOffset, IsFloatOrDouble(p2), es2);
			SerializeParamHeader(stack, sizeof(P3), &writeOffset, IsFloatOrDouble(p3), es3);
			SerializeParamHeader(stack, sizeof(P4), &writeOffset, IsFloatOrDouble(p4), es4);
			SerializeParamHeader(stack, sizeof(P5), &writeOffset, IsFloatOrDouble(p5), es5);
			SerializeParamHeader(stack, sizeof(P6), &writeOffset, IsFloatOrDouble(p6), es6);
			SerializeParamHeader(stack, sizeof(P7), &writeOffset, IsFloatOrDouble(p7), es7);
			SerializeParamData(stack, &p1, sizeof(P1), &writeOffset, es1);
			SerializeParamData(stack, &p2, sizeof(P2), &writeOffset, es2);
			SerializeParamData(stack, &p3, sizeof(P3), &writeOffset, es3);
			SerializeParamData(stack, &p4, sizeof(P4), &writeOffset, es4);
			SerializeParamData(stack, &p5, sizeof(P5), &writeOffset, es5);
			SerializeParamData(stack, &p6, sizeof(P6), &writeOffset, es6);
			SerializeParamData(stack, &p7, sizeof(P7), &writeOffset, es7);
			return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	/// Calls a remote function, using whatever was last passed to SetTimestamp(), SetSendParams(), SetRecipientAddress(), and SetRecipientObject()
	/// Passed parameter(s), if any, are passed via memcpy and pushed on the stack for the remote function
	/// \note This ONLY works with variables that are passable via memcpy! If you need more flexibility, use SetOutgoingExtraData() and GetIncomingExtraData()
	/// \note The this pointer, for this instance of AutoRPC, is pushed as the last parameter on the stack. See AutoRPCSample.ccp for an example of this
	/// \param[in] es1 Endian swap parameter 1..x if necessary. Requires __BITSTREAM_NATIVE_END is undefined in RakNetDefines.h
	template <class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8>
	bool Call(const char *uniqueIdentifier, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7, P8 p8,
		bool es1=true, bool es2=true, bool es3=true, bool es4=true, bool es5=true, bool es6=true, bool es7=true, bool es8=true ) {
			char stack[ARPC_MAX_STACK_SIZE];
			unsigned int writeOffset=0;
			SerializeHeader(stack, 8, &writeOffset);
			SerializeParamHeader(stack, sizeof(P1), &writeOffset, IsFloatOrDouble(p1), es1);
			SerializeParamHeader(stack, sizeof(P2), &writeOffset, IsFloatOrDouble(p2), es2);
			SerializeParamHeader(stack, sizeof(P3), &writeOffset, IsFloatOrDouble(p3), es3);
			SerializeParamHeader(stack, sizeof(P4), &writeOffset, IsFloatOrDouble(p4), es4);
			SerializeParamHeader(stack, sizeof(P5), &writeOffset, IsFloatOrDouble(p5), es5);
			SerializeParamHeader(stack, sizeof(P6), &writeOffset, IsFloatOrDouble(p6), es6);
			SerializeParamHeader(stack, sizeof(P7), &writeOffset, IsFloatOrDouble(p7), es7);
			SerializeParamHeader(stack, sizeof(P8), &writeOffset, IsFloatOrDouble(p8), es8);
			SerializeParamData(stack, &p1, sizeof(P1), &writeOffset, es1);
			SerializeParamData(stack, &p2, sizeof(P2), &writeOffset, es2);
			SerializeParamData(stack, &p3, sizeof(P3), &writeOffset, es3);
			SerializeParamData(stack, &p4, sizeof(P4), &writeOffset, es4);
			SerializeParamData(stack, &p5, sizeof(P5), &writeOffset, es5);
			SerializeParamData(stack, &p6, sizeof(P6), &writeOffset, es6);
			SerializeParamData(stack, &p7, sizeof(P7), &writeOffset, es7);
			SerializeParamData(stack, &p8, sizeof(P8), &writeOffset, es8);
			return SendCall(uniqueIdentifier, stack, writeOffset);
	}

	// If you need more than 8 parameters, just add it here...

	// ---------------------------- ALL INTERNAL AFTER HERE ----------------------------



	/// \internal
	/// Check what type this parameter is
	template <class P1>
	bool IsFloatOrDouble(P1 p1) {
#pragma warning(disable:4100)   // warning C4100: unreferenced formal parameter
		return false;}
	template <class P1>
	bool IsFloatOrDouble(float f) {
#pragma warning(disable:4100)   // warning C4100: unreferenced formal parameter
		return true;}
	template <class P1>
	bool IsFloatOrDouble(double f) {
#pragma warning(disable:4100)   // warning C4100: unreferenced formal parameter
		return true;}
	template <class P1>
	bool IsFloatOrDouble(long double f) {
#pragma warning(disable:4100)   // warning C4100: unreferenced formal parameter
		return true;}
	
	/// \internal
	/// Identifies an RPC function, by string identifier and if it is a C or C++ function
	struct RPCIdentifier
	{
		char *uniqueIdentifier;
		bool isObjectMember;
	};

	/// \internal
	/// The RPC identifier, and a pointer to the function
	struct LocalRPCFunction
	{
		RPCIdentifier identifier;
		void *functionPtr;
	};

	/// \internal
	/// The RPC identifier, and the index of the function on a remote system
	struct RemoteRPCFunction
	{
		RPCIdentifier identifier;
		unsigned int functionIndex;
	};

	/// \internal
	static int RemoteRPCFunctionComp( const RPCIdentifier &key, const RemoteRPCFunction &data );

	/// \internal
	/// Writes number of parameters to push on the stack
	static void SerializeHeader(char *out, unsigned int numParams, unsigned int *writeOffset);

	/// \internal
	/// Writes size of each paramter to push on the stack, and endian swaps if necessary
	static void SerializeParamHeader(char *out, unsigned int paramLength, unsigned int *writeOffset, bool isFloatOrDouble, bool endianSwap);

	/// \internal
	/// Writes each paramter on the stack with memcpy
	static void SerializeParamData(char *out, void *paramData, unsigned int paramLength, unsigned int *writeOffset, bool endianSwap);

	/// \internal
	/// Sends the RPC call, with a given serialized stack
	bool SendCall(const char *uniqueIdentifier, const char *stack, unsigned int bytesOnStack);


protected:

	// --------------------------------------------------------------------------------------------
	// Packet handling functions
	// --------------------------------------------------------------------------------------------
	void OnAttach(RakPeerInterface *peer);
	virtual PluginReceiveResult OnReceive(RakPeerInterface *peer, Packet *packet);
	virtual void OnAutoRPCCall(SystemAddress systemAddress, unsigned char *data, unsigned int lengthInBytes);
	virtual void OnRPCRemoteIndex(SystemAddress systemAddress, unsigned char *data, unsigned int lengthInBytes);
	virtual void OnCloseConnection(RakPeerInterface *peer, SystemAddress systemAddress);
	virtual void OnShutdown(RakPeerInterface *peer);

	void Clear(void);

	void SendError(SystemAddress target, unsigned char errorCode);
	unsigned GetLocalFunctionIndex(RPCIdentifier identifier);
	bool GetRemoteFunctionIndex(SystemAddress systemAddress, RPCIdentifier identifier, unsigned int *outerIndex, unsigned int *innerIndex);

	static bool DeserializeParameters(char *out, unsigned int outLength, unsigned int *bytesWritten,
		char *in, unsigned int inLength,
		unsigned char *numParameters,
		unsigned int *parameterLengths, unsigned int maxParameters);

	static void CallWithStack(const char *inputStack, unsigned int numBytes, void *functionPtr, void *lastParam, void *thisPtr);

	DataStructures::List<LocalRPCFunction> localFunctions;
	DataStructures::Map<SystemAddress, DataStructures::OrderedList<RPCIdentifier, RemoteRPCFunction, AutoRPC::RemoteRPCFunctionComp> *> remoteFunctions;

	RakNetTime outgoingTimestamp;
	PacketPriority outgoingPriority;
	PacketReliability outgoingReliability;
	char outgoingOrderingChannel;
	SystemAddress outgoingSystemAddress;
	bool outgoingBroadcast;
	NetworkID outgoingNetworkID;
	RakNet::BitStream outgoingExtraData;

	RakNetTime incomingTimeStamp;
	SystemAddress incomingSystemAddress;
	RakNet::BitStream incomingExtraData;

	RakPeerInterface *rakPeer;
	NetworkIDManager *networkIdManager;
	char currentExecution[512];
};

} // End namespace

#endif

#ifdef _MSC_VER
#pragma warning( pop )
#endif
