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

#include "monocle.h"
#include <QIcon>
#include <qurl.h>
#include <util/util.h>
#include <interfaces/entitytesthandleresult.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include "core.h"
#include "documenttab.h"
#include "xmlsettingsmanager.h"
#include "defaultbackendmanager.h"

namespace LeechCraft
{
namespace Monocle
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Util::InstallTranslator ("monocle");

		XSD_.reset (new Util::XmlSettingsDialog);
		XSD_->RegisterObject (&XmlSettingsManager::Instance (), "monoclesettings.xml");

		Core::Instance ().SetProxy (proxy);

		XSD_->SetDataSource ("DefaultBackends",
				Core::Instance ().GetDefaultBackendManager ()->GetModel ());

		DocTabInfo_ =
		{
			GetUniqueID () + "_Document",
			"Monocle",
			GetInfo (),
			GetIcon (),
			55,
			TFOpenableByRequest | TFSuggestOpening
		};
	}

	void Plugin::SecondInit ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Monocle";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "Monocle";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Modular document viewer for LeechCraft.");
	}

	QIcon Plugin::GetIcon () const
	{
		static QIcon icon ("lcicons:/monocle/resources/images/monocle.svg");
		return icon;
	}

	EntityTestHandleResult Plugin::CouldHandle (const Entity& e) const
	{
		if (!(e.Parameters_ & FromUserInitiated))
			return EntityTestHandleResult ();

		if (!e.Entity_.canConvert<QUrl> ())
			return EntityTestHandleResult ();

		const auto& url = e.Entity_.toUrl ();
		if (url.scheme () != "file")
			return EntityTestHandleResult ();

		const auto& local = url.toLocalFile ();
		if (!QFile::exists (local))
			return EntityTestHandleResult ();

		return Core::Instance ().CanLoadDocument (local) ?
				EntityTestHandleResult (EntityTestHandleResult::PIdeal) :
				EntityTestHandleResult ();
	}

	void Plugin::Handle (Entity e)
	{
		auto tab = new DocumentTab (DocTabInfo_, this);
		tab->SetDoc (e.Entity_.toUrl ().toLocalFile ());
		EmitTab (tab);
	}

	Util::XmlSettingsDialog_ptr Plugin::GetSettingsDialog () const
	{
		return XSD_;
	}

	TabClasses_t Plugin::GetTabClasses () const
	{
		return { DocTabInfo_ };
	}

	void Plugin::TabOpenRequested (const QByteArray& id)
	{
		if (id == DocTabInfo_.TabClass_)
			EmitTab (new DocumentTab (DocTabInfo_, this));
		else
			qWarning () << Q_FUNC_INFO
					<< "unknown tab class"
					<< id;
	}

	QSet<QByteArray> Plugin::GetExpectedPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Monocle.IBackendPlugin";
		return result;
	}

	void Plugin::AddPlugin (QObject *pluginObj)
	{
		Core::Instance ().AddPlugin (pluginObj);
	}

	void Plugin::RecoverTabs (const QList<TabRecoverInfo>& infos)
	{
		Q_FOREACH (const auto& info, infos)
		{
			auto tab = new DocumentTab (DocTabInfo_, this);
			Q_FOREACH (const auto& pair, info.DynProperties_)
				tab->setProperty (pair.first, pair.second);

			EmitTab (tab);

			tab->RecoverState (info.Data_);
		}
	}

	void Plugin::EmitTab (DocumentTab *tab)
	{
		emit addNewTab (DocTabInfo_.VisibleName_, tab);
		emit changeTabIcon (tab, DocTabInfo_.Icon_);
		emit raiseTab (tab);

		connect (tab,
				SIGNAL (removeTab (QWidget*)),
				this,
				SIGNAL (removeTab (QWidget*)));
		connect (tab,
				SIGNAL (changeTabName (QWidget*, QString)),
				this,
				SIGNAL (changeTabName (QWidget*, QString)));
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_monocle, LeechCraft::Monocle::Plugin);

