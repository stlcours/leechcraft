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

#include "mrimaccount.h"
#include <QDataStream>
#include <QUrl>
#include <util/xpc/util.h>
#include <interfaces/core/ientitymanager.h>
#include <interfaces/azoth/iproxyobject.h>
#include "proto/connection.h"
#include "proto/message.h"
#include "mrimprotocol.h"
#include "mrimaccountconfigwidget.h"
#include "mrimbuddy.h"
#include "mrimmessage.h"
#include "core.h"
#include "groupmanager.h"
#include "vaderutil.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Vader
{
	MRIMAccount::MRIMAccount (const QString& name, MRIMProtocol *proto)
	: QObject (proto)
	, Proto_ (proto)
	, Name_ (name)
	, Conn_ (new Proto::Connection (this))
	, GM_ (new GroupManager (this))
	{
		connect (Conn_,
				SIGNAL (authenticationError (QString)),
				this,
				SLOT (handleAuthError (QString)));
		connect (Conn_,
				SIGNAL (gotContacts (QList<Proto::ContactInfo>)),
				this,
				SLOT (handleGotContacts (QList<Proto::ContactInfo>)));
		connect (Conn_,
				SIGNAL (userStatusChanged (Proto::ContactInfo)),
				this,
				SLOT (handleUserStatusChanged (Proto::ContactInfo)));
		connect (Conn_,
				SIGNAL (gotUserInfoError (QString, Proto::AnketaInfoStatus)),
				this,
				SLOT (handleGotUserInfoError (QString, Proto::AnketaInfoStatus)));
		connect (Conn_,
				SIGNAL (gotUserInfoResult (QString, QMap<QString, QString>)),
				this,
				SLOT (handleGotUserInfo (QString, QMap<QString, QString>)));
		connect (Conn_,
				SIGNAL (gotAuthRequest (QString, QString)),
				this,
				SLOT (handleGotAuthRequest (QString, QString)));
		connect (Conn_,
				SIGNAL (gotAuthAck (QString)),
				this,
				SLOT (handleGotAuthAck (QString)));
		connect (Conn_,
				SIGNAL (gotMessage (Proto::Message)),
				this,
				SLOT (handleGotMessage (Proto::Message)));
		connect (Conn_,
				SIGNAL (gotOfflineMessage (Proto::Message)),
				this,
				SLOT (handleGotMessage (Proto::Message)));
		connect (Conn_,
				SIGNAL (gotAttentionRequest (QString, QString)),
				this,
				SLOT (handleGotAttentionRequest (QString, QString)));
		connect (Conn_,
				SIGNAL (statusChanged (EntryStatus)),
				this,
				SLOT (handleOurStatusChanged (EntryStatus)));
		connect (Conn_,
				SIGNAL (contactAdded (quint32, quint32)),
				this,
				SLOT (handleContactAdded (quint32, quint32)));
		connect (Conn_,
				SIGNAL (contactAdditionError (quint32, Proto::ContactAck)),
				this,
				SLOT (handleContactAdditionError (quint32, Proto::ContactAck)));
		connect (Conn_,
				SIGNAL (gotUserTune (QString, QString)),
				this,
				SLOT (handleGotUserTune (QString, QString)));
		connect (Conn_,
				SIGNAL (userStartedTyping (QString)),
				this,
				SLOT (handleUserStartedTyping (QString)));
		connect (Conn_,
				SIGNAL (userStoppedTyping (QString)),
				this,
				SLOT (handleUserStoppedTyping (QString)));
		connect (Conn_,
				SIGNAL (gotNewMail (QString, QString)),
				this,
				SLOT (handleGotNewMail (QString, QString)));
		connect (Conn_,
				SIGNAL (gotPOPKey (QString)),
				this,
				SLOT (handleGotPOPKey (QString)));

		QAction *mb = new QAction (tr ("Open mailbox..."), this);
		connect (mb,
				SIGNAL (triggered ()),
				this,
				SLOT (handleOpenMailbox ()));
		Actions_ << mb;
		Actions_ << VaderUtil::GetBuddyServices (this, SLOT (handleServices ()));

		const QString& ua = "LeechCraft Azoth " + Core::Instance ()
				.GetCoreProxy ()->GetVersion ();
		Conn_->SetUA (ua);

		XmlSettingsManager::Instance ().RegisterObject ("ShowSupportContact",
				this, "handleShowTechSupport");
	}

	void MRIMAccount::FillConfig (MRIMAccountConfigWidget *w)
	{
		Login_ = w->GetLogin ();

		const auto& pass = w->GetPassword ();
		if (!pass.isEmpty ())
			Proto_->GetAzothProxy ()->SetPassword (pass, this);
	}

	Proto::Connection* MRIMAccount::GetConnection () const
	{
		return Conn_;
	}

	GroupManager* MRIMAccount::GetGroupManager () const
	{
		return GM_;
	}

	void MRIMAccount::SetTypingState (const QString& to, ChatPartState state)
	{
		Conn_->SetTypingState (to, state == CPSComposing);
	}

	void MRIMAccount::RequestInfo (const QString& email)
	{
		Conn_->RequestInfo (email);
	}

	QObject* MRIMAccount::GetQObject ()
	{
		return this;
	}

	QObject* MRIMAccount::GetParentProtocol () const
	{
		return Proto_;
	}

	IAccount::AccountFeatures MRIMAccount::GetAccountFeatures () const
	{
		return FHasConfigurationDialog;
	}

	QList<QObject*> MRIMAccount::GetCLEntries ()
	{
		QList<QObject*> result;
		result.reserve (Buddies_.size ());
		std::copy (Buddies_.begin (), Buddies_.end (), std::back_inserter (result));
		return result;
	}

	QString MRIMAccount::GetAccountName () const
	{
		return Name_;
	}

	QString MRIMAccount::GetOurNick () const
	{
		return Login_.split ('@', QString::SkipEmptyParts).value (0);
	}

	void MRIMAccount::RenameAccount (const QString& name)
	{
		Name_ = name;
		emit accountRenamed (Name_);
		emit accountSettingsChanged ();
	}

	QByteArray MRIMAccount::GetAccountID () const
	{
		return Proto_->GetProtocolID () + "_" + Login_.toUtf8 ();
	}

	QList<QAction*> MRIMAccount::GetActions () const
	{
		return Actions_;
	}

	void MRIMAccount::OpenConfigurationDialog ()
	{
	}

	EntryStatus MRIMAccount::GetState () const
	{
		return Status_;
	}

	void MRIMAccount::ChangeState (const EntryStatus& status)
	{
		if (!Conn_->IsConnected ())
		{
			const auto& pass = Proto_->GetAzothProxy ()->GetAccountPassword (this);
			Conn_->SetCredentials (Login_, pass);
		}

		Conn_->SetState (status);
	}

	void MRIMAccount::Authorize (QObject *obj)
	{
		qDebug () << Q_FUNC_INFO << GetAccountName ();
		MRIMBuddy *buddy = qobject_cast<MRIMBuddy*> (obj);
		if (!buddy)
		{
			qWarning () << Q_FUNC_INFO
					<< "wrong object"
					<< obj;
			return;
		}

		const QString& id = buddy->GetHumanReadableID ();
		Conn_->Authorize (id);

		buddy->SetAuthorized (true);

		if (!Buddies_.contains (id))
			Buddies_ [id] = buddy;
		if (buddy->GetID () < 0)
		{
			const auto seq = Conn_->AddContact (0, id, buddy->GetEntryName ());
			PendingAdditions_ [seq] = buddy->GetInfo ();
		}
	}

	void MRIMAccount::DenyAuth (QObject *obj)
	{
		qDebug () << Q_FUNC_INFO << GetAccountName ();
		MRIMBuddy *buddy = qobject_cast<MRIMBuddy*> (obj);
		if (!buddy)
		{
			qWarning () << Q_FUNC_INFO
					<< "wrong object"
					<< obj;
			return;
		}

		emit removedCLItems (QList<QObject*> () << buddy);
		Buddies_.value (buddy->GetHumanReadableID (), buddy)->deleteLater ();
		Buddies_.remove (buddy->GetHumanReadableID ());
	}

	void MRIMAccount::RequestAuth (const QString& email,
			const QString& msg, const QString& name, const QStringList&)
	{
		if (!Buddies_.contains (email))
		{
			const quint32 group = 0;
			const quint32 seqId = Conn_->AddContact (group, email, name.isEmpty () ? email : name);
			PendingAdditions_ [seqId] = { -1, group, Proto::UserState::Offline,
					email, QString (), name, QString (), QString (), 0, QString () };
			Conn_->Authorize (email);
		}
		else if (!Buddies_ [email]->GaveSubscription ())
			Conn_->RequestAuth (email, msg);
	}

	void MRIMAccount::RemoveEntry (QObject *obj)
	{
		MRIMBuddy *buddy = qobject_cast<MRIMBuddy*> (obj);
		if (!buddy)
		{
			qWarning () << Q_FUNC_INFO
					<< "wrong object"
					<< obj;
			return;
		}

		const qint64 id = buddy->GetID ();
		if (id < 0)
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot remove buddy with negative ID";
			return;
		}

		Buddies_.take (buddy->GetHumanReadableID ())->deleteLater ();
		emit removedCLItems (QList<QObject*> () << buddy);

		Conn_->RemoveContact (id, buddy->GetHumanReadableID (), buddy->GetEntryName ());
	}

	QObject* MRIMAccount::GetTransferManager () const
	{
		return 0;
	}

	void MRIMAccount::PublishTune (const QMap<QString, QVariant>& data)
	{
		QString string;
		auto sa = [&string, &data] (const QString& name, const QString& sep)
			{
				if (!data [name].toString ().isEmpty ())
				{
					string += sep;
					string += data [name].toString ();
				}
			};
		string += data ["artist"].toString ();
		sa ("title", QString::fromUtf8 (" — "));
		sa ("source", " " + tr ("from") + " ");
		sa ("length", ", ");

		Conn_->PublishTune (string);
	}

	QObject* MRIMAccount::GetSelfContact () const
	{
		return 0;
	}

	QIcon MRIMAccount::GetAccountIcon () const
	{
		return QIcon ();
	}

	QByteArray MRIMAccount::Serialize () const
	{
		QByteArray result;
		QDataStream str (&result, QIODevice::WriteOnly);
		str << static_cast<quint8> (1)
			<< Name_
			<< Login_;

		return result;
	}

	MRIMAccount* MRIMAccount::Deserialize (const QByteArray& ba, MRIMProtocol *proto)
	{
		QDataStream str (ba);
		quint8 ver = 0;
		str >> ver;
		if (ver != 1)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown version"
					<< ver;
			return 0;
		}

		QString name;
		str >> name;
		MRIMAccount *result = new MRIMAccount (name, proto);
		str >> result->Login_;

		return result;
	}

	MRIMBuddy* MRIMAccount::GetBuddy (const Proto::ContactInfo& info)
	{
		if (Buddies_.contains (info.Email_))
		{
			auto buddy = Buddies_ [info.Email_];
			buddy->UpdateInfo (info);
			return buddy;
		}

		MRIMBuddy *buddy = new MRIMBuddy (info, this);
		Buddies_ [info.Email_] = buddy;

		if (info.Email_ != "support@corp.mail.ru" ||
				XmlSettingsManager::Instance ()
					.property ("ShowSupportContact").toBool ())
			emit gotCLItems (QList<QObject*> () << buddy);

		return buddy;
	}

	void MRIMAccount::handleAuthError (const QString& errorString)
	{
		const auto& e = Util::MakeNotification ("Azoth",
				tr ("Authentication error for account %1: server reports %2.")
					.arg ("<em>" + GetAccountName () + "</em>")
					.arg ("<em>" + errorString + "</em>"),
				PCritical_);
		Core::Instance ().SendEntity (e);
	}

	void MRIMAccount::handleGotContacts (const QList<Proto::ContactInfo>& contacts)
	{
		Q_FOREACH (const Proto::ContactInfo& contact, contacts)
		{
			qDebug () << Q_FUNC_INFO
					<< GetAccountName ()
					<< contact.Email_
					<< contact.Phone_
					<< contact.Alias_
					<< contact.ContactID_
					<< contact.UA_
					<< contact.Features_;
			MRIMBuddy *buddy = GetBuddy (contact);

			if (buddy->GetID () != contact.ContactID_)
				buddy->UpdateID (contact.ContactID_);

			buddy->SetGroup (GM_->GetGroup (contact.GroupNumber_));
			Buddies_ [contact.Email_] = buddy;
		}
	}

	void MRIMAccount::handleUserStatusChanged (const Proto::ContactInfo& status)
	{
		MRIMBuddy *buddy = Buddies_ [status.Email_];
		if (!buddy)
		{
			qWarning () << Q_FUNC_INFO
					<< GetAccountName ()
					<< "unknown buddy"
					<< status.Email_;
			return;
		}

		qDebug () << Q_FUNC_INFO << GetAccountName () << status.Email_;

		auto info = buddy->GetInfo ();
		info.Features_ = status.Features_;
		info.StatusDesc_ = status.StatusDesc_;
		info.StatusID_ = status.StatusID_;
		info.StatusTitle_ = status.StatusTitle_;
		info.UA_ = status.UA_;
		buddy->UpdateInfo (info);
	}

	void MRIMAccount::handleContactAdded (quint32 seq, quint32 id)
	{
		qDebug () << Q_FUNC_INFO << GetAccountName () << id;
		if (!PendingAdditions_.contains (seq))
			return;

		auto info = PendingAdditions_.take (seq);
		info.ContactID_ = id;

		const bool existed = Buddies_.contains (info.Email_);
		handleGotContacts (QList<Proto::ContactInfo> () << info);

		if (!existed)
			Buddies_ [info.Email_]->SetGaveSubscription (false);
	}

	void MRIMAccount::handleContactAdditionError (quint32 seq, Proto::ContactAck ack)
	{
		qDebug () << Q_FUNC_INFO << GetAccountName () << static_cast<quint32> (ack);
		if (!PendingAdditions_.contains (seq))
			return;

		auto reason = tr ("unknown");
		switch (ack)
		{
		case Proto::ContactAck::Error:
		case Proto::ContactAck::IntErr:
			reason = tr ("server error");
			break;
		case Proto::ContactAck::NoSuchUser:
			reason = tr ("no such user");
			break;
		case Proto::ContactAck::InvalidInfo:
			reason = tr ("invalid user information");
			break;
		case Proto::ContactAck::UserExists:
			reason = tr ("user exists already in contact list");
			break;
		case Proto::ContactAck::GroupLimit:
			reason = tr ("group limit exceeded");
			break;
		case Proto::ContactAck::Success:
			break;
		}

		const auto& info = PendingAdditions_.take (seq);
		const auto& e = Util::MakeNotification ("Azoth Vader",
				tr ("Unable to add contact %1: %2.")
					.arg (info.Email_)
					.arg (reason),
				PCritical_);

		const auto& proxy = Core::Instance ().GetCoreProxy ();
		proxy->GetEntityManager ()->HandleEntity (e);
	}

	void MRIMAccount::handleGotUserInfoError (const QString& id,
			Proto::AnketaInfoStatus status)
	{
		QString error;
		switch (status)
		{
		case Proto::AnketaInfoStatus::DBErr:
			error = tr ("database error");
			break;
		case Proto::AnketaInfoStatus::NoUser:
			error = tr ("no such user");
			break;
		case Proto::AnketaInfoStatus::RateLimit:
			error = tr ("rate limit exceeded");
			break;
		default:
			error = tr ("unknown error");
			break;
		}

		const Entity& e = Util::MakeNotification ("Azoth",
				tr ("Error fetching user info for %1: %2.")
					.arg (id)
					.arg (error),
				PCritical_);
		Core::Instance ().SendEntity (e);
	}

	void MRIMAccount::handleGotUserInfo (const QString& from,
			const QMap<QString, QString>& values)
	{
		if (!Buddies_.contains (from))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown buddy"
					<< from;
			return;
		}

		Buddies_ [from]->HandleWPInfo (values);
	}

	void MRIMAccount::handleGotAuthRequest (const QString& from, const QString& msg)
	{
		qDebug () << Q_FUNC_INFO << GetAccountName () << from;
		MRIMBuddy *buddy = 0;
		if (Buddies_.contains (from))
			buddy = Buddies_ [from];
		else
		{
			const Proto::ContactInfo info = {-1, 0, Proto::UserState::Online,
					from, from, QString (), QString (), QString (), 0, QString () };
			buddy = new MRIMBuddy (info, this);
			emit gotCLItems (QList<QObject*> () << buddy);
			Buddies_ [from] = buddy;
		}

		buddy->SetAuthorized (false);
		emit authorizationRequested (buddy, msg);
	}

	void MRIMAccount::handleGotAuthAck (const QString& from)
	{
		qDebug () << Q_FUNC_INFO << GetAccountName () << from;
		if (!Buddies_.contains (from))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown buddy"
					<< from;
			return;
		}

		auto buddy = Buddies_ [from];
		buddy->SetGaveSubscription (true);

		emit itemGrantedSubscription (buddy, QString ());
	}

	void MRIMAccount::handleGotMessage (const Proto::Message& msg)
	{
		if (!Buddies_.contains (msg.From_))
		{
			qWarning () << Q_FUNC_INFO
					<< "incoming message from unknown buddy"
					<< msg.From_
					<< msg.Text_
					<< msg.DT_;
			return;
		}

		auto buddy = Buddies_ [msg.From_];
		MRIMMessage *obj = new MRIMMessage (IMessage::Direction::In, IMessage::Type::ChatMessage, buddy);
		obj->SetBody (msg.Text_);
		obj->SetDateTime (msg.DT_);
		buddy->HandleMessage (obj);
	}

	void MRIMAccount::handleGotAttentionRequest (const QString& from, const QString& msg)
	{
		if (!Buddies_.contains (from))
		{
			qWarning () << Q_FUNC_INFO
					<< "from unknown buddy"
					<< from;
			return;
		}

		Buddies_ [from]->HandleAttention (msg);
	}

	void MRIMAccount::handleOurStatusChanged (const EntryStatus& status)
	{
		if (status.State_ == SOffline)
			Q_FOREACH (MRIMBuddy *buddy, Buddies_.values ())
			{
				auto info = buddy->GetInfo ();
				info.StatusID_ = Proto::UserState::Offline;
				info.StatusDesc_.clear ();
				info.StatusTitle_.clear ();
				buddy->UpdateInfo (info);
			}

		Status_ = status;
		emit statusChanged (status);
	}

	void MRIMAccount::handleGotUserTune (const QString& from, const QString& tune)
	{
		if (!Buddies_.contains (from))
		{
			qWarning () << Q_FUNC_INFO
					<< "from unknown buddy"
					<< from;
			return;
		}

		Buddies_ [from]->HandleTune (tune);
	}

	void MRIMAccount::handleUserStartedTyping (const QString& from)
	{
		if (!Buddies_.contains (from))
		{
			qWarning () << Q_FUNC_INFO
					<< "from unknown buddy"
					<< from;
			return;
		}

		Buddies_ [from]->HandleCPS (CPSComposing);
	}

	void MRIMAccount::handleUserStoppedTyping (const QString& from)
	{
		if (!Buddies_.contains (from))
		{
			qWarning () << Q_FUNC_INFO
					<< "from unknown buddy"
					<< from;
			return;
		}

		Buddies_ [from]->HandleCPS (CPSPaused);
	}

	void MRIMAccount::handleGotNewMail (const QString& from, const QString& subj)
	{
		const Entity& e = Util::MakeNotification (Login_,
				tr ("New mail from %1: %2.")
					.arg (from)
					.arg (subj),
				PInfo_);
		Core::Instance ().SendEntity (e);
	}

	void MRIMAccount::handleGotPOPKey (const QString& key)
	{
		qDebug () << Q_FUNC_INFO << key;
		const QString& str = QString ("http://win.mail.ru/cgi-bin/auth?Login=%1&agent=%2")
				.arg (Login_)
				.arg (key);
		const Entity& e = Util::MakeEntity (QUrl (str),
				QString (),
				static_cast<LeechCraft::TaskParameters> (OnlyHandle | FromUserInitiated));
		Core::Instance ().SendEntity (e);
	}

	void MRIMAccount::handleOpenMailbox ()
	{
		Conn_->RequestPOPKey ();
	}

	void MRIMAccount::handleServices ()
	{
		const QString& url = sender ()->property ("URL").toString ();
		const QString& subst = VaderUtil::SubstituteNameDomain (url, Login_);
		qDebug () << Q_FUNC_INFO << subst << url << Login_;
		const Entity& e = Util::MakeEntity (QUrl (subst),
				QString (),
				static_cast<LeechCraft::TaskParameters> (OnlyHandle | FromUserInitiated));
		Core::Instance ().SendEntity (e);
	}

	void MRIMAccount::handleShowTechSupport ()
	{
		if (!Buddies_.contains ("support@corp.mail.ru"))
			return;

		const bool show = XmlSettingsManager::Instance ()
				.property ("ShowSupportContact").toBool ();
		QList<QObject*> list;
		list << Buddies_ ["support@corp.mail.ru"];
		emit show ?
				gotCLItems (list) :
				removedCLItems (list);
	}
}
}
}
