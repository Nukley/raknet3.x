#include "AutoRPC.h"
#include "RakMemoryOverride.h"
#include "RakAssert.h"
#include "StringCompressor.h"
#include "BitStream.h"
#include "Types.h"
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "NetworkIDObject.h"
#include "NetworkIDManager.h"
#include <stdlib.h>

using namespace RakNet;

#ifdef _MSC_VER
#pragma warning( push )
#endif

// Just from looking at the disassembly, the minimum size pushed on the stack is a multiple of this value
static const int MIN_FUNC_STACK_ALIGNMENT=4;

int AutoRPC::RemoteRPCFunctionComp( const RPCIdentifier &key, const RemoteRPCFunction &data )
{
	if (key.isObjectMember==false && data.identifier.isObjectMember==true)
		return -1;
	if (key.isObjectMember==true && data.identifier.isObjectMember==false)
		return 1;
	return strcmp(key.uniqueIdentifier, data.identifier.uniqueIdentifier);
}

AutoRPC::AutoRPC()
{
	currentExecution[0]=0;
	rakPeer=0;
	networkIdManager=0;
	outgoingTimestamp=0;
	outgoingPriority=HIGH_PRIORITY;
	outgoingReliability=RELIABLE_ORDERED;
	outgoingOrderingChannel=0;
	outgoingBroadcast=true;
	incomingTimeStamp=0;
	DataStructures::Map<SystemAddress, DataStructures::OrderedList<RPCIdentifier, RemoteRPCFunction, AutoRPC::RemoteRPCFunctionComp> *>::IMPLEMENT_DEFAULT_COMPARISON();
}

AutoRPC::~AutoRPC()
{
	Clear();
}
void AutoRPC::SetNetworkIDManager(NetworkIDManager *idMan)
{
	networkIdManager=idMan;
}
bool AutoRPC::RegisterFunction(const char *uniqueIdentifier, void *functionPtr, bool isObjectMember)
{
	if (uniqueIdentifier==0 || functionPtr==0)
	{
		RakAssert(0);
		return false;
	}

	RPCIdentifier identifier;
	identifier.isObjectMember=isObjectMember;
	identifier.uniqueIdentifier=(char*) uniqueIdentifier;
	unsigned localIndex = GetLocalFunctionIndex(identifier);
	// Already registered?
	if (localIndex!=(unsigned)-1 && localFunctions[localIndex].functionPtr!=0)
		return false;
	if (localIndex!=(unsigned)-1)
	{
		// Reenable existing
		localFunctions[localIndex].functionPtr=functionPtr;
	}
	else
	{
		// Add new
		LocalRPCFunction func;
		func.functionPtr=functionPtr;
		func.identifier.isObjectMember=isObjectMember;
		func.identifier.uniqueIdentifier = (char*) rakMalloc(strlen(uniqueIdentifier)+1);
		strcpy(func.identifier.uniqueIdentifier, uniqueIdentifier);
		localFunctions.Insert(func);
	}
	return true;
}
bool AutoRPC::UnregisterFunction(const char *uniqueIdentifier, bool isObjectMember)
{
	if (uniqueIdentifier==0)
	{
		RakAssert(0);
		return false;
	}

	RPCIdentifier identifier;
	identifier.isObjectMember=isObjectMember;
	identifier.uniqueIdentifier=(char*) uniqueIdentifier;
	unsigned localIndex = GetLocalFunctionIndex(identifier);
	// Not registered?
	if (localIndex==(unsigned)-1)
		return false;
	// Leave the id in, in case the function is set again later. That way we keep the same remote index
	localFunctions[localIndex].functionPtr=0;
	return true;
}
void AutoRPC::SetTimestamp(RakNetTime timeStamp)
{
	outgoingTimestamp=timeStamp;
}
void AutoRPC::SetSendParams(PacketPriority priority, PacketReliability reliability, char orderingChannel)
{
	outgoingPriority=priority;
	outgoingReliability=reliability;
	outgoingOrderingChannel=orderingChannel;
}
void AutoRPC::SetRecipientAddress(SystemAddress systemAddress, bool broadcast)
{
	outgoingSystemAddress=systemAddress;
	outgoingBroadcast=broadcast;
}
void AutoRPC::SetRecipientObject(NetworkID networkID)
{
	outgoingNetworkID=networkID;
}
RakNet::BitStream *AutoRPC::SetOutgoingExtraData(void)
{
	return &outgoingExtraData;
}
RakNetTime AutoRPC::GetLastSenderTimestamp(void) const
{
	return incomingTimeStamp;
}
SystemAddress AutoRPC::GetLastSenderAddress(void) const
{
	return incomingSystemAddress;
}
RakPeerInterface *AutoRPC::GetRakPeer(void) const
{
	return rakPeer;
}
const char *AutoRPC::GetCurrentExecution(void) const
{
	return (const char *) currentExecution;
}
RakNet::BitStream *AutoRPC::GetIncomingExtraData(void)
{
	return &incomingExtraData;
}
bool AutoRPC::SendCall(const char *uniqueIdentifier, const char *stack, unsigned int bytesOnStack)
{
	SystemAddress systemAddr;
	RPCIdentifier identifier;
	unsigned int outerIndex;
	unsigned int innerIndex;

	if (uniqueIdentifier==0)
		return false;

	identifier.uniqueIdentifier=(char*) uniqueIdentifier;
	identifier.isObjectMember=(outgoingNetworkID!=UNASSIGNED_NETWORK_ID);

	RakNet::BitStream bs;
	if (outgoingTimestamp!=0)
	{
		bs.Write((MessageID)ID_TIMESTAMP);
		bs.Write(outgoingTimestamp);
	}
	bs.Write((MessageID)ID_AUTO_RPC_CALL);
	bs.WriteCompressed(outgoingExtraData.GetNumberOfBitsUsed());
	bs.Write(&outgoingExtraData);
	int writeOffset = bs.GetWriteOffset();
	if (outgoingBroadcast)
	{
		unsigned systemIndex;
		for (systemIndex=0; systemIndex < rakPeer->GetMaximumNumberOfPeers(); systemIndex++)
		{
			systemAddr=rakPeer->GetSystemAddressFromIndex(systemIndex);
			if (systemAddr!=UNASSIGNED_SYSTEM_ADDRESS)
			{
				if (outgoingNetworkID!=UNASSIGNED_NETWORK_ID)
				{
					bs.Write(true);
					bs.Write(outgoingNetworkID);
				}
				else
				{
					bs.Write(false);
				}
				if (GetRemoteFunctionIndex(systemAddr, identifier, &outerIndex, &innerIndex))
				{
					// Write a number to identify the function if possible, for faster lookup and less bandwidth
					bs.Write(true);
					bs.WriteCompressed(remoteFunctions[outerIndex]->operator [](innerIndex).functionIndex);
				}
				else
				{
					bs.Write(false);
					stringCompressor->EncodeString(uniqueIdentifier, 512, &bs, 0);
				}

				bs.WriteCompressed(bytesOnStack);
				bs.WriteAlignedBytes((const unsigned char*) stack, bytesOnStack);
				rakPeer->Send(&bs, outgoingPriority, outgoingReliability, outgoingOrderingChannel, systemAddr, false);

				// Start writing again after ID_AUTO_RPC_CALL
				bs.SetWriteOffset(writeOffset);
			}
		}
	}
	else
	{
		systemAddr = outgoingSystemAddress;
		if (systemAddr!=UNASSIGNED_SYSTEM_ADDRESS)
		{
			if (GetRemoteFunctionIndex(systemAddr, identifier, &outerIndex, &innerIndex))
			{
				// Write a number to identify the function if possible, for faster lookup and less bandwidth
				bs.Write(true);
				bs.WriteCompressed(remoteFunctions[outerIndex]->operator [](innerIndex).functionIndex);
			}
			else
			{
				bs.Write(false);
				stringCompressor->EncodeString(uniqueIdentifier, 512, &bs, 0);
			}

			bs.WriteCompressed(bytesOnStack);
			bs.WriteAlignedBytes((const unsigned char*) stack, bytesOnStack);
			rakPeer->Send(&bs, outgoingPriority, outgoingReliability, outgoingOrderingChannel, systemAddr, false);
		}
		else
			return false;
	}
	return true;
}
void AutoRPC::OnAttach(RakPeerInterface *peer)
{
	rakPeer=peer;
	outgoingSystemAddress=UNASSIGNED_SYSTEM_ADDRESS;
	outgoingNetworkID=UNASSIGNED_NETWORK_ID;
	incomingSystemAddress=UNASSIGNED_SYSTEM_ADDRESS;

}
PluginReceiveResult AutoRPC::OnReceive(RakPeerInterface *peer, Packet *packet)
{
	RakNetTime timestamp=0;
	unsigned char packetIdentifier, packetDataOffset;
	if ( ( unsigned char ) packet->data[ 0 ] == ID_TIMESTAMP )
	{
		if ( packet->length > sizeof( unsigned char ) + sizeof( RakNetTime ) )
		{
			packetIdentifier = ( unsigned char ) packet->data[ sizeof( unsigned char ) + sizeof( RakNetTime ) ];
			memcpy(&timestamp, packet->data+sizeof(unsigned char), sizeof(RakNetTime));
			packetDataOffset=sizeof( unsigned char )*2 + sizeof( RakNetTime );
		}
		else
			return RR_STOP_PROCESSING_AND_DEALLOCATE;
	}
	else
	{
		packetIdentifier = ( unsigned char ) packet->data[ 0 ];
		packetDataOffset=sizeof( unsigned char );
	}

	switch (packetIdentifier)
	{
	case ID_DISCONNECTION_NOTIFICATION:
	case ID_CONNECTION_LOST:
		OnCloseConnection(peer, packet->systemAddress);
		return RR_CONTINUE_PROCESSING;
	case ID_AUTO_RPC_CALL:
		incomingTimeStamp=timestamp;
		incomingSystemAddress=packet->systemAddress;
		OnAutoRPCCall(packet->systemAddress, packet->data+packetDataOffset, packet->length-packetDataOffset);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	case ID_AUTO_RPC_REMOTE_INDEX:
		OnRPCRemoteIndex(packet->systemAddress, packet->data+packetDataOffset, packet->length-packetDataOffset);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	}

	return RR_CONTINUE_PROCESSING;
}
#ifdef _MSC_VER
#pragma warning(disable:4100)   // warning C4100: 'peer' : unreferenced formal parameter
#endif
void AutoRPC::OnCloseConnection(RakPeerInterface *peer, SystemAddress systemAddress)
{
	if (remoteFunctions.Has(systemAddress))
	{
		DataStructures::OrderedList<RPCIdentifier, RemoteRPCFunction, AutoRPC::RemoteRPCFunctionComp> *theList = remoteFunctions.Get(systemAddress);
		unsigned i;
		for (i=0; i < theList->Size(); i++)
		{
			if (theList->operator [](i).identifier.uniqueIdentifier)
				rakFree(theList->operator [](i).identifier.uniqueIdentifier);
		}
		delete theList;
		remoteFunctions.Delete(systemAddress);
	}
}
void AutoRPC::OnAutoRPCCall(SystemAddress systemAddress, unsigned char *data, unsigned int lengthInBytes)
{
	RakNet::BitStream bs(data,lengthInBytes,false);

	int numberOfBitsUsed;
	incomingExtraData.Reset();
	bs.ReadCompressed(numberOfBitsUsed);
	if (numberOfBitsUsed > (int) incomingExtraData.GetNumberOfBitsAllocated())
		incomingExtraData.AddBitsAndReallocate(numberOfBitsUsed-(int) incomingExtraData.GetNumberOfBitsAllocated());
	bs.ReadBits(incomingExtraData.GetData(), numberOfBitsUsed, false);
	incomingExtraData.SetWriteOffset(numberOfBitsUsed);

	char inputStack[ARPC_MAX_STACK_SIZE];
	const unsigned int outputStackSize = ARPC_MAX_STACK_SIZE+128*4; // Enough padding to round up to 4 for each parameter, max 128 parameters
	char outputStack[outputStackSize];

	NetworkIDObject *networkIdObject;
	NetworkID networkId;
	bool hasNetworkId;
	bool hasFunctionIndex;
	unsigned int functionIndex;
	unsigned int bytesOnStack;
	char strIdentifier[512];
	bs.Read(hasNetworkId);
	if (hasNetworkId)
	{
		bs.Read(networkId);
		if (networkIdManager==0 && (networkIdManager=rakPeer->GetNetworkIDManager())==0)
		{
			// Failed - Tried to call object member, however, networkIDManager system was never registered
			SendError(systemAddress, RPC_ERROR_NETWORK_ID_MANAGER_UNAVAILABLE);
			return;
		}
		networkIdObject = (NetworkIDObject*) networkIdManager->GET_OBJECT_FROM_ID(networkId);
		if (networkIdObject==0)
		{
			// Failed - Tried to call object member, object does not exist (deleted?)
			SendError(systemAddress, RPC_ERROR_OBJECT_DOES_NOT_EXIST);
			return;
		}
	}
	else
	{
		networkIdObject=0;
	}
	bs.Read(hasFunctionIndex);
	if (hasFunctionIndex)
	{
		bs.ReadCompressed(functionIndex);

		if (functionIndex>localFunctions.Size())
		{
			// Failed - other system specified a totally invalid index
			// Possible causes: Bugs, attempts to crash the system, requested function not registered
			SendError(systemAddress, RPC_ERROR_FUNCTION_INDEX_OUT_OF_RANGE);
			return;
		}
	}
	else
	{
		stringCompressor->DecodeString(strIdentifier,512,&bs,0);

		// Find the registered function with this str
		for (functionIndex=0; functionIndex < localFunctions.Size(); functionIndex++)
		{
			if (localFunctions[functionIndex].identifier.isObjectMember == (networkIdObject!=0) &&
				strcmp(localFunctions[functionIndex].identifier.uniqueIdentifier, strIdentifier)==0)
			{
				// SEND RPC MAPPING
				RakNet::BitStream outgoingBitstream;
				outgoingBitstream.Write((MessageID)ID_AUTO_RPC_REMOTE_INDEX);
				outgoingBitstream.Write(hasNetworkId);
				outgoingBitstream.WriteCompressed(functionIndex);
				stringCompressor->EncodeString(strIdentifier,512,&outgoingBitstream,0);
				rakPeer->Send(&outgoingBitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, false);
				break;
			}
		}

		if (functionIndex==localFunctions.Size())
		{
			for (functionIndex=0; functionIndex < localFunctions.Size(); functionIndex++)
			{
				if (strcmp(localFunctions[functionIndex].identifier.uniqueIdentifier, strIdentifier)==0)
				{
					if (localFunctions[functionIndex].identifier.isObjectMember==true && networkIdObject==0)
					{
						// Failed - Calling C++ function as C function
						SendError(systemAddress, RPC_ERROR_CALLING_CPP_AS_C);
						return;
					}

					if (localFunctions[functionIndex].identifier.isObjectMember==false && networkIdObject!=0)
					{
						// Failed - Calling C++ function as C function
						SendError(systemAddress, RPC_ERROR_CALLING_C_AS_CPP);
						return;
					}
				}
			}

			SendError(systemAddress, RPC_ERROR_FUNCTION_NOT_REGISTERED);
			return;
		}
	}

	if (localFunctions[functionIndex].functionPtr==0)
	{
		// Failed - Function was previously registered, but isn't registered any longer
		SendError(systemAddress, RPC_ERROR_FUNCTION_NO_LONGER_REGISTERED);
		return;
	}

	bs.ReadCompressed(bytesOnStack);
	if (bytesOnStack > ARPC_MAX_STACK_SIZE)
	{
		// Failed - Not enough bytes on predetermined stack. Shouldn't hit this since the sender also uses this value
		SendError(systemAddress, RPC_ERROR_STACK_TOO_SMALL);
		return;
	}

	bs.ReadAlignedBytes((unsigned char *) inputStack,bytesOnStack);
	unsigned int bytesWritten;
	unsigned char numParameters;
	unsigned int parameterLengths[64]; // 64 is arbitrary, just needs to be more than whatever might be serialized
	if (DeserializeParameters(outputStack, outputStackSize, &bytesWritten,
		inputStack, bytesOnStack,
		&numParameters,
		parameterLengths, 64)==false)
	{
		// Failed - Couldn't deserialize
		SendError(systemAddress, RPC_ERROR_STACK_DESERIALIZATION_FAILED);
		return;
	}

	strncpy(currentExecution, localFunctions[functionIndex].identifier.uniqueIdentifier, sizeof(currentExecution)-1);
	CallWithStack(outputStack, bytesWritten, localFunctions[functionIndex].functionPtr, this, networkIdObject);
	currentExecution[0]=0;
}
void AutoRPC::OnRPCRemoteIndex(SystemAddress systemAddress, unsigned char *data, unsigned int lengthInBytes)
{
	// A remote system has given us their internal index for a particular function.
	// Store it and use it from now on, to save bandwidth and search time
	bool objectExists;
	char strIdentifier[512];
	unsigned int insertionIndex;
	unsigned int remoteIndex;
	RemoteRPCFunction newRemoteFunction;
	RakNet::BitStream bs(data,lengthInBytes,false);
	RPCIdentifier identifier;
	bs.Read(identifier.isObjectMember);
	bs.ReadCompressed(remoteIndex);
	stringCompressor->DecodeString(strIdentifier,512,&bs,0);
	identifier.uniqueIdentifier=strIdentifier;

	if (strIdentifier[0]==0)
		return;

	DataStructures::OrderedList<RPCIdentifier, RemoteRPCFunction, AutoRPC::RemoteRPCFunctionComp> *theList;
	if (remoteFunctions.Has(systemAddress))
	{
		theList = remoteFunctions.Get(systemAddress);
		insertionIndex=theList->GetIndexFromKey(identifier, &objectExists);
		if (objectExists==false)
		{
			newRemoteFunction.functionIndex=remoteIndex;
			newRemoteFunction.identifier.isObjectMember=identifier.isObjectMember;
			newRemoteFunction.identifier.uniqueIdentifier = (char*) rakMalloc(strlen(strIdentifier)+1);
			strcpy(newRemoteFunction.identifier.uniqueIdentifier, strIdentifier);
			theList->InsertAtIndex(newRemoteFunction, insertionIndex);
		}
	}
	else
	{
		theList = new DataStructures::OrderedList<RPCIdentifier, RemoteRPCFunction, AutoRPC::RemoteRPCFunctionComp>;

		newRemoteFunction.functionIndex=remoteIndex;
		newRemoteFunction.identifier.isObjectMember=identifier.isObjectMember;
		newRemoteFunction.identifier.uniqueIdentifier = (char*) rakMalloc(strlen(strIdentifier)+1);
		strcpy(newRemoteFunction.identifier.uniqueIdentifier, strIdentifier);
		theList->InsertAtEnd(newRemoteFunction);

		remoteFunctions.SetNew(systemAddress,theList);
	}
}
bool AutoRPC::DeserializeParameters(char *out, unsigned int outLength, unsigned int *bytesWritten,
								  char *in, unsigned int inLength,
								  unsigned char *numParameters,
								  unsigned int *parameterLengths, unsigned int maxParameters)
{
	unsigned int readOffset=0;
	unsigned char parameterIndex;
	unsigned int alignedBytesRequired;
	bool isFloatOrDouble[128];
#ifndef __BITSTREAM_NATIVE_END
	bool endianSwap[128];
#endif
	// Read number of parameters
	*numParameters=in[0];
	*bytesWritten=0;
	readOffset+=sizeof(unsigned char);
	// Is the list parameterLengths long enough?
	if (*numParameters > maxParameters) return false;
	// Is there enough data to read all the stated parameters?
	if (readOffset+*numParameters*sizeof(unsigned int) > inLength) return false;
	// Read out all parameters lengths, write to parameterLengths
	for (parameterIndex=0; parameterIndex < *numParameters; parameterIndex++)
	{
		memcpy(parameterLengths+parameterIndex, in+readOffset, sizeof(unsigned int));
#ifndef __BITSTREAM_NATIVE_END
		if (RakNet::BitStream::DoEndianSwap())
			RakNet::BitStream::ReverseBytesInPlace((unsigned char*)(parameterLengths+parameterIndex),sizeof(unsigned int));
#endif
		readOffset+=sizeof(unsigned int);

		isFloatOrDouble[parameterIndex]=in[readOffset] & 1;
#ifndef __BITSTREAM_NATIVE_END
		endianSwap[parameterIndex]=in[readOffset] & 2;
#endif
		readOffset+=1;

	}
	// Read out each parameter, unpacking, rounding up to REGISTER_MIN bytes
	for (parameterIndex=0; parameterIndex < *numParameters; parameterIndex++)
	{
		alignedBytesRequired = parameterLengths[parameterIndex];
		if (alignedBytesRequired % MIN_FUNC_STACK_ALIGNMENT!=0)
		{
			// Not already divisible by REGISTER_MIN, round up to nearest multiple of REGISTER_MIN
			alignedBytesRequired+=MIN_FUNC_STACK_ALIGNMENT-(alignedBytesRequired % MIN_FUNC_STACK_ALIGNMENT);
		}
		if (outLength < *bytesWritten + alignedBytesRequired )
			return false;
#ifndef __BITSTREAM_NATIVE_END
		if (endianSwap[parameterIndex])
			RakNet::BitStream::ReverseBytesInPlace((unsigned char*)(in+readOffset),parameterLengths[parameterIndex]);
#endif
		if (RakNet::BitStream::IsBigEndian())
		{
			// Write data to rightmost
			memcpy(out+*bytesWritten+alignedBytesRequired-parameterLengths[parameterIndex],in+readOffset,parameterLengths[parameterIndex]);
			// Fill out left with 0
			memset(out+*bytesWritten,0,alignedBytesRequired-parameterLengths[parameterIndex]);
		}
		else
		{
			// Write data to leftmost
			memcpy(out+*bytesWritten,in+readOffset,parameterLengths[parameterIndex]);
			// Fill out right with 0
			memset(out+*bytesWritten+parameterLengths[parameterIndex],0,alignedBytesRequired-parameterLengths[parameterIndex]);
		}
		readOffset+=parameterLengths[parameterIndex];
		*bytesWritten+=alignedBytesRequired;
	}
	return true;
}
void AutoRPC::CallWithStack(const char *inputStack, unsigned int numBytes, void *functionPtr, void *lastParam, void *thisPtr)
{
	// Are we x86-32?
#if defined(__i386__) || defined( _M_IX86 ) || defined( __INTEL__ )
#if defined(__GNUC__)

	// GCC has its own form of asm block - so we'll always have to write two versions.
	// Therefore, as we're writing it twice, we use the ATT dialect, because only later
	// gcc support Intel dialect.
	asm (\
		"pushl  %0\n\
		sub    %%ecx,%%esp\n\
		mov    %2,%%esi\n\
		shr    $0x2,%%ecx\n\
		mov    %%esp,%%edi\n\
		rep movsl %%ds:(%%esi),%%es:(%%edi)\n\
		test   %%eax,%%eax\n\
		jz     AutoRPC__GNUC__1\n\
		push   %%eax\n\
		AutoRPC__GNUC__1: call   *%4\n\
		lea    0x4(%%edi),%%esp"\
		: /* no outputs */\
		: "m" (lastParam), "c" (numBytes), "m" ( inputStack ), "a" (thisPtr), "m" (functionPtr)\
		: "edi" , "esi", "edx"\
		);
#elif defined(_WIN32)
	// Intel dialect assembly
	ASSEMBLY_BLOCK
	{
		// make lastParam is last param on stack
		push        lastParam

			// Load numbytes.
			mov         ecx,numBytes

			// allocate space on the stack
			sub         esp,ecx

			// Adjust ecx to be the dword count - movsd is more efficent than movsb
			shr         ecx,2

			// Setup the source of copy: the input "stack"
			mov         esi,inputStack

			// Setup the destination of the copy: the return stack.
			mov         edi,esp

			// copy data
			rep movs    dword ptr es:[edi],dword ptr [esi]

			// Registers are the L0 cache - so load thisPtr into eax, where we can examine
			// and, if necessary, push it.
			mov eax,    thisPtr

			// If thisPtr (eax) is not zero then push it as first arg.
			test        eax,eax
			jz          AutoRPC1_WIN32
			push        eax
AutoRPC1_WIN32:
		// call the function
		call        functionPtr

			// Restore the stack to its state, prior to our invocation.
			//
			// Detail: edi is one of the registers that must be preserved
			// across function calls. (THe compiler should be saving it for us.)
			//
			// We left edi pointing to the end of the block copied; i.e. the state
			// of the stack prior to copying our params.  So by loading it
			// into the esp we can restore the return stack to the state prior
			// to the copy.
			//
			// However we also pushed lastParam, so we need to add
			// sizeof(dword) to the stack - which we do in a single op.
			//
			// (And while it may go against every instinct in a programmers
			// body to hard code sizeof(dword) - truth is, this code will
			// only work on 32 bit modes. The register names are different
			// for 16 bit and 64 bit processors; the shr instruction above
			// is also hard coded.  And the parameter passing conventions
			// are different.  So hard coding word sizes is fine in asm.)
			//
			// Doing it this way means we can be used for both stdcall
			// (callee pops) and cdecl (caller pops).
			//
			lea         esp,dword ptr 4[edi]
	};
#endif // GNUC vs non GNUC

#else   // x86-32

	#pragma message("-- RakNet: AutoRPC::CallWithStack not supported on this platform. --")

#endif // x86-32
}
void AutoRPC::SendError(SystemAddress target, unsigned char errorCode)
{
	RakNet::BitStream bs;
	bs.Write((MessageID)ID_RPC_REMOTE_ERROR);
	bs.Write(errorCode);
	rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, target, false);
}
#ifdef _MSC_VER
#pragma warning(disable:4100)   // warning C4100: 'peer' : unreferenced formal parameter
#endif
void AutoRPC::OnShutdown(RakPeerInterface *peer)
{
	Clear();
}
void AutoRPC::Clear(void)
{
	unsigned i,j;
	for (j=0; j < remoteFunctions.Size(); j++)
	{
		DataStructures::OrderedList<RPCIdentifier, RemoteRPCFunction, AutoRPC::RemoteRPCFunctionComp> *theList = remoteFunctions[j];
		for (i=0; i < theList->Size(); i++)
		{
			if (theList->operator [](i).identifier.uniqueIdentifier)
				rakFree(theList->operator [](i).identifier.uniqueIdentifier);
		}
		delete theList;
	}
	remoteFunctions.Clear();
	outgoingExtraData.Reset();
	incomingExtraData.Reset();
}
unsigned AutoRPC::GetLocalFunctionIndex(AutoRPC::RPCIdentifier identifier)
{
	unsigned i;
	for (i=0; i < localFunctions.Size(); i++)
	{
		if (localFunctions[i].identifier.isObjectMember==identifier.isObjectMember &&
			strcmp(localFunctions[i].identifier.uniqueIdentifier,identifier.uniqueIdentifier)==0)
			return i;
	}
	return (unsigned) -1;
}
bool AutoRPC::GetRemoteFunctionIndex(SystemAddress systemAddress, AutoRPC::RPCIdentifier identifier, unsigned int *outerIndex, unsigned int *innerIndex)
{
	bool objectExists=false;
	if (remoteFunctions.Has(systemAddress))
	{
		*outerIndex = remoteFunctions.GetIndexAtKey(systemAddress);
		DataStructures::OrderedList<RPCIdentifier, RemoteRPCFunction, AutoRPC::RemoteRPCFunctionComp> *theList = remoteFunctions[*outerIndex];
		*innerIndex = theList->GetIndexFromKey(identifier, &objectExists);
	}
	return objectExists;
}
void AutoRPC::SerializeHeader(char *out, unsigned int numParams, unsigned int *writeOffset)
{
	out[*writeOffset]=(char) numParams;
	*writeOffset+=sizeof(unsigned char);
}
#ifdef _MSC_VER
#pragma warning(disable:4100)   // warning C4100: 'peer' : unreferenced formal parameter
#endif
void AutoRPC::SerializeParamHeader(char *out, unsigned int paramLength, unsigned int *writeOffset, bool isFloatOrDouble, bool endianSwap)
{
	memcpy(out+*writeOffset, &paramLength, sizeof(unsigned int) );
#ifndef __BITSTREAM_NATIVE_END
	if (RakNet::BitStream::IsNetworkOrder()==false)
		RakNet::BitStream::ReverseBytesInPlace((unsigned char*) (out+*writeOffset),sizeof(unsigned int));
#endif
	*writeOffset+=sizeof(unsigned int);

	*(out+*writeOffset)=0;
	if (isFloatOrDouble==true)
		*(out+*writeOffset) |= 1;
	if (endianSwap==true)
		*(out+*writeOffset) |= 2;
	*writeOffset+=1;

}

void AutoRPC::SerializeParamData(char *out, void *paramData, unsigned int paramLength, unsigned int *writeOffset, bool endianSwap)
{
	memcpy(out+*writeOffset,paramData,paramLength);
#ifndef __BITSTREAM_NATIVE_END
	if (endianSwap && RakNet::BitStream::IsNetworkOrder()==false)
		RakNet::BitStream::ReverseBytesInPlace((unsigned char *) (out+*writeOffset),paramLength);
#endif
	*writeOffset+=paramLength;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

