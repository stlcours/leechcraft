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

#include "recursivedirwatcher_generic.h"
#include <QFileSystemWatcher>
#include <QStringList>
#include <QDir>
#include <QtConcurrentRun>
#include <util/threads/futures.h>

namespace LeechCraft
{
namespace LMP
{
	namespace
	{
		QStringList CollectSubdirs (const QString& path)
		{
			QStringList result { path };

			QDir dir { path };
			for (const auto& item : dir.entryList (QDir::Dirs | QDir::NoDotAndDotDot))
				result += CollectSubdirs (dir.filePath (item));

			return result;
		}
	}

	RecursiveDirWatcherImpl::RecursiveDirWatcherImpl (QObject *parent)
	: QObject { parent }
	, Watcher_ { new QFileSystemWatcher { this } }
	{
		connect (Watcher_,
				SIGNAL (directoryChanged (QString)),
				this,
				SIGNAL (directoryChanged (QString)));
	}

	void RecursiveDirWatcherImpl::AddRoot (const QString& root)
	{
		qDebug () << Q_FUNC_INFO << "scanning" << root;
		Util::Sequence (this, QtConcurrent::run (CollectSubdirs, root)) >>
				[this, root] (const QStringList& paths)
				{
					Dir2Subdirs_ [root] = paths;
					Watcher_->addPaths (paths);
				};
	}

	void RecursiveDirWatcherImpl::RemoveRoot (const QString& root)
	{
		Watcher_->removePaths (Dir2Subdirs_.take (root));
	}
}
}
