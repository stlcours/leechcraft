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

#include "chattab.h"
#include <QWebFrame>
#include <QWebElement>
#include <QTextDocument>
#include <QTimer>
#include <QPalette>
#include <QApplication>
#include <QShortcut>
#include <plugininterface/defaulthookproxy.h>
#include <plugininterface/util.h>
#include "interfaces/iclentry.h"
#include "interfaces/imessage.h"
#include "interfaces/iaccount.h"
#include "interfaces/imucentry.h"
#include "core.h"

namespace LeechCraft
{
	namespace Plugins
	{
		namespace Azoth
		{
			QObject *ChatTab::S_ParentMultiTabs_ = 0;

			void ChatTab::SetParentMultiTabs (QObject *obj)
			{
				S_ParentMultiTabs_ = obj;
			}

			ChatTab::ChatTab (QObject *entryObj,
					const QString& variant, QWidget *parent)
			: QWidget (parent)
			, Entry_ (entryObj)
			, Variant_ (variant)
			, LinkRegexp_ ("(\\b(?:(?:https?|ftp)://|www.|xmpp:)[\\w\\d/\\?.=:@&%#_;\\(?:\\)\\+\\-\\~\\*\\,]+)",
					Qt::CaseInsensitive, QRegExp::RegExp2)
			, BgColor_ (QApplication::palette ().color (QPalette::Base))
			, CurrentHistoryPosition_ (-1)
			{
				Ui_.setupUi (this);
				Ui_.View_->page ()->setLinkDelegationPolicy (QWebPage::DelegateAllLinks);
				QFile file (":/plugins/azoth/resources/html/viewcontents.html");
				if (!file.open (QIODevice::ReadOnly))
				{
					qWarning () << Q_FUNC_INFO
							<< "could not open resource file"
							<< file.errorString ();
				}
				else
				{
					QString data = file.readAll ();
					data.replace ("BACKGROUNDCOLOR",
							BgColor_.name ());
					data.replace ("FOREGROUNDCOLOR",
							QApplication::palette ().color (QPalette::Text).name ());
					Ui_.View_->setHtml (data);
				}

				connect (Ui_.View_->page (),
						SIGNAL (linkClicked (const QUrl&)),
						this,
						SLOT (handleViewLinkClicked (const QUrl&)));

				connect (entryObj,
						SIGNAL (gotMessage (QObject*)),
						this,
						SLOT (handleEntryMessage (QObject*)));

				Plugins::ICLEntry *e = GetEntry<Plugins::ICLEntry> ();
				Q_FOREACH (Plugins::IMessage *msg, e->GetAllMessages ())
					AppendMessage (msg);

				CheckMUC ();

				QShortcut *histUp = new QShortcut (Qt::CTRL + Qt::Key_Up,
						Ui_.MsgEdit_, 0, 0, Qt::WidgetShortcut);
				connect (histUp,
						SIGNAL (activated ()),
						this,
						SLOT (handleHistoryUp ()));

				QShortcut *histDown = new QShortcut (Qt::CTRL + Qt::Key_Down,
						Ui_.MsgEdit_, 0, 0, Qt::WidgetShortcut);
				connect (histDown,
						SIGNAL (activated ()),
						this,
						SLOT (handleHistoryDown ()));

				Ui_.MsgEdit_->setFocus ();
			}

			QList<QAction*> ChatTab::GetTabBarContextMenuActions () const
			{
				return QList<QAction*> ();
			}

			QObject* ChatTab::ParentMultiTabs () const
			{
				return S_ParentMultiTabs_;
			}

			void ChatTab::NewTabRequested ()
			{
			}

			QToolBar* ChatTab::GetToolBar () const
			{
				return 0;
			}

			void ChatTab::Remove ()
			{
				emit needToClose (this);
			}

			void ChatTab::on_MsgEdit__returnPressed ()
			{
				QString text = Ui_.MsgEdit_->text ();
				if (text.isEmpty ())
					return;

				Ui_.MsgEdit_->clear ();
				CurrentHistoryPosition_ = -1;
				MsgHistory_.prepend (text);

				Plugins::ICLEntry *e = GetEntry<Plugins::ICLEntry> ();
				QStringList currentVariants = e->Variants ();
				QString variant = currentVariants.contains (Variant_) ?
						Variant_ :
						currentVariants.first ();
				Plugins::IMessage::MessageType type =
						e->GetEntryFeatures () & Plugins::ICLEntry::FIsMUC ?
								Plugins::IMessage::MTMUCMessage :
								Plugins::IMessage::MTChatMessage;
				Plugins::IMessage *msg = e->CreateMessage (type, variant, text);
				msg->Send ();
				AppendMessage (msg);
			}

			void ChatTab::handleEntryMessage (QObject *msgObj)
			{
				Plugins::IMessage *msg = qobject_cast<Plugins::IMessage*> (msgObj);
				if (!msg)
				{
					qWarning () << Q_FUNC_INFO
							<< msgObj
							<< "doesn't implement IMessage"
							<< sender ();
					return;
				}

				AppendMessage (msg);

				if (msg->GetDirection () == Plugins::IMessage::DIn &&
						msg->GetMessageType () != Plugins::IMessage::MTMUCMessage &&
						!isVisible ())
				{
					Entity e = Util::MakeNotification ("Azoth",
							tr ("Incoming chat message from %1.")
								.arg (msg->OtherPart ()->GetEntryName ()),
							PInfo_);
					Core::Instance ().SendEntity (e);
				}
			}

			void ChatTab::handleViewLinkClicked (const QUrl& url)
			{
				qDebug () << url.scheme () << url.host () << url.path ();
				if (url.scheme () != "azoth")
				{
					Entity e = Util::MakeEntity (url,
							QString (), FromUserInitiated);
					Core::Instance ().SendEntity (e);
					return;
				}

				if (url.host () == "msgeditreplace")
				{
					Ui_.MsgEdit_->setText (url.path ().mid (1));
					Ui_.MsgEdit_->setFocus ();
				}
			}

			void ChatTab::scrollToEnd ()
			{
				QWebFrame *frame = Ui_.View_->page ()->mainFrame ();
				int scrollMax = frame->scrollBarMaximum (Qt::Vertical);
				frame->setScrollBarValue (Qt::Vertical, scrollMax);
			}

			void ChatTab::handleHistoryUp ()
			{
				if (CurrentHistoryPosition_ == MsgHistory_.size () - 1)
					return;

				Ui_.MsgEdit_->setText (MsgHistory_
							.at (++CurrentHistoryPosition_));
			}

			void ChatTab::handleHistoryDown ()
			{
				if (CurrentHistoryPosition_ == -1)
					return;

				if (CurrentHistoryPosition_-- == 0)
					Ui_.MsgEdit_->clear ();
				else
					Ui_.MsgEdit_->setText (MsgHistory_
								.at (CurrentHistoryPosition_));
			}

			template<typename T>
			T* ChatTab::GetEntry () const
			{
				T *entry = qobject_cast<T*> (Entry_);
				if (!entry)
					qWarning () << Q_FUNC_INFO
							<< "object"
							<< Entry_
							<< "doesn't implement the required interface";
				return entry;
			}

			void ChatTab::CheckMUC ()
			{
				Plugins::ICLEntry *e = GetEntry<Plugins::ICLEntry> ();

				bool isGoodMUC = true;
				if (!(e->GetEntryFeatures () & Plugins::ICLEntry::FIsMUC))
					isGoodMUC = false;

				if (e->GetEntryFeatures () & Plugins::ICLEntry::FIsMUC &&
						!GetEntry<Plugins::IMUCEntry> ())
				{
					qWarning () << Q_FUNC_INFO
						<< e->GetEntryName ()
						<< "declares itself to be a MUC, "
							"but doesn't implement IMUCEntry";
					isGoodMUC = false;
				}

				if (isGoodMUC)
					HandleMUC ();
			}

			void ChatTab::HandleMUC ()
			{
				// TODO
			}

			void ChatTab::AppendMessage (Plugins::IMessage *msg)
			{
				if (msg->GetDirection () == Plugins::IMessage::DOut &&
						msg->OtherPart ()->GetEntryFeatures () & Plugins::ICLEntry::FIsMUC)
					return;

				QWebFrame *frame = Ui_.View_->page ()->mainFrame ();
				bool shouldScrollFurther = (frame->scrollBarMaximum (Qt::Vertical) ==
								frame->scrollBarValue (Qt::Vertical));

				QString body = FormatBody (msg->GetBody (), msg);

				QString string = QString ("%1 ")
						.arg (FormatDate (msg->GetDateTime (), msg));
				string.append (' ');
				switch (msg->GetDirection ())
				{
				case Plugins::IMessage::DIn:
				{
					QString entryName = Qt::escape (msg->OtherPart ()->GetEntryName ());
					entryName = FormatNickname (entryName, msg);

					if (body.startsWith ("/me "))
					{
						body = body.mid (3);
						string.append ("*** ");
						string.append (entryName);
						string.append (' ');
					}
					else
					{
						string.append (entryName);
						string.append (": ");
					}
					break;
				}
				case Plugins::IMessage::DOut:
					string.append (FormatNickname ("R", msg));
					string.append (": ");
					break;
				}

				string.append (body);

				QWebElement elem = frame->findFirstElement ("body");
				elem.appendInside (QString ("<div>%1</div").arg (string));

				if (shouldScrollFurther)
					QTimer::singleShot (100,
							this,
							SLOT (scrollToEnd ()));
			}

			QString ChatTab::FormatDate (QDateTime dt, Plugins::IMessage *msg)
			{
				Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
				emit hookFormatDateTime (proxy, &dt, msg->GetObject ());
				if (proxy->IsCancelled ())
					return proxy->GetReturnValue ().toString ();

				QString str = dt.time ().toString ();
				return QString ("<span class='datetime' style='color:green'>[" +
						str + "]</span>");
			}

			QString ChatTab::FormatNickname (QString nick, Plugins::IMessage *msg)
			{
				Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
				emit hookFormatNickname (proxy, &nick, msg->GetObject ());
				if (proxy->IsCancelled ())
					return nick;

				QString string;

				QString color = "green";
				if (msg->GetMessageType () == Plugins::IMessage::MTMUCMessage)
				{
					if (NickColors_.isEmpty ())
						GenerateColors ();

					if (!NickColors_.isEmpty ())
					{
						int hash = 0;
						for (int i = 0; i < nick.length (); ++i)
							hash += nick.at (i).unicode ();
						QColor nc = NickColors_.at (hash % NickColors_.size ());
						color = nc.name ();
					}

					QUrl url (QString ("azoth://msgeditreplace/%1")
							.arg (nick + ":"));

					string.append ("<span class='nickname'><a href='");
					string.append (url.toString () + "%20");
					string.append ("' class='nicklink' style='text-decoration:none; color:");
					string.append (color);
					string.append ("'>");
					string.append (nick);
					string.append ("</a></span>");
				}
				else
				{
					switch (msg->GetDirection ())
					{
						case Plugins::IMessage::DIn:
							color = "blue";
							break;
						case Plugins::IMessage::DOut:
							color = "red";
							break;
					}

					string = QString ("<span class='nickname' style='color:%2'>%1</span>")
							.arg (nick)
							.arg (color);
				}

				return string;
			}

			QString ChatTab::FormatBody (QString body, Plugins::IMessage *msg)
			{
				QObject *msgObj = msg->GetObject ();

				Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
				emit hookFormatBodyBegin (proxy, &body, msgObj);
				if (!proxy->IsCancelled ())
				{
					body = Qt::escape (body);
					body.replace ('\n', "<br />");
					body.replace ("  ", "&nbsp; ");

					int pos = 0;
					while ((pos = LinkRegexp_.indexIn (body, pos)) != -1)
					{
						QString link = LinkRegexp_.cap (1);
						QString str = QString ("<a href=\"%1\">%1</a>")
								.arg (link);
						body.replace (pos, link.length (), str);

						pos += str.length ();
					}

					emit hookFormatBodyEnd (proxy, &body, msgObj);
				}

				return proxy->IsCancelled () ?
						proxy->GetReturnValue ().toString () :
						body;
			}

			namespace
			{
				qreal Fix (qreal h)
				{
					while (h < 0)
						h += 1;
					while (h >= 1)
						h -= 1;
					return h;
				}
			}

			void ChatTab::GenerateColors ()
			{
				const qreal lower = 50. / 360.;
				const qreal higher = 160. / 360.;
				const qreal delta = 25. / 360.;

				const qreal alpha = BgColor_.alphaF ();

				qreal s = BgColor_.saturationF ();
				s += (1 - s) / 2;
				qreal v = BgColor_.valueF ();
				v = 0.95 - v / 2;

				qreal h = BgColor_.hueF ();

				QColor color;
				for (qreal d = lower; d <= higher; d += delta)
				{
					color.setHsvF (Fix (h + d), s, v, alpha);
					NickColors_ << color;
					color.setHsvF (Fix (h - d), s, v, alpha);
					NickColors_ << color;
				}
			}
		}
	}
}
