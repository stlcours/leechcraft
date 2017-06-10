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

#include "roomhandler.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QtDebug>
#include <QXmppVCardIq.h>
#include <QXmppMucManager.h>
#include <QXmppClient.h>
#include <util/sll/delayedexecutor.h>
#include <util/sll/util.h>
#include <util/sll/eithercont.h>
#include <util/xpc/passutils.h>
#include <interfaces/azoth/iproxyobject.h>
#include "glooxaccount.h"
#include "roomclentry.h"
#include "roompublicmessage.h"
#include "roomparticipantentry.h"
#include "clientconnection.h"
#include "glooxmessage.h"
#include "util.h"
#include "glooxprotocol.h"
#include "formbuilder.h"
#include "sdmanager.h"
#include "core.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	const QString NSData = "jabber:x:data";

	RoomHandler::RoomHandler (const QString& jid,
			const QString& ourNick,
			bool asAutojoin,
			GlooxAccount* account)
	: Account_ (account)
	, MUCManager_ (Account_->GetClientConnection ()->GetMUCManager ())
	, RoomJID_ (jid)
	, Room_ (MUCManager_->addRoom (jid))
	, CLEntry_ (new RoomCLEntry (this, asAutojoin, Account_))
	, HadRequestedPassword_ (false)
	{
		const QString& server = jid.split ('@', QString::SkipEmptyParts).value (1);
		auto sdManager = Account_->GetClientConnection ()->GetSDManager ();

		QPointer<RoomHandler> pThis (this);
		sdManager->RequestInfo ([pThis] (const QXmppDiscoveryIq& iq)
					{ if (pThis) pThis->ServerDisco_ = iq; },
				server);

		Room_->setNickName (ourNick);

		connect (Room_,
				SIGNAL (participantChanged (const QString&)),
				this,
				SLOT (handleParticipantChanged (const QString&)));
		connect (Room_,
				SIGNAL (participantAdded (const QString&)),
				this,
				SLOT (handleParticipantAdded (const QString&)));
		connect (Room_,
				SIGNAL (participantRemoved (const QString&)),
				this,
				SLOT (handleParticipantRemoved (const QString&)));

		connect (this,
				SIGNAL (gotPendingForm (QXmppDataForm*, const QString&)),
				Account_->GetClientConnection ().get (),
				SLOT (handlePendingForm (QXmppDataForm*, const QString&)),
				Qt::QueuedConnection);

		new Util::DelayedExecutor
		{
			[this] { Room_->join (); },
			0
		};
	}

	QString RoomHandler::GetRoomJID () const
	{
		return RoomJID_;
	}

	RoomCLEntry* RoomHandler::GetCLEntry ()
	{
		return CLEntry_;
	}

	void RoomHandler::HandleVCard (const QXmppVCardIq& card, const QString& nick)
	{
		if (!Nick2Entry_.contains (nick))
		{
			qWarning () << Q_FUNC_INFO
					<< "no such nick"
					<< nick
					<< "; available:"
					<< Nick2Entry_.keys ();
			return;
		}

		Nick2Entry_ [nick]->SetVCard (card);
	}

	void RoomHandler::SetPresence (QXmppPresence pres)
	{
		if (pres.type () == QXmppPresence::Unavailable)
			Leave (pres.statusText (), false);
		else if (!Room_->isJoined ())
			Join ();
	}

	/** @todo Detect kicks, bans and the respective actor.
	 */
	void RoomHandler::MakeLeaveMessage (const QXmppPresence& pres, const QString& nick)
	{
		QString msg = tr ("%1 has left the room").arg (nick);
		if (pres.statusText ().size ())
			msg += ": " + pres.statusText ();

		RoomPublicMessage *message = new RoomPublicMessage (msg,
				IMessage::Direction::In,
				CLEntry_,
				IMessage::Type::StatusMessage,
				IMessage::SubType::ParticipantLeave,
				GetParticipantEntry (nick));
		CLEntry_->HandleMessage (message);
	}

	void RoomHandler::MakeJoinMessage (const QXmppPresence& pres, const QString& nick)
	{
		QString affiliation = XooxUtil::AffiliationToString (pres.mucItem ().affiliation ());
		QString role = XooxUtil::RoleToString (pres.mucItem ().role ());
		QString realJid = pres.mucItem ().jid ();
		QString msg;
		if (realJid.isEmpty ())
			msg = tr ("%1 joined the room as %2 and %3")
					.arg (nick)
					.arg (role)
					.arg (affiliation);
		else
			msg = tr ("%1 (%2) joined the room as %3 and %4")
					.arg (nick)
					.arg (realJid)
					.arg (role)
					.arg (affiliation);

		RoomPublicMessage *message = new RoomPublicMessage (msg,
				IMessage::Direction::In,
				CLEntry_,
				IMessage::Type::StatusMessage,
				IMessage::SubType::ParticipantJoin,
				GetParticipantEntry (nick));
		CLEntry_->HandleMessage (message);
	}

	void RoomHandler::MakeStatusChangedMessage (const QXmppPresence& pres, const QString& nick)
	{
		const auto proxy = Account_->GetParentProtocol ()->GetProxyObject ();
		const auto& state = proxy->StateToString (static_cast<State> (pres.availableStatusType () + 1));
		const auto& msg = tr ("%1 changed status to %2 (%3)")
				.arg (nick)
				.arg (state)
				.arg (pres.statusText ());

		RoomPublicMessage *message = new RoomPublicMessage (msg,
				IMessage::Direction::In,
				CLEntry_,
				IMessage::Type::StatusMessage,
				IMessage::SubType::ParticipantStatusChange,
				GetParticipantEntry (nick));
		message->setProperty ("Azoth/Nick", nick);
		message->setProperty ("Azoth/TargetState", state);
		message->setProperty ("Azoth/StatusText", pres.statusText ());
		CLEntry_->HandleMessage (message);
	}

	void RoomHandler::MakeNickChangeMessage (const QString& oldNick, const QString& newNick)
	{
		QString msg = tr ("%1 changed nick to %2")
				.arg (oldNick)
				.arg (newNick);

		auto message = new RoomPublicMessage (msg,
				IMessage::Direction::In,
				CLEntry_,
				IMessage::Type::StatusMessage,
				IMessage::SubType::ParticipantNickChange,
				GetParticipantEntry (newNick));
		CLEntry_->HandleMessage (message);
	}

	void RoomHandler::MakeKickMessage (const QString& nick, const QString& reason)
	{
		QString msg;
		if (reason.isEmpty ())
			msg = tr ("%1 has been kicked")
					.arg (nick);
		else
			msg = tr ("%1 has been kicked: %2")
					.arg (nick)
					.arg (reason);

		RoomPublicMessage *message = new RoomPublicMessage (msg,
				IMessage::Direction::In,
				CLEntry_,
				IMessage::Type::StatusMessage,
				IMessage::SubType::KickNotification,
				GetParticipantEntry (nick));
		CLEntry_->HandleMessage (message);
	}

	void RoomHandler::MakeBanMessage (const QString& nick, const QString& reason)
	{
		QString msg;
		if (reason.isEmpty ())
			msg = tr ("%1 has been banned")
					.arg (nick);
		else
			msg = tr ("%1 has been banned: %2")
					.arg (nick)
					.arg (reason);

		RoomPublicMessage *message = new RoomPublicMessage (msg,
				IMessage::Direction::In,
				CLEntry_,
				IMessage::Type::StatusMessage,
				IMessage::SubType::BanNotification,
				GetParticipantEntry (nick));
		CLEntry_->HandleMessage (message);
	}

	void RoomHandler::MakePermsChangedMessage (const QString& nick,
			QXmppMucItem::Affiliation aff,
			QXmppMucItem::Role role, const QString& reason)
	{
		const QString& affStr = XooxUtil::AffiliationToString (aff);
		const QString& roleStr = XooxUtil::RoleToString (role);
		QString msg;
		if (reason.isEmpty ())
			msg = tr ("%1 is now %2 and %3")
					.arg (nick)
					.arg (roleStr)
					.arg (affStr);
		else
			msg = tr ("%1 is now %2 and %3: %4")
					.arg (nick)
					.arg (roleStr)
					.arg (affStr)
					.arg (reason);

		RoomPublicMessage *message = new RoomPublicMessage (msg,
				IMessage::Direction::In,
				CLEntry_,
				IMessage::Type::StatusMessage,
				IMessage::SubType::ParticipantRoleAffiliationChange,
				GetParticipantEntry (nick));
		CLEntry_->HandleMessage (message);
	}

	void RoomHandler::HandleNickConflict ()
	{
		// The room is already joined, should do nothing special here.
		if (Room_->isJoined ())
			return;

		emit CLEntry_->nicknameConflict (Room_->nickName ());
	}

	void RoomHandler::HandlePasswordRequired ()
	{
		const auto& text = tr ("This room is password-protected. Please enter the "
				"password required to join this room.");
		Util::GetPassword (GetPassKey (),
				text,
				Core::Instance ().GetProxy (),
				{
					[this] { Leave ({}); },
					[this] (const QString& pass)
					{
						HadRequestedPassword_ = true;
						Room_->setPassword (pass);
						Join ();
					}
				},
				this,
				!HadRequestedPassword_);
	}

	QString RoomHandler::GetPassKey () const
	{
		return "org.LeechCraft.Azoth.Xoox.MUCpass_" + CLEntry_->GetHumanReadableID ();
	}

	void RoomHandler::HandleErrorPresence (const QXmppPresence& pres, const QString& nick)
	{
		const QString& errorText = pres.error ().text ();
		QString hrText;
		switch (pres.error ().condition ())
		{
		case QXmppStanza::Error::Conflict:
			hrText = tr ("nickname already taken");
			break;
		case QXmppStanza::Error::Forbidden:
		case QXmppStanza::Error::NotAllowed:
			hrText = tr ("access forbidden");
			break;
		case QXmppStanza::Error::NotAuthorized:
			hrText = tr ("password required");
			break;
		case QXmppStanza::Error::JidMalformed:
			hrText = tr ("malformed JID");
			break;
		case QXmppStanza::Error::RegistrationRequired:
			hrText = tr ("only registered users can enter this room");
			break;
		case QXmppStanza::Error::RemoteServerNotFound:
			hrText = tr ("remote server not found (try contacting your server's administrator)");
			break;
		case QXmppStanza::Error::RemoteServerTimeout:
			hrText = tr ("timeout connecting to remote server (try contacting your server's administrator)");
			break;
		case QXmppStanza::Error::ServiceUnavailable:
			hrText = tr ("service unavailable");
			break;
		default:
			hrText = tr ("unknown condition %1 (please report to developers)")
				.arg (pres.error ().condition ());
			break;
		}
		const QString& text = tr ("Error for %1: %2 (original message: %3)")
				.arg (nick)
				.arg (hrText)
				.arg (errorText.isEmpty () ?
						tr ("no message") :
						errorText);
		RoomPublicMessage *message = new RoomPublicMessage (text,
				IMessage::Direction::In,
				CLEntry_,
				IMessage::Type::EventMessage,
				IMessage::SubType::Other);
		CLEntry_->HandleMessage (message);

		switch (pres.error ().condition ())
		{
		case QXmppStanza::Error::Conflict:
			HandleNickConflict ();
			break;
		case QXmppStanza::Error::NotAuthorized:
			HandlePasswordRequired ();
			break;
		default:
			break;
		}
	}

	void RoomHandler::HandlePermsChanged (const QString& nick,
			QXmppMucItem::Affiliation aff,
			QXmppMucItem::Role role,
			const QString& reason)
	{
		const auto& entry = GetParticipantEntry (nick);
		if (aff == QXmppMucItem::OutcastAffiliation ||
				role == QXmppMucItem::NoRole)
		{
			Account_->handleEntryRemoved (entry.get ());

			if (aff == QXmppMucItem::OutcastAffiliation)
				MakeBanMessage (nick, reason);
			else
				MakeKickMessage (nick, reason);

			Nick2Entry_.remove (nick);

			return;
		}

		entry->SetAffiliation (aff);
		entry->SetRole (role);
		MakePermsChangedMessage (nick, aff, role, reason);
	}

	void RoomHandler::HandleMessageExtensions (const QXmppMessage& msg)
	{
		for (const auto& elem : msg.extensions ())
		{
			const auto& xmlns = elem.attribute ("xmlns");
			if (xmlns == NSData)
			{
				const auto df = new QXmppDataForm ();
				df->parse (XooxUtil::XmppElem2DomElem (elem));
				if (df->isNull ())
				{
					qWarning () << Q_FUNC_INFO
							<< "unable to parse form from"
							<< msg.from ();
					delete df;
				}
				else
					emit gotPendingForm (df, msg.from ());
			}
			else
			{
				QString str;
				QXmlStreamWriter w (&str);
				w.setAutoFormatting (true);
				elem.toXml (&w);
				qWarning () << Q_FUNC_INFO
						<< "unhandled <x> element"
						<< str;
			}
		}
	}

	void RoomHandler::HandleMessage (const QXmppMessage& msg, const QString& nick)
	{
		HandleMessageExtensions (msg);

		const bool existed = Nick2Entry_.contains (nick);
		const auto& entry = GetParticipantEntry (nick, false);
		if (msg.type () == QXmppMessage::Chat && !nick.isEmpty ())
		{
			if (msg.isAttentionRequested ())
				entry->HandleAttentionMessage (msg);

			if (msg.state ())
				entry->UpdateChatState (msg.state (), QString ());

			if (!msg.body ().isEmpty ())
			{
				GlooxMessage *message = new GlooxMessage (msg,
						Account_->GetClientConnection ().get ());
				entry->HandleMessage (message);
			}
		}
		else
		{
			RoomPublicMessage *message = nullptr;
			if (msg.type () == QXmppMessage::GroupChat &&
				!msg.subject ().isEmpty ())
			{
				Subject_ = msg.subject ();
				CLEntry_->HandleSubjectChanged (Subject_);

				const QString& string = nick.isEmpty () ?
						msg.subject () :
						tr ("%1 changed subject to %2")
							.arg (nick)
							.arg (msg.subject ());

				message = new RoomPublicMessage (string,
					IMessage::Direction::In,
					CLEntry_,
					IMessage::Type::EventMessage,
					IMessage::SubType::RoomSubjectChange);
			}
			else if (!nick.isEmpty ())
			{
				if (!msg.body ().isEmpty ())
					message = new RoomPublicMessage (msg, CLEntry_, entry);
			}
			else if (!msg.body ().isEmpty ())
				message = new RoomPublicMessage (msg.body (),
					IMessage::Direction::In,
					CLEntry_,
					IMessage::Type::EventMessage,
					IMessage::SubType::Other);

			if (message)
				CLEntry_->HandleMessage (message);

			if (!existed)
				RemoveEntry (entry.get ());
		}
	}

	void RoomHandler::UpdatePerms (const QList<QXmppMucItem>& perms)
	{
		for (const auto& item : perms)
		{
			if (!Nick2Entry_.contains (item.nick ()))
			{
				qWarning () << Q_FUNC_INFO
						<< "no participant with nick"
						<< item.nick ()
						<< Nick2Entry_.keys ();
				continue;
			}

			Nick2Entry_ [item.nick ()]->SetAffiliation (item.affiliation ());
			Nick2Entry_ [item.nick ()]->SetRole (item.role ());
		}
	}

	GlooxMessage* RoomHandler::CreateMessage (IMessage::Type,
			const QString& nick, const QString& body)
	{
		GlooxMessage *message = new GlooxMessage (IMessage::Type::ChatMessage,
				IMessage::Direction::Out,
				GetRoomJID (),
				nick,
				Account_->GetClientConnection ().get ());
		message->SetBody (body);
		message->SetDateTime (QDateTime::currentDateTime ());
		return message;
	}

	QList<QObject*> RoomHandler::GetParticipants () const
	{
		QList<QObject*> result;
		for (const auto& rpe : Nick2Entry_)
			result << rpe.get ();
		return result;
	}

	QString RoomHandler::GetSubject () const
	{
		return Subject_;
	}

	void RoomHandler::Join ()
	{
		if (Room_->isJoined ())
			return;

		Room_->join ();
	}

	void RoomHandler::SetSubject (const QString& subj)
	{
		Room_->setSubject (subj);
	}

	void RoomHandler::Leave (const QString& msg, bool remove)
	{
		for (const auto& entry : Nick2Entry_)
			Account_->handleEntryRemoved (entry.get ());

		Room_->leave (msg);
		Nick2Entry_.clear ();

		if (remove)
			RemoveThis ();
	}

	RoomParticipantEntry* RoomHandler::GetSelf ()
	{
		return GetParticipantEntry (Room_->nickName ()).get ();
	}

	QString RoomHandler::GetOurNick () const
	{
		return Room_->nickName ();
	}

	void RoomHandler::SetOurNick (const QString& nick)
	{
		Room_->setNickName (nick);
	}

	void RoomHandler::SetAffiliation (RoomParticipantEntry *entry,
			QXmppMucItem::Affiliation newAff, const QString& reason)
	{
		QXmppMucItem item;
		item.setNick (entry->GetNick ());
		item.setReason (reason);
		item.setAffiliation (newAff);
		Account_->GetClientConnection ()->Update (item, Room_->jid ());
	}

	void RoomHandler::SetRole (RoomParticipantEntry *entry,
			QXmppMucItem::Role newRole, const QString& reason)
	{
		QXmppMucItem item;
		item.setNick (entry->GetNick ());
		item.setReason (reason);
		item.setRole (newRole);
		Account_->GetClientConnection ()->Update (item, Room_->jid ());
	}

	QXmppMucRoom* RoomHandler::GetRoom () const
	{
		return Room_;
	}

	void RoomHandler::HandleRenameStart (const RoomParticipantEntry_ptr& entry,
			const QString& nick, const QString& newNick)
	{
		if (!Nick2Entry_.contains (newNick))
		{
			const auto& newEntry = GetParticipantEntry (newNick, false);
			newEntry->SetAffiliation (entry->GetAffiliation ());
			newEntry->SetRole (entry->GetRole ());
			Account_->handleGotRosterItems ({ newEntry.get () });
		}

		PendingNickChanges_ << newNick;

		const auto& otherEntry = Nick2Entry_.value (newNick);
		otherEntry->StealMessagesFrom (entry.get ());
		CLEntry_->MoveMessages (entry, otherEntry);

		MakeNickChangeMessage (nick, newNick);
		Account_->handleEntryRemoved (Nick2Entry_.value (nick).get ());
		Nick2Entry_.remove (nick);
	}

	RoomParticipantEntry_ptr RoomHandler::CreateParticipantEntry (const QString& nick, bool announce)
	{
		RoomParticipantEntry_ptr entry (new RoomParticipantEntry (nick,
					this, Account_));
		connect (entry.get (),
				SIGNAL (chatTabClosed ()),
				this,
				SLOT (handleChatTabClosed ()),
				Qt::QueuedConnection);
		Nick2Entry_ [nick] = entry;
		if (announce)
			Account_->handleGotRosterItems ({ entry.get () });
		return entry;
	}

	RoomParticipantEntry_ptr RoomHandler::GetParticipantEntry (const QString& nick, bool announce)
	{
		if (!Nick2Entry_.contains (nick))
			return CreateParticipantEntry (nick, announce);
		else
			return Nick2Entry_ [nick];
	}

	void RoomHandler::handleParticipantAdded (const QString& jid)
	{
		const QXmppPresence& pres = Room_->participantPresence (jid);

		QString nick;
		ClientConnection::Split (jid, 0, &nick);

		const bool existed = Nick2Entry_.contains (nick);

		const auto& entry = GetParticipantEntry (nick, false);

		if (PendingNickChanges_.remove (nick))
		{
			entry->HandlePresence (pres, {});
			return;
		}

		entry->SetAffiliation (pres.mucItem ().affiliation ());
		entry->SetRole (pres.mucItem ().role ());

		entry->HandlePresence (pres, {});

		if (!existed)
			Account_->handleGotRosterItems ({ entry.get () });

		MakeJoinMessage (pres, nick);
	}

	void RoomHandler::handleParticipantChanged (const QString& jid)
	{
		const QXmppPresence& pres = Room_->participantPresence (jid);

		QString nick;
		ClientConnection::Split (jid, 0, &nick);

		RoomParticipantEntry_ptr entry = GetParticipantEntry (nick);

		entry->HandlePresence (pres, QString ());
		if (XooxUtil::PresenceToStatus (pres) != entry->GetStatus (QString ()))
			MakeStatusChangedMessage (pres, nick);

		const QXmppMucItem& item = pres.mucItem ();
		if (item.affiliation () != entry->GetAffiliation () ||
				item.role () != entry->GetRole ())
			HandlePermsChanged (nick,
					item.affiliation (), item.role (), item.reason ());
	}

	void RoomHandler::handleParticipantRemoved (const QString& jid)
	{
		const QXmppPresence& pres = Room_->participantPresence (jid);

		QString nick;
		ClientConnection::Split (jid, 0, &nick);

		const bool us = Room_->nickName () == nick;

		const auto& entry = GetParticipantEntry (nick);
		const QXmppMucItem& item = pres.mucItem ();
		if (!item.nick ().isEmpty () &&
				item.nick () != nick)
		{
			HandleRenameStart (entry, nick, item.nick ());
			return;
		}
		else if (pres.mucStatusCodes ().contains (301))
			!us ?
				MakeBanMessage (nick, item.reason ()) :
				static_cast<void> (QMetaObject::invokeMethod (CLEntry_,
							"beenBanned",
							Qt::QueuedConnection,
							Q_ARG (QString, item.reason ())));
		else if (pres.mucStatusCodes ().contains (307))
			!us ?
				MakeKickMessage (nick, item.reason ()) :
				static_cast<void> (QMetaObject::invokeMethod (CLEntry_,
							"beenKicked",
							Qt::QueuedConnection,
							Q_ARG (QString, item.reason ())));
		else
			MakeLeaveMessage (pres, nick);

		const auto checkRejoin = Util::MakeScopeGuard ([this]
				{
					if (Nick2Entry_.isEmpty () ||
							std::all_of (Nick2Entry_.begin (), Nick2Entry_.end (),
									[] (const RoomParticipantEntry_ptr& entry)
										{ return entry->GetStatus ({}).State_ == SOffline; }))
						new Util::DelayedExecutor { [this] { Join (); }, 5000, this };
				});

		if (us)
		{
			Leave (QString (), false);
			return;
		}

		if (entry->HasUnreadMsgs ())
			entry->SetStatus (EntryStatus (SOffline, item.reason ()),
					QString (), QXmppPresence (QXmppPresence::Unavailable));
		else
			RemoveEntry (entry.get ());
	}

	void RoomHandler::requestVoice ()
	{
		QList<QXmppDataForm::Field> fields;

		QXmppDataForm::Field typeField (QXmppDataForm::Field::HiddenField);
		typeField.setKey ("FORM_TYPE");
		typeField.setValue ("http://jabber.org/protocol/muc#request");
		fields << typeField;

		QXmppDataForm::Field reqField (QXmppDataForm::Field::TextSingleField);
		reqField.setLabel ("Requested role");
		reqField.setKey ("muc#role");
		reqField.setValue ("participant");
		fields << reqField;

		QXmppDataForm form;
		form.setType (QXmppDataForm::Submit);
		form.setFields (fields);

		QXmppMessage msg ("", Room_->jid ());
		msg.setType (QXmppMessage::Normal);
		msg.setExtensions (QXmppElementList () << XooxUtil::Form2XmppElem (form));

		Account_->GetClientConnection ()->GetClient ()->sendPacket (msg);
	}

	void RoomHandler::handleChatTabClosed ()
	{
		auto entry = qobject_cast<RoomParticipantEntry*> (sender ());
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< sender ()
					<< "is not a RoomParticipantEntry";
			return;
		}

		if (entry->GetStatus (QString ()).State_ == SOffline)
			RemoveEntry (entry);
	}

	bool RoomHandler::IsGateway () const
	{
		if (ServerDisco_.identities ().size () != 1)
			return true;

		auto id = ServerDisco_.identities ().at (0);
		return id.category () == "conference" && id.type () != "text";
	}

	void RoomHandler::RemoveEntry (RoomParticipantEntry *entry)
	{
		Account_->handleEntryRemoved (entry);
		Nick2Entry_.remove (entry->GetNick ());
	}

	void RoomHandler::RemoveThis ()
	{
		Account_->GetClientConnection ()->Unregister (this);
		Account_->handleEntryRemoved (CLEntry_);
		Room_->deleteLater ();
		Room_ = 0;
		deleteLater ();
	}
}
}
}
