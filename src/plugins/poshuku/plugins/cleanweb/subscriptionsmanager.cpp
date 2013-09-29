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

#include "subscriptionsmanager.h"
#include <memory>
#include <QMessageBox>
#ifdef USE_QT5
#include <QUrlQuery>
#endif
#include "core.h"
#include "subscriptionadddialog.h"

namespace LeechCraft
{
namespace Poshuku
{
namespace CleanWeb
{
	SubscriptionsManager::SubscriptionsManager (QWidget *parent)
	: QWidget (parent)
	{
		Ui_.setupUi (this);
		Ui_.Subscriptions_->setModel (Core::Instance ().GetModel ());
	}

	void SubscriptionsManager::on_RemoveButton__released ()
	{
		QModelIndex current = Ui_.Subscriptions_->currentIndex ();
		if (!current.isValid ())
			return;

		Core::Instance ().Remove (current);
	}

	void SubscriptionsManager::AddCustom (const QString& title, const QString& urlStr)
	{
		QUrl url (urlStr);
		QUrl locationUrl;

#ifdef USE_QT5
		const auto& locationItem = QUrlQuery (url).queryItemValue ("location");
#else
		const auto& locationItem = url.queryItemValue ("location");
#endif

		if (locationItem.contains ("%"))
			locationUrl.setUrl (QUrl::fromPercentEncoding (locationItem.toLatin1 ()));
		else
			locationUrl.setUrl (locationItem);

		if (url.scheme () == "abp" &&
				url.host () == "subscribe" &&
				locationUrl.isValid ())
		{
			if (title.isEmpty ())
			{
				QMessageBox::warning (this,
						tr ("Error adding subscription"),
						tr ("Can't add subscription without a title."),
						QMessageBox::Ok);
				return;
			}

			if (Core::Instance ().Exists (title))
			{
				QMessageBox::warning (this,
						tr ("Error adding subscription"),
						tr ("Subscription with such title allready exists."),
						QMessageBox::Ok);
				return;
			}

			if (Core::Instance ().Exists (locationUrl))
			{
				QMessageBox::warning (this,
						tr ("Error adding subscription"),
						tr ("Subscription with such title allready exists."),
						QMessageBox::Ok);
				return;
			}
		}
		else
		{
			QMessageBox::warning (this,
					tr ("Error adding subscription"),
					tr ("Invalid URL.<br />Valid URL format is: abp://subscribe/?location=URL"),
					QMessageBox::Ok);
			return;
		}

		Core::Instance ().Load (locationUrl, title);
	}

	void SubscriptionsManager::on_AddButton__released ()
	{
		std::auto_ptr<SubscriptionAddDialog> subscriptionAdd (new SubscriptionAddDialog (this));

		if (!subscriptionAdd->exec ())
			return;

		QString title = subscriptionAdd->GetName ();
		QString urlStr = subscriptionAdd->GetURL ();
		if (!title.isEmpty () ||
				!urlStr.isEmpty ())
			AddCustom (title, urlStr);

		QList<QUrl> urls = subscriptionAdd->GetAdditionalSubscriptions ();
		Q_FOREACH (const QUrl& url, urls)
			Core::Instance ().Add (url);
	}
}
}
}
