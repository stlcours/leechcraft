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

#include <libimobiledevice/libimobiledevice.h>
#include <stdexcept>
#include <typeinfo>
#include <QString>

namespace LeechCraft
{
namespace LMP
{
namespace jOS
{
	class MobileRaiiException : public std::runtime_error
	{
		uint16_t ErrCode_;
	public:
		MobileRaiiException (const std::string&, uint16_t);
		~MobileRaiiException () noexcept;

		uint16_t GetErrCode () const;
	};

	template<typename T, typename Deleter = int16_t (*) (T)>
	class MobileRaii
	{
		T Type_ = nullptr;
		const Deleter Deleter_;
	public:
		typedef MobileRaii<T, Deleter> type;

		template<typename Creator>
		MobileRaii (Creator c, Deleter d)
		: Deleter_ { d }
		{
			if (const auto ret = c (&Type_))
			{
				const auto& errStr = "Cannot create something: " + QString::number (ret) + " for " + typeid (T).name ();
				throw MobileRaiiException (errStr.toUtf8 ().constData (), ret);
			}
		}

		~MobileRaii ()
		{
			if (Type_)
				Deleter_ (Type_);
		}

		MobileRaii (MobileRaii&& other)
		: Type_ { other.Type_ }
		, Deleter_ { other.Deleter_ }
		{
			other.Type_ = nullptr;
		}

		MobileRaii (const MobileRaii&) = delete;
		MobileRaii& operator= (const MobileRaii&) = delete;

		operator T () const
		{
			return Type_;
		}
	};

	template<typename T, typename Creator, typename Deleter>
	MobileRaii<T, Deleter> MakeRaii (Creator c, Deleter d)
	{
		return { c, d };
	}
}
}
}
