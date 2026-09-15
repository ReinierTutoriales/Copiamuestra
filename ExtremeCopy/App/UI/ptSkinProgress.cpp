/****************************************************************************

Copyright (c) 2008-2020 Kevin Wu (Wu Feng)

github: https://github.com/kevinwu1024/ExtremeCopy
site: http://www.easersoft.com

Licensed under the Apache License, Version 2.0 License (the "License"); you may not use this file except
in compliance with the License. You may obtain a copy of the License at

https://opensource.org/licenses/Apache-2.0

****************************************************************************/

#include "StdAfx.h"
#include "ptSkinProgress.h"
#include "..\XCGlobal.h"

CptSkinProgress::CptSkinProgress(void):m_nMaxValue(100),m_nMinValue(0),m_nCurValue(0),m_hBarBitmap(NULL),m_hResultBufBitmap(NULL)
{
	m_hPercentFont = ::CreateFont(11,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,
		CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,_T("Segoe UI")) ;
}

CptSkinProgress::~CptSkinProgress(void)
{
	if(m_hBarBitmap!=NULL)
	{
		::DeleteObject(m_hBarBitmap) ;
		m_hBarBitmap =NULL ;
	}

	if(m_hResultBufBitmap!=NULL)
	{
		::DeleteObject(m_hResultBufBitmap) ;
		m_hResultBufBitmap = NULL ;
	}

	SAFE_DELETE_GDI(m_hPercentFont) ;
}

void CptSkinProgress::Draw(HDC hDC) 
{
	_ASSERT(hDC!=NULL) ;

	const int nWidth = m_Rect.GetWidth() ;
	const int nHeight = m_Rect.GetHeight() ;
	if(nWidth<=0 || nHeight<=0)
		return ;

	RECT rtTrack = {m_Rect.nLeft,m_Rect.nTop,m_Rect.nLeft+nWidth,m_Rect.nTop+nHeight} ;
	const int nRadius = max(2,min(nHeight/2,4)) ;

	// Flat system-aware track replaces the legacy bitmap strip while keeping
	// the exact existing progress rectangle and public control API.
	HBRUSH hTrackBrush = ::CreateSolidBrush(::GetSysColor(COLOR_3DFACE)) ;
	HPEN hBorderPen = ::CreatePen(PS_SOLID,1,::GetSysColor(COLOR_3DSHADOW)) ;
	HBRUSH hOldBrush = (HBRUSH)::SelectObject(hDC,hTrackBrush) ;
	HPEN hOldPen = (HPEN)::SelectObject(hDC,hBorderPen) ;
	::RoundRect(hDC,rtTrack.left,rtTrack.top,rtTrack.right,rtTrack.bottom,nRadius,nRadius) ;
	::SelectObject(hDC,hOldPen) ;
	::SelectObject(hDC,hOldBrush) ;
	::DeleteObject(hBorderPen) ;
	::DeleteObject(hTrackBrush) ;

	const int nRange = m_nMaxValue-m_nMinValue ;
	int nValidLen = 0 ;
	if(nRange>0)
		nValidLen = ::MulDiv(m_nCurValue-m_nMinValue,nWidth,nRange) ;

	if(nValidLen>0)
	{
		RECT rtFill = rtTrack ;
		rtFill.right = min(rtTrack.right,rtTrack.left+nValidLen) ;
		HBRUSH hFillBrush = ::CreateSolidBrush(::GetSysColor(COLOR_HIGHLIGHT)) ;
		HPEN hFillPen = ::CreatePen(PS_SOLID,1,::GetSysColor(COLOR_HIGHLIGHT)) ;
		hOldBrush = (HBRUSH)::SelectObject(hDC,hFillBrush) ;
		hOldPen = (HPEN)::SelectObject(hDC,hFillPen) ;
		::RoundRect(hDC,rtFill.left,rtFill.top,rtFill.right,rtFill.bottom,nRadius,nRadius) ;
		::SelectObject(hDC,hOldPen) ;
		::SelectObject(hDC,hOldBrush) ;
		::DeleteObject(hFillPen) ;
		::DeleteObject(hFillBrush) ;
	}

	HFONT hOldFont = (HFONT)::SelectObject(hDC,m_hPercentFont) ;
	const int nOldBkMode = ::SetBkMode(hDC,TRANSPARENT) ;
	const COLORREF oldTextColor = ::SetTextColor(hDC,::GetSysColor(COLOR_WINDOWTEXT)) ;
	CptString strPercent ;
	const int nPercent = nRange>0 ? ::MulDiv(m_nCurValue-m_nMinValue,100,nRange) : 0 ;
	strPercent.Format(_T("%d%%"),nPercent) ;
	::DrawText(hDC,strPercent.c_str(),strPercent.GetLength(),m_Rect.GetRECTPointer(),DT_CENTER|DT_VCENTER|DT_SINGLELINE) ;
	::SetTextColor(hDC,oldTextColor) ;
	::SetBkMode(hDC,nOldBkMode) ;
	::SelectObject(hDC,hOldFont) ;
}

void CptSkinProgress::Draw()
{
	if(m_hParentWnd!=NULL && ::IsWindowVisible(m_hParentWnd))
	{
		HDC hDC = ::GetDC(m_hParentWnd) ;

		if(NULL!=hDC)
		{
			this->Draw(hDC) ;

			::ReleaseDC(m_hParentWnd,hDC) ;
		}
	}
}

void CptSkinProgress::SetParent(HWND hWnd)
{
	m_hParentWnd = hWnd ;
}

bool CptSkinProgress::SetRange(int nMax,int nMin)
{
	bool bRet = false ;

	if(nMax>nMin)
	{
		m_nMaxValue=nMax;
		m_nMinValue=nMin;

		if(m_nCurValue<m_nMinValue)
			m_nCurValue=m_nMinValue;
		else if(m_nCurValue>m_nMaxValue)
			m_nCurValue=m_nMaxValue;

		this->Draw() ;

		bRet = true ;
	}

	return bRet ;
}

void CptSkinProgress::SetValue(int nValue)
{
	if(nValue<m_nMinValue)
	{
		nValue = m_nMinValue ;
	}
	else if(nValue>m_nMaxValue)
	{
		nValue = m_nMaxValue ;
	}

	m_nCurValue = nValue ;
	this->Draw() ;
}

void CptSkinProgress::SetRectangle(const SptRect& rt)
{
	m_Rect = rt ;

	if(m_hResultBufBitmap!=NULL)
	{
		::DeleteObject(m_hResultBufBitmap) ;
		m_hResultBufBitmap = NULL ;
	}

	this->Draw() ;
}
