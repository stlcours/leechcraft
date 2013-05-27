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

#include "pagerwindow.h"
#include <QUrl>
#include <QStandardItemModel>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QtDebug>
#include <util/sys/paths.h>
#include <util/gui/autoresizemixin.h>
#include <util/gui/unhoverdeletemixin.h>
#include <util/qml/colorthemeproxy.h>
#include <util/qml/settableiconprovider.h>
#include <util/x11/xwrapper.h>

namespace LeechCraft
{
namespace Krigstask
{
	class DesktopsModel : public QStandardItemModel
	{
	public:
		enum Role
		{
			SubModel = Qt::UserRole + 1,
			DesktopName
		};

		DesktopsModel (QObject *parent)
		: QStandardItemModel (parent)
		{
			QHash<int, QByteArray> roleNames;
			roleNames [Role::SubModel] = "subModel";
			roleNames [Role::DesktopName] = "desktopName";
			setRoleNames (roleNames);
		}
	};

	class SingleDesktopModel : public QStandardItemModel
	{
	public:
		enum Role
		{
			WinName = Qt::UserRole + 1,
			WID
		};

		SingleDesktopModel (QObject *parent)
		: QStandardItemModel (parent)
		{
			QHash<int, QByteArray> roleNames;
			roleNames [Role::WinName] = "winName";
			roleNames [Role::WID] = "wid";
			setRoleNames (roleNames);
		}
	};

	PagerWindow::PagerWindow (ICoreProxy_ptr proxy, QWidget *parent)
	: QDeclarativeView (parent)
	, DesktopsModel_ (new DesktopsModel (this))
	, WinIconProv_ (new Util::SettableIconProvider)
	, WinSnapshotProv_ (new Util::SettableIconProvider)
	{
		new Util::UnhoverDeleteMixin (this);

		setStyleSheet ("background: transparent");
		setWindowFlags (Qt::ToolTip);
		setAttribute (Qt::WA_TranslucentBackground);

		for (const auto& cand : Util::GetPathCandidates (Util::SysPath::QML, ""))
			engine ()->addImportPath (cand);

		rootContext ()->setContextProperty ("colorProxy",
				new Util::ColorThemeProxy (proxy->GetColorThemeManager (), this));

		engine ()->addImageProvider ("WinIcons", WinIconProv_);
		engine ()->addImageProvider ("WinSnaps", WinSnapshotProv_);

		FillModel ();
		rootContext ()->setContextProperty ("desktopsModel", DesktopsModel_);

		setResizeMode (SizeViewToRootObject);

		const auto& path = Util::GetSysPath (Util::SysPath::QML, "krigstask", "Pager.qml");
		setSource (QUrl::fromLocalFile (path));
	}

	void PagerWindow::FillModel ()
	{
		auto& w = Util::XWrapper::Instance ();

		const auto numDesktops = w.GetDesktopCount ();
		if (numDesktops <= 0)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown desktop count";
			deleteLater ();
			return;
		}

		const auto& allWindows = w.GetWindows ();
		QMap<int, QList<ulong>> desk2wins;
		for (auto wid : w.GetWindows ())
			if (w.ShouldShow (wid))
				desk2wins [w.GetWindowDesktop (wid)] << wid;

		for (int i = 0; i < numDesktops; ++i)
		{
			auto subModel = new SingleDesktopModel (this);
			FillSubmodel (subModel, desk2wins [i]);

			auto item = new QStandardItem;
			item->setData (QVariant::fromValue<QObject*> (subModel), DesktopsModel::Role::SubModel);
			item->setData ("Desktop " + QString::number (i + 1), DesktopsModel::Role::DesktopName);
			DesktopsModel_->appendRow (item);
		}
	}

	void PagerWindow::FillSubmodel (SingleDesktopModel *model, const QList<ulong>& windows)
	{
		auto& w = Util::XWrapper::Instance ();

		for (auto wid : windows)
		{
			WinIconProv_->SetIcon ({ QString::number (wid) }, w.GetWindowIcon (wid));

			auto item = new QStandardItem;
			item->setData (w.GetWindowTitle (wid), SingleDesktopModel::Role::WinName);
			item->setData (static_cast<qulonglong> (wid), SingleDesktopModel::Role::WID);
			model->appendRow (item);
		}
	}
}
}
