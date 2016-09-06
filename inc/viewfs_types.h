/////////////////////////////////////////////////////////////////////
// 24/JUN/2006
//
// Project: ViewFS	
// File: viewfs_types.h
// Author: Mark McGuill
//
// Copyright (C) 2006  Mark McGuill
//
//
/////////////////////////////////////////////////////////////////////

// Integers

typedef __int64			_I64;
typedef __int32			_I32;
typedef __int16			_I16;
typedef __int8			_I8;

typedef unsigned __int64 _U64;
typedef unsigned __int32 _U32;
typedef unsigned __int16 _U16;
typedef unsigned __int8	_U8;

typedef void* HANDLE;
//typedef char TCHAR;

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))