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

#include "quarkmanager.h"

#if QT_VERSION < 0x050000
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QDeclarativeImageProvider>
#else
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#endif

#include <QStandardItem>
#include <QApplication>
#include <QTranslator>
#include <QFile>
#include <QtDebug>
#include <QFileInfo>
#include <QDir>
#include <qjson/parser.h>
#include <util/util.h>
#include <interfaces/iquarkcomponentprovider.h>
#include <interfaces/core/iiconthememanager.h>
#include "viewmanager.h"
#include "sbview.h"
#include "quarksettingsmanager.h"

namespace LeechCraft
{
namespace SB2
{
	namespace
	{
#if QT_VERSION < 0x050000
		class ImageProvProxy : public QDeclarativeImageProvider
		{
			QDeclarativeImageProvider *Wrapped_;
		public:
			ImageProvProxy (QDeclarativeImageProvider *other)
			: QDeclarativeImageProvider (other->imageType ())
#else
		class ImageProvProxy : public QQuickImageProvider
		{
			QQuickImageProvider *Wrapped_;
		public:
			ImageProvProxy (QQuickImageProvider *other)
			: QQuickImageProvider (other->imageType ())
#endif
			, Wrapped_ (other)
			{
			}

			QImage requestImage (const QString& id, QSize *size, const QSize& requestedSize)
			{
				return Wrapped_->requestImage (id, size, requestedSize);
			}

			QPixmap requestPixmap (const QString& id, QSize *size, const QSize& requestedSize)
			{
				return Wrapped_->requestPixmap (id, size, requestedSize);
			}
		};
	}

	QuarkManager::QuarkManager (QuarkComponent_ptr comp,
			ViewManager *manager, ICoreProxy_ptr proxy)
	: QObject (manager)
	, ViewMgr_ (manager)
	, Proxy_ (proxy)
	, Component_ (comp)
	, URL_ (comp->Url_)
	, SettingsManager_ (0)
	, Translator_ (TryLoadTranslator ())
	, Manifest_ (URL_.toLocalFile ())
	{
		if (!ViewMgr_)
			return;

		qDebug () << Q_FUNC_INFO << "adding" << comp->Url_;
		auto ctx = manager->GetView ()->rootContext ();
		for (const auto& pair : comp->StaticProps_)
			ctx->setContextProperty (pair.first, pair.second);
		for (const auto& pair : comp->DynamicProps_)
			ctx->setContextProperty (pair.first, pair.second);
		for (const auto& pair : comp->ContextProps_)
			ctx->setContextProperty (pair.first, pair.second);

		auto engine = manager->GetView ()->engine ();
		for (const auto& pair : comp->ImageProviders_)
		{
			if (auto old = engine->imageProvider (pair.first))
			{
				engine->removeImageProvider (pair.first);
				delete old;
			}
			engine->addImageProvider (pair.first, new ImageProvProxy (pair.second));
		}

		CreateSettings ();
	}

	const Manifest& QuarkManager::GetManifest () const
	{
		return Manifest_;
	}

	bool QuarkManager::IsValidArea () const
	{
		const auto& areas = Manifest_.GetAreas ();
		return areas.isEmpty () || areas.contains ("panel");
	}

	bool QuarkManager::HasSettings () const
	{
		return SettingsManager_;
	}

	Util::XmlSettingsDialog* QuarkManager::GetXSD () const
	{
		return XSD_.get ();
	}

	QString QuarkManager::GetSuffixedName (const QString& suffix) const
	{
		if (!URL_.isLocalFile ())
			return {};

		const auto& localName = URL_.toLocalFile ();
		const auto& suffixed = localName + suffix;
		if (!QFile::exists (suffixed))
			return {};

		return suffixed;
	}

	std::shared_ptr<QTranslator> QuarkManager::TryLoadTranslator () const
	{
		if (!URL_.isLocalFile ())
			return {};

		auto dir = QFileInfo { URL_.toLocalFile () }.dir ();
		if (!dir.cd ("ts"))
			return {};

		const auto& locale = Util::GetLocaleName ();
		const auto& localeLang = locale.section ('_', 0, 0);

		const QStringList filters { "*_" + locale + ".qm", "*_" + localeLang + ".qm" };
		const auto& files = dir.entryList (filters, QDir::Files);
		if (files.isEmpty ())
			return {};

		std::shared_ptr<QTranslator> result
		{
			new QTranslator,
			[] (QTranslator *tr)
			{
				QApplication::removeTranslator (tr);
				delete tr;
			}
		};
		const auto& filename = dir.filePath (files.value (0));
		if (!result->load (filename))
			return {};

		QApplication::installTranslator (result.get ());

		return result;
	}

	void QuarkManager::CreateSettings ()
	{
		const auto& settingsName = GetSuffixedName (".settings");
		if (settingsName.isEmpty ())
			return;

		XSD_.reset (new Util::XmlSettingsDialog);
		SettingsManager_ = new QuarkSettingsManager (URL_, ViewMgr_->GetView ()->rootContext ());
		XSD_->RegisterObject (SettingsManager_, settingsName);
	}
}
}
