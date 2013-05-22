/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2011  Yury Erik Potapov
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

#include "gmailnotifier.h"
#include <QIcon>
#include <QTimer>
#include <QAction>
#include <QApplication>
#include <QPen>
#include <QTranslator>
#include <util/util.h>
#include <util/sys/paths.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include "xmlsettingsmanager.h"
#include "gmailchecker.h"
#include "notifier.h"
#include "quarkmanager.h"

namespace LeechCraft
{
namespace GmailNotifier
{
	void GmailNotifier::Init (ICoreProxy_ptr proxy)
	{
		Util::InstallTranslator ("gmailnotifier");
		SettingsDialog_.reset (new Util::XmlSettingsDialog ());
		SettingsDialog_->RegisterObject (XmlSettingsManager::Instance (),
				"gmailnotifiersettings.xml");
		XmlSettingsManager::Instance ()->RegisterObject ({ "Login",  "Password"},
				this,
				"setAuthorization");

		GmailChecker_ = new GmailChecker (this);
		setAuthorization ();

		UpdateTimer_ = new QTimer (this);
		applyInterval ();
		UpdateTimer_->start ();

		XmlSettingsManager::Instance ()->RegisterObject ("UpdateInterval",
				this,
				"applyInterval");

		connect (UpdateTimer_,
				SIGNAL (timeout ()),
				GmailChecker_,
				SLOT (checkNow ()));

		connect (GmailChecker_,
				SIGNAL (waitMe ()),
				UpdateTimer_,
				SLOT (stop ()));
		connect (GmailChecker_,
				SIGNAL (canContinue ()),
				UpdateTimer_,
				SLOT (start ()));

		Notifier_ = new Notifier (proxy, this);
		connect (GmailChecker_,
				SIGNAL (gotConversations (ConvInfos_t)),
				Notifier_,
				SLOT (notifyAbout (ConvInfos_t)));

		auto manager = new QuarkManager (proxy, this);
		Quark_.reset (new QuarkComponent ("gmailnotifier", "GMQuark.qml"));
		Quark_->DynamicProps_ << QPair<QString, QObject*> ("GMN_proxy", manager);

		connect (GmailChecker_,
				SIGNAL (gotConversations (ConvInfos_t)),
				manager,
				SLOT (handleConversations (ConvInfos_t)));
	}

	void GmailNotifier::SecondInit ()
	{
	}

	QByteArray GmailNotifier::GetUniqueID () const
	{
		return "org.LeechCraft.GmailNotifier";
	}

	void GmailNotifier::Release ()
	{
	}

	QString GmailNotifier::GetName () const
	{
		return "Gmail Notifier";
	}

	QString GmailNotifier::GetInfo () const
	{
		return tr ("Google mail notification plugin");
	}

	QIcon GmailNotifier::GetIcon () const
	{
		static QIcon icon ("lcicons:/gmailnotifier/gmailnotifier.svg");
		return icon;
	}

	Util::XmlSettingsDialog_ptr GmailNotifier::GetSettingsDialog () const
	{
		return SettingsDialog_;
	}

	QuarkComponents_t GmailNotifier::GmailNotifier::GetComponents () const
	{
		return { Quark_ };
	}

	void GmailNotifier::setAuthorization ()
	{
		GmailChecker_->SetAuthSettings (XmlSettingsManager::Instance ()->property ("Login").toString (),
				XmlSettingsManager::Instance ()->property ("Password").toString ());
	}

	void GmailNotifier::applyInterval ()
	{
		const int secs = XmlSettingsManager::Instance ()->property ("UpdateInterval").toInt ();
		UpdateTimer_->stop ();
		UpdateTimer_->setInterval (secs * 1000);
		UpdateTimer_->start ();
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_gmailnotifier, LeechCraft::GmailNotifier::GmailNotifier);
