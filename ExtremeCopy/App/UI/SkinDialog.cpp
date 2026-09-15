/****************************************************************************

Copyright (c) 2008-2020 Kevin Wu (Wu Feng)

github: https://github.com/kevinwu1024/ExtremeCopy
site: http://www.easersoft.com

Licensed under the Apache License, Version 2.0 License (the "License"); you may not use this file except
in compliance with the License. You may obtain a copy of the License at

https://opensource.org/licenses/Apache-2.0

****************************************************************************/

#include "StdAfx.h"
#include "SkinDialog.h"
#include "..\XCConfiguration.h"

#define ROUND_EDGE_SIZE 8
#define TITLE_BAR_HEIGHT 22

namespace
{
UINT GetDialogDpi(HWND hWnd)
{
	HMODULE hUser32 = ::GetModuleHandle(_T("user32.dll")) ;
	if(hUser32!=NULL)
	{
		typedef UINT (WINAPI *GetDpiForWindowFn)(HWND) ;
		GetDpiForWindowFn pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
			::GetProcAddress(hUser32,"GetDpiForWindow")) ;
		if(pGetDpiForWindow!=NULL)
		{
			const UINT dpi = pGetDpiForWindow(hWnd) ;
			if(dpi!=0)
				return dpi ;
		}
	}

	HDC hDC = ::GetDC(hWnd) ;
	if(hDC!=NULL)
	{
		const int dpi = ::GetDeviceCaps(hDC,LOGPIXELSX) ;
		::ReleaseDC(hWnd,hDC) ;
		if(dpi>0)
			return static_cast<UINT>(dpi) ;
	}

	return 96 ;
}

int ScaleMetric(HWND hWnd,int value)
{
	return ::MulDiv(value,static_cast<int>(GetDialogDpi(hWnd)),96) ;
}

void ApplyRoundedRegion(HWND hWnd,int width,int height,BOOL redraw)
{
	const int radius = ScaleMetric(hWnd,ROUND_EDGE_SIZE) ;
	HRGN hRgn = ::CreateRoundRectRgn(0,0,width,height,radius,radius) ;
	if(hRgn!=NULL)
	{
		// After a successful SetWindowRgn call Windows owns the region handle.
		if(::SetWindowRgn(hWnd,hRgn,redraw)==0)
			::DeleteObject(hRgn) ;
	}
}
}

CSkinDialog::CSkinDialog(int nDlgID,HWND hParentWnd):CptDialog(nDlgID,hParentWnd,CptMultipleLanguage::GetInstance()->GetResourceHandle()) 
{
	m_bDrawEdge = true ;
	m_bDrawTitleBar = true ;
	m_bDragMove = true ;
}

CSkinDialog::~CSkinDialog(void)
{
}

BOOL CSkinDialog::OnInitDialog()
{
	RECT rt = {0} ;
	::GetClientRect(this->GetSafeHwnd(),&rt) ;
	ApplyRoundedRegion(this->GetSafeHwnd(),rt.right-rt.left,rt.bottom-rt.top,FALSE) ;
	return TRUE ;
}

void CSkinDialog::SetDragMove(bool bDragMove)
{
	m_bDragMove = bDragMove ;
}

void CSkinDialog::SetWindowSize(const SptSize& size)
{
	::SetWindowRgn(this->GetSafeHwnd(),NULL,FALSE) ;
	::SetWindowPos(this->GetSafeHwnd(),HWND_TOP,0,0,size.nWidth,size.nHeight,SWP_NOZORDER|SWP_NOMOVE) ;
	ApplyRoundedRegion(this->GetSafeHwnd(),size.nWidth,size.nHeight,TRUE) ;
	::InvalidateRect(this->GetSafeHwnd(),NULL,TRUE) ;
	this->OnPaint() ;
}

void CSkinDialog::OnPaint()
{
	if(m_bDrawTitleBar)
		this->DrawTitleBar() ;

	if(m_bDrawEdge)
		this->DrawEdge() ;
}

void CSkinDialog::SetDrawing(bool bDrawEdge,bool bDrawTitelBar)
{
	m_bDrawTitleBar = bDrawTitelBar ;
	m_bDrawEdge = bDrawEdge ;
	::InvalidateRect(this->GetSafeHwnd(),NULL,TRUE) ;
	::UpdateWindow(this->GetSafeHwnd()) ;
}

BOOL CSkinDialog::OnEraseBkgnd(HDC hDC)
{
	SptRect rt ;
	::GetClientRect(this->GetSafeHwnd(),rt.GetRECTPointer()) ;
	::FillRect(hDC,rt.GetRECTPointer(),(HBRUSH)::GetSysColorBrush(COLOR_BTNFACE)) ;
	this->OnPaint() ;
	return TRUE ;
}

void CSkinDialog::DrawEdge()
{
	HWND hWnd = this->GetSafeHwnd() ;
	HDC hDC = ::GetDC(hWnd) ;
	if(hDC==NULL)
		return ;

	RECT rt = {0} ;
	::GetClientRect(hWnd,&rt) ;
	const int penWidth = ScaleMetric(hWnd,1) ;
	const int radius = ScaleMetric(hWnd,ROUND_EDGE_SIZE+5) ;
	HPEN hPen = ::CreatePen(PS_SOLID,penWidth>0?penWidth:1,::GetSysColor(COLOR_3DSHADOW)) ;
	HBRUSH hOldBrush = (HBRUSH)::SelectObject(hDC,::GetStockObject(NULL_BRUSH)) ;
	HPEN hOldPen = (HPEN)::SelectObject(hDC,hPen) ;

	::RoundRect(hDC,rt.left,rt.top,rt.right,rt.bottom,radius,radius) ;

	::SelectObject(hDC,hOldPen) ;
	::SelectObject(hDC,hOldBrush) ;
	::DeleteObject(hPen) ;
	::ReleaseDC(hWnd,hDC) ;
}

void CSkinDialog::DrawTitleBar()
{
	HWND hWnd = this->GetSafeHwnd() ;
	HDC hDC = ::GetDC(hWnd) ;
	if(hDC==NULL)
		return ;

	RECT rtClient = {0} ;
	::GetClientRect(hWnd,&rtClient) ;
	const int titleHeight = ScaleMetric(hWnd,TITLE_BAR_HEIGHT) ;
	RECT rtTitle = {0,0,rtClient.right-rtClient.left,titleHeight} ;

	// Use system colors instead of the legacy bitmap skin. This keeps the
	// existing custom title-bar structure while matching current Windows UI.
	::FillRect(hDC,&rtTitle,::GetSysColorBrush(COLOR_WINDOW)) ;

	const int iconX = ScaleMetric(hWnd,8) ;
	const int iconY = ScaleMetric(hWnd,2) ;
	const int iconSize = ScaleMetric(hWnd,18) ;
	HICON hIcon = CptMultipleLanguage::GetInstance()->GetIcon(IDI_SMALL) ;
	if(hIcon!=NULL)
		::DrawIconEx(hDC,iconX,iconY,hIcon,iconSize,iconSize,0,NULL,DI_NORMAL) ;

	TCHAR szBuf[128] = {0} ;
	if(::GetWindowText(hWnd,szBuf,sizeof(szBuf)/sizeof(TCHAR))>0)
	{
		const int fontHeight = -ScaleMetric(hWnd,12) ;
		HFONT hFont = ::CreateFont(fontHeight,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
			DEFAULT_PITCH|FF_DONTCARE,_T("Segoe UI")) ;
		HFONT hOldFont = NULL ;
		if(hFont!=NULL)
			hOldFont = (HFONT)::SelectObject(hDC,hFont) ;

		const int oldMode = ::SetBkMode(hDC,TRANSPARENT) ;
		const COLORREF oldColor = ::SetTextColor(hDC,::GetSysColor(COLOR_WINDOWTEXT)) ;
		RECT rtText = {ScaleMetric(hWnd,32),0,rtTitle.right-ScaleMetric(hWnd,8),titleHeight} ;
		::DrawText(hDC,szBuf,-1,&rtText,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX) ;
		::SetTextColor(hDC,oldColor) ;
		::SetBkMode(hDC,oldMode) ;

		if(hOldFont!=NULL)
			::SelectObject(hDC,hOldFont) ;
		if(hFont!=NULL)
			::DeleteObject(hFont) ;
	}

	if(hIcon!=NULL)
		::DestroyIcon(hIcon) ;
	::ReleaseDC(hWnd,hDC) ;
}

int CSkinDialog::OnProcessMessage(HWND hWnd,UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	switch(uMsg)
	{
	case 0x00AE: // WM_NCUAHDRAWCAPTION
	case 0x00AF: // WM_NCUAHDRAWFRAME
		return WM_NCPAINT ;

	case WM_SHOWWINDOW:
		// The old 1 ms AnimateWindow blend predates modern composition and can
		// introduce unnecessary redraw/flicker. Let DWM present the window.
		::InvalidateRect(hWnd,NULL,TRUE) ;
		::UpdateWindow(hWnd) ;
		return 0 ;

	case WM_LBUTTONDOWN:
		if(m_bDragMove)
			this->SendMessage(WM_SYSCOMMAND,SC_MOVE|0x0002,NULL) ;
		return 1 ;

	case WM_SIZE:
		break ;

	case WM_NCPAINT:
		break ;
	}

	return CptDialog::OnProcessMessage(hWnd,uMsg,wParam,lParam) ;
}