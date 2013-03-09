//
// internal.h
// ~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//
// This file is part of Libavplayer.
//
// Libavplayer is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// Libavplayer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with Libavplayer; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//
// * $Id: ins.h 98 2011-08-19 16:08:52Z jack $
//

#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>

#ifndef WIN32
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <unistd.h>
#else
#	include <io.h>
#	include <windows.h>
#endif // WIN32


#include <set>
#include <map>
#include <list>
#include <algorithm>



#endif // __INTERNAL_H__
