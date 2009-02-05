/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/usb/IOUSBLog.h>

#include "AppleEHCItdMemoryBlock.h"

#define super OSObject
OSDefineMetaClassAndStructors(AppleEHCItdMemoryBlock, OSObject);

AppleEHCItdMemoryBlock*
AppleEHCItdMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCItdMemoryBlock					*me = new AppleEHCItdMemoryBlock;
    IOByteCount								len;
	IODMACommand							*dmaCommand = NULL;
	UInt64									offset = 0;
	IODMACommand::Segment32					segments;
	UInt32									numSegments = 1;
	IOReturn								status = kIOReturnSuccess;
    EHCIGeneralTransferDescriptorSharedPtr	sharedPtr;
    IOPhysicalAddress						sharedPhysical;
    UInt32									i;
    
    if (me)
	{
		// Use IODMACommand to get the physical address
		dmaCommand = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, PAGE_SIZE, (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
		if (!dmaCommand)
		{
			USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock - could not create IODMACommand");
			return NULL;
		}
		USBLog(6, "AppleEHCItdMemoryBlock::NewMemoryBlock - got IODMACommand %p", dmaCommand);
		
		// allocate one page on a page boundary below the 4GB line
		me->_buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryUnshared | kIODirectionInOut, kEHCIPageSize, kEHCIStructureAllocationPhysicalMask);
		
		// allocate exactly one physical page
		if (me->_buffer) 
		{
			status = me->_buffer->prepare();
			if (status)
			{
				USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock - could not prepare buffer");
				me->_buffer->release();
				me->release();
				dmaCommand->release();
				return NULL;
			}
			sharedPtr = (EHCIGeneralTransferDescriptorSharedPtr)me->_buffer->getBytesNoCopy();
			bzero(sharedPtr, kEHCIPageSize);
			status = dmaCommand->setMemoryDescriptor(me->_buffer);
			if (status)
			{
				USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock - could not set memory descriptor");
				me->_buffer->complete();
				me->_buffer->release();
				me->release();
				dmaCommand->release();
				return NULL;
			}
			status = dmaCommand->gen32IOVMSegments(&offset, &segments, &numSegments);
			dmaCommand->clearMemoryDescriptor();
			dmaCommand->release();
			if (status || (numSegments != 1) || (segments.fLength != kEHCIPageSize))
			{
				USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock - could not get physical segment");
				me->_buffer->complete();
				me->_buffer->release();
				me->release();
				return NULL;
			}
			sharedPhysical = segments.fIOVMAddr;
			for (i=0; i < TDsPerBlock; i++)
			{
				me->_TDs[i].pPhysical = sharedPhysical+(i * sizeof(EHCIGeneralTransferDescriptorShared));
				me->_TDs[i].pShared = &sharedPtr[i];
			}
		}
		else
		{
			USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock, could not allocate buffer!");
			me->release();
			me = NULL;
		}
	}
	else
	{
		USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock, constructor failed!");
    }
    return me;
}



UInt32
AppleEHCItdMemoryBlock::NumTDs(void)
{
    return TDsPerBlock;
}



EHCIGeneralTransferDescriptorPtr
AppleEHCItdMemoryBlock::GetTD(UInt32 index)
{
    return (index < TDsPerBlock) ? &_TDs[index] : NULL;
}



AppleEHCItdMemoryBlock*
AppleEHCItdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCItdMemoryBlock::SetNextBlock(AppleEHCItdMemoryBlock* next)
{
    _nextBlock = next;
}
