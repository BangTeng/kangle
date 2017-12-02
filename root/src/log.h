/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef LOG_H_23541234123413241234
#define LOG_H_23541234123413241234
#include <time.h>
#include <stdarg.h>
#define KLOG_ERR			1
#define KLOG_WARNING		2
#define KLOG_NOTICE		3
#define KLOG_INFO			4
#define KLOG_DEBUG		5
void vklog(int level, const char *fmt, va_list ap);
void klog(int level, const char *fmt, ...);
int klog_start();
void set_logger();
void CTIME_R(time_t *a, char *b, size_t l);
void debug(const char *fmt, ...);
#ifdef _WIN32
void LogEvent(const char *pFormat, ...);
#endif

#endif
