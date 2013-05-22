/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#pragma once

#include <QFlags>

namespace LeechCraft
{
namespace Util
{
	enum WinStateFlag
	{
		NoState			= 0,
		Modal			= 1 << 0,
		Sticky			= 1 << 1,
		MaximizedVert	= 1 << 2,
		MaximizedHorz	= 1 << 3,
		Shaded			= 1 << 4,
		SkipTaskbar		= 1 << 5,
		SkipPager		= 1 << 6,
		Hidden			= 1 << 7,
		Fullscreen		= 1 << 8,
		OnTop			= 1 << 9,
		OnBottom		= 1 << 10,
		Attention		= 1 << 11
	};

	Q_DECLARE_FLAGS (WinStateFlags, WinStateFlag)

	enum AllowedActionFlag
	{
		NoAction		= 0,
		Move			= 1 << 0,
		Resize			= 1 << 1,
		Minimize		= 1 << 2,
		Shade			= 1 << 3,
		Stick			= 1 << 4,
		MaximizeHorz	= 1 << 5,
		MaximizeVert	= 1 << 6,
		ShowFullscreen	= 1 << 7,
		ChangeDesktop	= 1 << 8,
		Close			= 1 << 9,
		MoveToTop		= 1 << 10,
		MoveToBottom	= 1 << 11
	};

	Q_DECLARE_FLAGS (AllowedActionFlags, AllowedActionFlag)
}
}

Q_DECLARE_OPERATORS_FOR_FLAGS (LeechCraft::Util::WinStateFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS (LeechCraft::Util::AllowedActionFlags)
