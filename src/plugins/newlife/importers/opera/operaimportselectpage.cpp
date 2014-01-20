/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2013  Oleg Linkin <MaledictusDeMagog@gmail.com>
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


#include "operaimportselectpage.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSettings>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrl>
#include <QDateTime>
#include <QXmlStreamWriter>
#include <util/util.h>

namespace LeechCraft
{
namespace NewLife
{
namespace Importers
{
	OperaImportSelectPage::OperaImportSelectPage (QWidget *parent)
	: QWizardPage (parent)
	{
		Ui_.setupUi (this);
	}

	int OperaImportSelectPage::nextId () const
	{
		return -1;
	}

	void OperaImportSelectPage::initializePage ()
	{
		connect (wizard (),
				SIGNAL (accepted ()),
				this,
				SLOT (handleAccepted ()));

		connect (this,
				SIGNAL (gotEntity (const LeechCraft::Entity&)),
				wizard (),
				SIGNAL (gotEntity (const LeechCraft::Entity&)));
	}

	void OperaImportSelectPage::checkImportDataAvailable (int index)
	{
		Ui_.HistoryImport_->setChecked (false);
		Ui_.BookmarksImport_->setChecked (false);

		if (!index)
		{
			Ui_.HistoryImport_->setEnabled (false);
			Ui_.BookmarksImport_->setEnabled (false);
			return;
		}
	}

	QList<QVariant> OperaImportSelectPage::GetHistory ()
	{
		QList<QVariant> history;
		QStringList historyPaths;
#ifdef Q_OS_LINUX
		historyPaths << QDir::homePath () + "/.opera/global_history.dat"
				<< QDir::homePath () + "/.opera-next/global_history.dat";
#elif defined Q_OS_WIN
		historyPaths << QDir::homePath () + "/Application Data/Opera Software/Opera Stable/global_history.dat";
#endif
		for (const auto& path : historyPaths)
		{
			QFile oldHistoryFile (path);
			if (!oldHistoryFile.exists () || !oldHistoryFile.open (QIODevice::ReadOnly))
				continue;
			
			while (!oldHistoryFile.atEnd ())
			{
				const auto& line = oldHistoryFile.readLine ().trimmed ();
				if (line == "-1")
				{
					QMap<QString, QVariant> record;
					record ["Title"] = oldHistoryFile.readLine ().trimmed ();
					record ["URL"] = oldHistoryFile.readLine ().trimmed ();
					record ["DateTime"] = QDateTime::fromTime_t (oldHistoryFile.readLine ()
							.trimmed ().toLongLong ());
					history.push_back (record);
				}
			}
		}
		
		{
			QString historyDbPath;
#ifdef Q_OS_WIN
			historyDbPath = QDir::homePath () + "/Application Data/Opera Software/Opera Stable/History";
#endif
			if (historyDbPath.isEmpty () || !QFileInfo (historyDbPath).exists ())
				return history;

			QSqlDatabase db = QSqlDatabase::addDatabase ("QSQLITE", "Import history connection");
			db.setDatabaseName (historyDbPath);
			if (!db.open ())
			{
				qWarning () << Q_FUNC_INFO
						<< "could not open database. Try to close Opera"
						<< db.lastError ().text ();
				emit gotEntity (Util::MakeNotification (tr ("Opera Import"),
						tr ("Could not open Opera database: %1.")
							.arg (db.lastError ().text ()),
						PCritical_));
			}
			else
			{
				QSqlQuery query (db);
				query.exec ("SELECT url, title, last_visit_time FROM urls;");
				while (query.next ())
				{
					QMap<QString, QVariant> record;
					record ["URL"] = query.value (0).toUrl ();
					record ["Title"] = query.value (1).toString ();
					record ["DateTime"] = QDateTime::fromTime_t (query.value (2).toLongLong () / 1000000 - 11644473600);
					history.push_back (record);
				}
			}

			QSqlDatabase::database ("Import history connection").close ();
			QSqlDatabase::removeDatabase ("Import history connection");
		}
		
		return history;
	}

	QList<QVariant> OperaImportSelectPage::GetBookmarks ()
	{
		QList<QVariant> bookmarks;
		QStringList bookmarksPaths;
#ifdef Q_OS_LINUX
		bookmarksPaths << QDir::homePath () + "/.opera/bookmarks.adr"
				<< QDir::homePath () + "/.opera-next/bookmarks.adr";
#elif defined Q_OS_WIN
		bookmarksPaths << QDir::homePath () + "/Application Data/Opera Software/Opera Stable/bookmarks.adr";
#endif
		for (const auto& path : bookmarksPaths)
		{
			QFile oldBookmarksFile (path);
			if (!oldBookmarksFile.exists () || !oldBookmarksFile.open (QIODevice::ReadOnly))
				continue;
				
			while (!oldBookmarksFile.atEnd ())
			{
				const auto& line = oldBookmarksFile.readLine ().trimmed ();
				if (line == "#URL")
				{
					QMap<QString, QVariant> record;
					oldBookmarksFile.readLine ();
					const auto& nameLine = QString::fromUtf8 (oldBookmarksFile.readLine ().trimmed ());
					record ["Title"] = nameLine.mid (nameLine.indexOf ('=') + 1);
					const auto& urlLine = QString::fromUtf8 (oldBookmarksFile.readLine ().trimmed ());
					record ["URL"] = urlLine.mid (urlLine.indexOf ('=') + 1);
					bookmarks.push_back (record);
				}
			}
		}
		
		{
			QString bookmarksDbPath;
#ifdef Q_OS_WIN
			bookmarksDbPath = QDir::homePath () + "/Application Data/Opera Software/Opera Stable/favorites.db";
#endif
			
			if (bookmarksDbPath.isEmpty () || !QFileInfo (bookmarksDbPath).exists ())
				return bookmarks;

			QSqlDatabase db = QSqlDatabase::addDatabase ("QSQLITE", "Import bookmarks connection");
			db.setDatabaseName (bookmarksDbPath);
			if (!db.open ())
			{
				qWarning () << Q_FUNC_INFO
						<< "could not open database. Try to close Opera"
						<< db.lastError ().text ();
				emit gotEntity (Util::MakeNotification (tr ("Opera Import"),
						tr ("Could not open Opera database: %1.")
							.arg (db.lastError ().text ()),
						PCritical_));
			}
			else
			{
				QSqlQuery query (db);
				query.exec ("SELECT name, url FROM favorites;");
				while (query.next ())
				{
					QMap<QString, QVariant> record;
					record ["Title"] = query.value (0).toString ();
					record ["URL"] = query.value (1).toUrl ();
					bookmarks.push_back (record);
				}
			}
			
			QSqlDatabase::database ("Import bookmarks connection").close ();
			QSqlDatabase::removeDatabase ("Import bookmarks connection");
		}

		return bookmarks;
	}

	void OperaImportSelectPage::handleAccepted ()
	{
		if (Ui_.HistoryImport_->isEnabled () && Ui_.HistoryImport_->isChecked ())
		{
			Entity eHistory = Util::MakeEntity (QVariant (),
				QString (),
				FromUserInitiated,
				"x-leechcraft/browser-import-data");

			eHistory.Additional_ ["BrowserHistory"] = GetHistory ();
			emit gotEntity (eHistory);
		}

		if (Ui_.BookmarksImport_->isEnabled () && Ui_.BookmarksImport_->isChecked ())
		{
			Entity eBookmarks = Util::MakeEntity (QVariant (),
					QString (),
					FromUserInitiated,
					"x-leechcraft/browser-import-data");

			eBookmarks.Additional_ ["BrowserBookmarks"] = GetBookmarks ();
			emit gotEntity (eBookmarks);
		}
	}
}
}
}
