/****************************************************************************

Copyright (c) 2008-2020 Kevin Wu (Wu Feng)

github: https://github.com/kevinwu1024/ExtremeCopy
site: http://www.easersoft.com

Licensed under the Apache License, Version 2.0 License (the "License"); you may not use this file except
in compliance with the License. You may obtain a copy of the License at

https://opensource.org/licenses/Apache-2.0

****************************************************************************/

#include "StdAfx.h"
#include "XCFileDataBuffer.h"
#include "..\Common\ptGlobal.h"
#include "../Common/ptDebugView.h"
#include <set>


CXCFileDataBuffer::CXCFileDataBuffer(void):m_pBlockBuf(NULL),m_pRefCountBuf(NULL)
	,m_nCurAllocIndex(0),m_nChunkSize(32*1024*1024),m_nRefPageCount(0)
{
#ifdef _DEBUG
	m_nIDCount = 0 ;
	m_nLastFreeID = 0 ;
	m_pAllocIDRecordBuf = NULL ;
#endif
}


CXCFileDataBuffer::~CXCFileDataBuffer(void)
{
	this->Release() ;
}

void CXCFileDataBuffer::ResetRef() 
{
	m_nRefPageCount = 0 ;
	m_nCurAllocIndex = 0 ;

	::memset(m_pRefCountBuf,0,m_nRefNum) ;

}

void CXCFileDataBuffer::Release() 
{
	CptAutoLock lcok(&m_Lock) ;

	if(m_pBlockBuf!=NULL)
	{
		::VirtualUnlock(m_pBlockBuf,m_nChunkSize) ;
		::VirtualFree(m_pBlockBuf,0,MEM_RELEASE) ;
		//::free(m_pBlockBuf) ;
		m_pBlockBuf = NULL ;
	}

	if(m_pRefCountBuf!=NULL)
	{
		::free(m_pRefCountBuf) ;
		m_pRefCountBuf = NULL ;
	}

	_ASSERTE( _CrtCheckMemory( ) );
}

bool CXCFileDataBuffer::IsAllocateChunk() const 
{
	return (m_pBlockBuf!=NULL) ;
}

bool CXCFileDataBuffer::AllocateChunk(int nChunkSize,int nAlignSize)
{
	bool bRet = false ;

	this->Release() ;

	CptAutoLock lcok(&m_Lock) ;

	if(nAlignSize<=0 || nAlignSize%512)
	{
		nAlignSize = ALIGN_SIZE_UP(nAlignSize,1024) ;
	}

	m_nPageSize = nAlignSize ;

	switch(nChunkSize)
	{
	case (2*1024*1024):
	case (4*1024*1024):
	case (8*1024*1024):
	case (32*1024*1024):
		break ;

	default:
	case (16*1024*1024):
		nChunkSize = 16*1024*1024 ;
		break ;

	}

	//if(nChunkSize<1024*1024)
	//{
	//	nChunkSize = 32*1024*1024 ;
	//}

	do
	{
		m_pBlockBuf = (BYTE*)::VirtualAlloc(NULL,nChunkSize,MEM_COMMIT,PAGE_READWRITE) ;

		if(m_pBlockBuf==NULL)
		{
			nChunkSize = (nChunkSize)>>1 ;
		}
	}
	while((m_pBlockBuf==NULL) && nChunkSize>4*1024);

	if(m_pBlockBuf!=NULL)
	{
		::VirtualLock(m_pBlockBuf,nChunkSize) ; // keep resident for performance

		m_nRefNum = ALIGN_SIZE_DOWN(nChunkSize,m_nPageSize)/m_nPageSize ;
		m_pRefCountBuf = (BYTE*)::malloc(m_nRefNum) ;

#ifdef _DEBUG
		m_pAllocIDRecordBuf = (unsigned*)::malloc(m_nRefNum*sizeof(unsigned)) ;
		::memset(m_pAllocIDRecordBuf,0,m_nRefNum*sizeof(unsigned)) ;
#endif
		m_nCurAllocIndex = 0 ;

		bRet = (m_pRefCountBuf!=NULL) ;

		if(bRet)
		{
			::memset(m_pBlockBuf,0,nChunkSize) ;
			::memset(m_pRefCountBuf,0,m_nRefNum) ;
		}
	}

	_ASSERTE( _CrtCheckMemory( ) );

	m_nChunkSize = nChunkSize ;

	return bRet ;
}

int CXCFileDataBuffer::GetChunkSize() const 
{
	return m_nChunkSize ;
}


bool CXCFileDataBuffer::IsEmpty()  
{
	CptAutoLock lock(&m_Lock) ;

	return m_nRefPageCount==0 ;
}

bool CXCFileDataBuffer::IsFull()  
{
	CptAutoLock lock(&m_Lock) ;

	return m_nRefPageCount==m_nRefNum ;
}

int CXCFileDataBuffer::GetRemainSpace() 
{
	CptAutoLock lcok(&m_Lock) ;

	return (m_nRefNum - m_nRefPageCount)*m_nPageSize ;
}

int CXCFileDataBuffer::GetBottomRemainSpace() 
{
	int nRet = 0 ;

	CptAutoLock lcok(&m_Lock) ;

	int nIndex = m_nCurAllocIndex ;

	while(nIndex<m_nRefNum && m_pRefCountBuf[nIndex]>0)
	{
		++nIndex ;
	}

	if(nIndex>=m_nRefNum)
	{
		nRet = 0 ;
	}
	else
	{
		int nIndex2 = nIndex+1 ;

		while(nIndex2<m_nRefNum && m_pRefCountBuf[nIndex2]==0)
		{
			++nIndex2 ;
		}

		nRet = (nIndex2 - nIndex)*m_nPageSize ;
	}

	return nRet ;
}
#ifdef _DEBUG
void CXCFileDataBuffer::CheckBufAlloc() 
{
	int nNonSwitchZero = 0 ;
	int nZeroSwitchNon = 0 ;

	for(int i=1;i<m_nRefNum;++i)
	{
		if(m_pRefCountBuf[i-1]!=m_pRefCountBuf[i])
		{
			if(m_pRefCountBuf[i])
			{
				++nZeroSwitchNon ;
			}
			else
			{
				++nNonSwitchZero ;
			}

			if(nZeroSwitchNon>=2)
			{
				_ASSERT(FALSE) ;
			}
		}
	}
}
#endif

void CXCFileDataBuffer::Free(void* pBuf,int nSize) 
{
#ifdef COMPILE_TEST_PERFORMANCE
	DWORD dw = CptPerformanceCalcator::GetInstance()->BeginCal() ;
#endif

	_ASSERT(m_pBlockBuf!=NULL) ;
	_ASSERT(m_pRefCountBuf!=NULL) ;
	_ASSERT(pBuf!=NULL) ;

	const int nOffset = (int)((BYTE*)pBuf - m_pBlockBuf)  ;
	_ASSERT(nOffset%m_nPageSize ? FALSE : TRUE) ;
	const int nIndex = nOffset/m_nPageSize ;
	_ASSERT(m_nRefNum>nIndex) ;
	const int nPageCount = nSize/m_nPageSize + (nSize%m_nPageSize ? 1 : 0) ;

	Debug_Printf(_T("CXCFileDataBuffer::Free() free_page_count=%d remain=%d pBuf=%p nSize=%d"),nPageCount,m_nRefNum-m_nRefPageCount, pBuf, nSize) ;

	{
		CptAutoLock lcok(&m_Lock) ;
		for(int i=0;i<nPageCount;++i)
		{
			_ASSERT(m_pRefCountBuf[nIndex+i]>0) ;
			if(--m_pRefCountBuf[nIndex+i]==0)
			{
				Debug_Printf(_T("free page index=%d "), nIndex + i);
				--m_nRefPageCount ;
#ifdef _DEBUG
				_ASSERT(m_pAllocIDRecordBuf[nIndex+i]>0) ;
				m_nLastFreeID = m_pAllocIDRecordBuf[nIndex+i] ;
				m_pAllocIDRecordBuf[nIndex+i]=0 ;
#endif
			}
		}
		_ASSERT(!(m_nRefPageCount<0 || m_nRefPageCount>m_nRefNum)) ;
		_ASSERTE( _CrtCheckMemory() );
	}

#ifdef COMPILE_TEST_PERFORMANCE
	CptPerformanceCalcator::GetInstance()->EndCalAndSave(dw,10) ;
#endif
}

void* CXCFileDataBuffer::Allocate(BYTE nRefCount,int nSize)
{
	_ASSERT(nSize<=m_nChunkSize) ;
	const int nPageCount = nSize/m_nPageSize + (nSize%m_nPageSize ? 1 : 0) ;
	CptAutoLock lcok(&m_Lock) ;

	if(m_nRefNum-m_nRefPageCount<nPageCount)
	{
		return NULL ;
	}

#ifdef COMPILE_TEST_PERFORMANCE
	DWORD dw = CptPerformanceCalcator::GetInstance()->BeginCal() ;
#endif

	while(m_nCurAllocIndex<m_nRefNum && m_pRefCountBuf[m_nCurAllocIndex]>0)
	{
		++m_nCurAllocIndex ;
	}

	if(m_nCurAllocIndex+nPageCount>m_nRefNum)
	{
		m_nCurAllocIndex = 0 ;
		if(m_pRefCountBuf[m_nCurAllocIndex]>0)
		{
#ifdef COMPILE_TEST_PERFORMANCE
			CptPerformanceCalcator::GetInstance()->EndCalAndSave(dw,9) ;
#endif
			return NULL ;
		}
	}

	for(int i=1;i<nPageCount;++i)
	{
		if(m_pRefCountBuf[m_nCurAllocIndex+i])
		{
#ifdef COMPILE_TEST_PERFORMANCE
			CptPerformanceCalcator::GetInstance()->EndCalAndSave(dw,9) ;
#endif
			return NULL ;
		}
	}

	void* pRet = m_pBlockBuf + (m_nCurAllocIndex * m_nPageSize);
#ifdef _DEBUG
	++m_nIDCount ;
	for(int i=0;i<nPageCount;++i)
	{
		m_pAllocIDRecordBuf[i+m_nCurAllocIndex] = m_nIDCount ;
	}
#endif
	::memset(m_pRefCountBuf+m_nCurAllocIndex,nRefCount,nPageCount) ;
	m_nRefPageCount += nPageCount ;
	Debug_Printf(_T("alloc page. cur_alloc=%d  total_alloc=%d pBuf=%p nSize=%d"),nPageCount,m_nRefPageCount, pRet, nSize) ;
	_ASSERT(!(m_nRefPageCount<0 || m_nRefPageCount>m_nRefNum)) ;
	m_nCurAllocIndex += nPageCount ;
	_ASSERTE( _CrtCheckMemory( ) );
#ifdef COMPILE_TEST_PERFORMANCE
	CptPerformanceCalcator::GetInstance()->EndCalAndSave(dw,9) ;
#endif
	return pRet ;
}

int CXCFileDataBuffer::GetPageSize() const 
{
	return m_nPageSize ;
}