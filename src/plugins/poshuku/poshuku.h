/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2009  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#ifndef PLUGINS_POSHUKU_POSHUKU_H
#define PLUGINS_POSHUKU_POSHUKU_H
#include <memory>
#include <QAction>
#include <QTranslator>
#include <QWidget>
#include <interfaces/iinfo.h>
#include <interfaces/iembedtab.h>
#include <interfaces/imultitabs.h>
#include <interfaces/iwebbrowser.h>
#include <interfaces/ipluginready.h>
#include <interfaces/imenuembedder.h>
#include <interfaces/ihavesettings.h>
#include <interfaces/ientityhandler.h>
#include <interfaces/ihaveshortcuts.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include "ui_poshuku.h"

class QWebView;

namespace LeechCraft
{
	namespace Plugins
	{
		namespace Poshuku
		{
			class Poshuku : public QWidget
						  , public IInfo
						  , public IEmbedTab
						  , public IMultiTabs
						  , public IMultiTabsWidget
						  , public IPluginReady
						  , public IHaveSettings
						  , public IEntityHandler
						  , public IHaveShortcuts
						  , public IWebBrowser
						  , public IMenuEmbedder
			{
				Q_OBJECT
				Q_INTERFACES (IInfo IEmbedTab IMultiTabs IMultiTabsWidget IHaveSettings IEntityHandler IPluginReady IWebBrowser IHaveShortcuts IMenuEmbedder)

				Ui::Poshuku Ui_;
				QMenu *ToolMenu_;
				QAction *ImportXbel_;
				QAction *ExportXbel_;
				QAction *CheckFavorites_;

				std::auto_ptr<QTranslator> Translator_;
				boost::shared_ptr<LeechCraft::Util::XmlSettingsDialog> XmlSettingsDialog_;
			public:
				enum Actions
				{
					EAImportXbel_ = BrowserWidget::ActionMax + 1,
					EAExportXbel_,
					EACheckFavorites_
				};
				void Init (ICoreProxy_ptr);
				void SecondInit ();
				void Release ();
				QString GetName () const;
				QString GetInfo () const;
				QStringList Provides () const;
				QStringList Needs () const;
				QStringList Uses () const;
				void SetProvider (QObject*, const QString&);
				QIcon GetIcon () const;

				QWidget* GetTabContents ();
				QToolBar* GetToolBar () const;

				void Remove ();
				void NewTabRequested ();
				QList<QAction*> GetTabBarContextMenuActions () const;
				QObject* ParentMultiTabs () const;

				QSet<QByteArray> GetExpectedPluginClasses () const;
				void AddPlugin (QObject*);

				boost::shared_ptr<LeechCraft::Util::XmlSettingsDialog> GetSettingsDialog () const;

				bool CouldHandle (const LeechCraft::DownloadEntity&) const;
				void Handle (LeechCraft::DownloadEntity);

				void Open (const QString&);
				IWebWidget* GetWidget () const;
				QWebView* CreateWindow ();

				void SetShortcut (int, const QKeySequence&);
				QMap<int, LeechCraft::ActionInfo> GetActionInfo () const;

				QList<QMenu*> GetToolMenus () const;
				QList<QAction*> GetToolActions () const;
			public slots:
				void newTabRequested ();
			private:
				void InitConnections ();
				void RegisterSettings ();
			private slots:
				void setHtml ();
				void viewerSettingsChanged ();
				void developerExtrasChanged ();
				void cacheSettingsChanged ();
				void handleError (const QString&);
				void handleNewTab ();
				void handleSettingsClicked (const QString&);
				void handleCheckFavorites ();
			signals:
				void bringToFront ();
				void addNewTab (const QString&, QWidget*);
				void removeTab (QWidget*);
				void changeTabName (QWidget*, const QString&);
				void changeTabIcon (QWidget*, const QIcon&);
				void changeTooltip (QWidget*, QWidget*);
				void statusBarChanged (QWidget*, const QString&);
				void raiseTab (QWidget*);
				void gotEntity (const LeechCraft::DownloadEntity&);
				void couldHandle (const LeechCraft::DownloadEntity&, bool*);
				void notify (const LeechCraft::Notification&);
			};
		};
	};
};

#endif

