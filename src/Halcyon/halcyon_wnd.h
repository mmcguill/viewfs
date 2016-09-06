/////////////////////////////////////////////////////////////////////
// 10/APR/2005
//
// Project: Halcyon
// File: halcyon_wnd.h
// Author: Mark McGuill
//
// Copyright (C) 2005  Mark McGuill
//
/////////////////////////////////////////////////////////////////////

typedef struct _HALCYON_WND_LAYOUT
{
	HWND hWndTree;
	RECT rcTree;
	HWND hWndList;
	RECT rcList;
}HALCYON_WND_LAYOUT;

/////////////////////////////////////////////////////////////////////

class HalcyonWnd
{
public:
	HalcyonWnd();
	~HalcyonWnd();

public:
	void Close();

protected:
	
protected:
	HALCYON_WND_LAYOUT	m_layout;
	HWND				m_hWnd;

private:

private:
};

/////////////////////////////////////////////////////////////////////