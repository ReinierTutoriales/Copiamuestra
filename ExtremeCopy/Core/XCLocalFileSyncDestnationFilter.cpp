/****************************************************************************

Copyright (c) 2008-2020 Kevin Wu (Wu Feng)

github: https://github.com/kevinwu1024/ExtremeCopy
site: http://www.easersoft.com

Licensed under the Apache License, Version 2.0 License (the "License"); you may not use this file except
in compliance with the License. You may obtain a copy of the License at

https://opensource.org/licenses/Apache-2.0

****************************************************************************/

#include "StdAfx.h"
#include "XCLocalFileSyncDestnationFilter.h"
#include "XCCopyingEvent.h"
#include "..\Common\ptGlobal.h"
#include "XCFileDataBuffer.h"
#include "..\Common\ptPerformanceCalcator.h"

CXCLocalFileSyncDestnationFilter::CXCLocalFileSyncDestnationFilter(CXCCopyingEvent* pEvent,const CptString strDestRoot,const SStorageInfoOfFile& siof,
	const bool bIsRenameCopy):CXCLocalFileDestnationFilter(pEvent,strDestRoot,siof,bIsRenameCopy)
{
}


CXCLocalFileSyncDestnationFilter::~CXCLocalFileSyncDestnationFilter(void)
{
	//Release_Printf(_T("~CXCLocalFileSyncDestnationFilter()")) ;
}

bool CXCLocalFileSyncDestnationFilter::OnContinue() 
{
	return CXCLocalFileDestnationFilter::OnContinue() ;
}

bool CXCLocalFileSyncDestnationFilter::OnPause()
{
	return CXCLocalFileDestnationFilter::OnPause() ;
}

void CXCLocalFileSyncDestnationFilter::OnStop()
{
	CXCLocalFileDestnationFilter::OnStop() ;
}

bool CXCLocalFileSyncDestnationFilter::OnInitialize() 
{
	m_nCreateFileFlag = FILE_FLAG_SEQUENTIAL_SCAN ;

	return CXCLocalFileDestnationFilter::OnInitialize() ;
}


int CXCLocalFileSyncDestnationFilter::WriteFileData(SDataPack_FileData& fd)
{
	int nRet = 0 ;

	if(!this->IsValideRunningState())
	{
		return ErrorHandlingFlag_Exit ;
	}

	_ASSERT(!m_FileInfoList.empty()) ;

	if(m_CurFileIterator==m_FileInfoList.end())
	{
		m_CurFileIterator = m_FileInfoList.begin() ;
	}

	_ASSERT(fd.uFileID>=(*m_CurFileIterator).pSfi->uFileID) ;

	while((*m_CurFileIterator).pSfi->nFileSize==0 || (*m_CurFileIterator).pSfi->uFileID<fd.uFileID)
	{
		++m_CurFileIterator ;
	}

	_ASSERT((*m_CurFileIterator).pSfi->uFileID==fd.uFileID) ;
	_ASSERT(m_CurFileIterator!=m_FileInfoList.end()) ;

	if((*m_CurFileIterator).pSfi->IsDiscard())
	{
		_ASSERT(fd.pData!=NULL) ;
		_ASSERT(fd.uFileID>0) ;
		m_pFileDataBuf->Free(fd.pData,fd.nBufSize) ;
		return 0 ;
	}

	_ASSERT(!(*m_CurFileIterator).pSfi->IsDiscard()) ;

	DWORD nSize = (DWORD)fd.nDataSize ;
	if((*m_CurFileIterator).bNoBuf)
	{
		nSize = (DWORD)ALIGN_SIZE_UP(fd.nDataSize,m_StorageInfo.nSectorSize) ;
	}

	DWORD nWrittenTotal = 0 ;
	DWORD nSystemError = ERROR_SUCCESS ;

EXCEPTION_RETRY_WRITEDATA:
	while(nWrittenTotal<nSize)
	{
		DWORD dwWrite = 0 ;
		DWORD nWriteSize = nSize-nWrittenTotal ;
		BYTE* pWriteData = ((BYTE*)fd.pData)+nWrittenTotal ;

#ifdef COMPILE_TEST_PERFORMANCE
		DWORD dw = CptPerformanceCalcator::GetInstance()->BeginCal() ;
#endif

		BOOL b = ::WriteFile((*m_CurFileIterator).hFile,pWriteData,nWriteSize,&dwWrite,NULL) ;

#ifdef COMPILE_TEST_PERFORMANCE
		CptPerformanceCalcator::GetInstance()->EndCalAndSave(dw,2) ;
#endif

		if(!this->IsValideRunningState())
		{
			return ErrorHandlingFlag_Exit ;
		}

		if(!b)
		{
			nSystemError = ::GetLastError() ;
			break ;
		}

		if(dwWrite==0)
		{
			nSystemError = ERROR_WRITE_FAULT ;
			break ;
		}

		nWrittenTotal += dwWrite ;

		if((*m_CurFileIterator).bNoBuf && nWrittenTotal<nSize)
		{
			LARGE_INTEGER liMove ;
			liMove.QuadPart = -(LONGLONG)nWrittenTotal ;
			if(!::SetFilePointerEx((*m_CurFileIterator).hFile,liMove,NULL,FILE_CURRENT))
			{
				m_pFileDataBuf->Free(fd.pData,fd.nBufSize) ;
				*this->m_pRunningState = CFS_ReadyStop ;
				return ErrorHandlingFlag_Exit ;
			}

			nWrittenTotal = 0 ;
			nSystemError = ERROR_WRITE_FAULT ;
			break ;
		}
	}

	if(nWrittenTotal==nSize)
	{
		m_pFileDataBuf->Free(fd.pData,fd.nBufSize) ;

		_ASSERT((*m_CurFileIterator).uRemainSize>=nSize) ;
		(*m_CurFileIterator).uRemainSize -= nSize ;

		if(m_pEvent!=NULL && this->CanCallbackFileInfo())
		{
			SFileDataOccuredInfo fdoi ;
			fdoi.bReadOrWrite = false ;
			fdoi.nDataSize = fd.nDataSize ;
			fdoi.uFileID = (*m_CurFileIterator).pSfi->uFileID ;
			m_pEvent->XCOperation_FileDataOccured(fdoi) ;
		}

		if((*m_CurFileIterator).uRemainSize==0)
		{
			while((++m_CurFileIterator)!=m_FileInfoList.end() && (*m_CurFileIterator).pSfi->nFileSize==0)
			{
				NULL ;
			}
		}
	}
	else
	{
		Debug_Printf(_T("CXCLocalFileDestnationFilter::WriteFileData() failed")) ;

		if(m_pEvent!=NULL)
		{
			SXCExceptionInfo ei ;
			ei.uFileID =(*m_CurFileIterator).pSfi->uFileID ;
			ei.ErrorCode.nSystemError = nSystemError ;
			ei.SupportType = ErrorHandlingFlag_RetryIgnoreCancel ;
			ei.strDstFile = (*m_CurFileIterator).strFileName ;
			ei.strSrcFile = (*m_CurFileIterator).pSfi->strSourceFile ;

			ErrorHandlingResult result = m_pEvent->XCOperation_CopyExcetption(ei) ;

			switch(result)
			{
			case ErrorHandlingFlag_Ignore:
				{
					::CloseHandle((*m_CurFileIterator).hFile) ;
					(*m_CurFileIterator).hFile = INVALID_HANDLE_VALUE ;
					::DeleteFile((*m_CurFileIterator).strFileName) ;
					(*m_CurFileIterator).pSfi->SetDiscard(true) ;
					m_pFileDataBuf->Free(fd.pData,fd.nBufSize) ;

					m_pEvent->XCOperation_FileDiscard((*m_CurFileIterator).pSfi,(*m_CurFileIterator).uRemainSize) ;
					++m_CurFileIterator ;
					m_pEvent->XCOperation_RecordError(ei) ;
					nRet = ErrorHandlingFlag_Ignore ;
				}
				break ;

			case ErrorHandlingFlag_Retry:
				goto EXCEPTION_RETRY_WRITEDATA ;

			default:
			case ErrorHandlingFlag_Exit:
				::CloseHandle((*m_CurFileIterator).hFile) ;
				(*m_CurFileIterator).hFile = INVALID_HANDLE_VALUE ;
				::DeleteFile((*m_CurFileIterator).strFileName) ;
				m_pFileDataBuf->Free(fd.pData,fd.nBufSize) ;
				*this->m_pRunningState = CFS_ReadyStop ;
				return ErrorHandlingFlag_Exit ;
			}
		}
		else
		{
			m_pFileDataBuf->Free(fd.pData,fd.nBufSize) ;
			*this->m_pRunningState = CFS_ReadyStop ;
			return ErrorHandlingFlag_Exit ;
		}
	}

	return nRet ;
}

void CXCLocalFileSyncDestnationFilter::OnLinkEnded(pt_STL_list(SDataPack_SourceFileInfo*)& FileList) 
{
	if(!FileList.empty())
	{
		// 因为是异步处理，当LinkEnded命令发出时，可能有文件未被确认释放（事实上，命令发出后一瞬间已确认释放了），
		// 所以就会出现 FileList 有文件ID，但却已释放了的情况
		if(!m_FileInfoList.empty())
		{
			this->RoundOffFile(FileList) ;
		}
	}
}
