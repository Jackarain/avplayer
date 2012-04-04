//
// stdafx.h
// ~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __STDAFX_H__
#define __STDAFX_H__

#pragma once

#ifndef _WIN32_WINNT		// 允许使用特定于 Windows XP 或更高版本的功能。
#define _WIN32_WINNT 0x0501	// 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif						

#include <stdio.h>
#include <windows.h>
#include <tchar.h>

#include <map>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#endif // __STDAFX_H__

