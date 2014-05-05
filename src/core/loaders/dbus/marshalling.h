/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
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

#include <memory>
#include <QObject>
#include <QMetaType>
#include <QDBusObjectPath>

class QDBusArgument;

class ICoreProxy;

typedef std::shared_ptr<ICoreProxy> ICoreProxy_ptr;

Q_DECLARE_METATYPE (QIcon)

QDBusArgument& operator<< (QDBusArgument&, const ICoreProxy_ptr&);
const QDBusArgument& operator>> (const QDBusArgument&, ICoreProxy_ptr&);

QDBusArgument& operator<< (QDBusArgument&, const QIcon&);
const QDBusArgument& operator>> (const QDBusArgument&, QIcon&);

namespace LeechCraft
{
namespace DBus
{
	void RegisterTypes ();

	class ObjectManager : public QObject
	{
		Q_OBJECT

		quint64 Counter_;
	public:
		struct ObjectDataInfo
		{
			QString Service_;
			QDBusObjectPath Path_;
		};
	private:
		QHash<QObject*, ObjectDataInfo> Registered_;

		ObjectManager ();

		ObjectManager (const ObjectManager&) = delete;
		ObjectManager (ObjectManager&&) = delete;
	public:
		static ObjectManager& Instance ();

		template<typename T>
		ObjectDataInfo RegisterObject (std::shared_ptr<T>);

		template<typename T>
		ObjectDataInfo RegisterObject (T*);

		template<typename T>
		void Wrap (std::shared_ptr<T>&, const ObjectDataInfo&);

		template<typename T>
		void Wrap (T*&, const ObjectDataInfo&);
	private slots:
		void handleObjectDestroyed (QObject*);
	};
}
}
