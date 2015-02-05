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

#include <QNetworkAccessManager>
#include <QLocale>
#include "interfaces/core/ihookproxy.h"

class QTimer;

namespace LeechCraft
{
	class SslErrorsDialog;

	namespace Util
	{
		class CustomCookieJar;
	}

	class NetworkAccessManager : public QNetworkAccessManager
	{
		Q_OBJECT

		QTimer * const CookieSaveTimer_;

		Util::CustomCookieJar *CookieJar_;
	public:
		NetworkAccessManager (QObject* = 0);
		virtual ~NetworkAccessManager ();
	protected:
		QNetworkReply* createRequest (Operation,
				const QNetworkRequest&, QIODevice*);
	private:
		void DoCommonAuth (const QString&, QAuthenticator*);
	private slots:
		void handleAuthentication (QNetworkReply*, QAuthenticator*);
		void handleAuthentication (const QNetworkProxy&, QAuthenticator*);
		void handleSslErrors (QNetworkReply*, const QList<QSslError>&);

		void saveCookies () const;
		void handleFilterTrackingCookies ();
		void setCookiesEnabled ();
		void setMatchDomainExactly ();
		void setCookiesLists ();

		void handleCacheSize ();
	signals:
		void requestCreated (QNetworkAccessManager::Operation,
				const QNetworkRequest&, QNetworkReply*);
		void error (const QString&) const;
		void acceptableLanguagesChanged ();

		void hookNAMCreateRequest (LeechCraft::IHookProxy_ptr proxy,
					QNetworkAccessManager *manager,
					QNetworkAccessManager::Operation *op,
					QIODevice **dev);
	};
};
