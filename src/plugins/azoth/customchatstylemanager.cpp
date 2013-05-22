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

#include "customchatstylemanager.h"
#include <QSettings>
#include <QCoreApplication>
#include "interfaces/azoth/iaccount.h"

namespace LeechCraft
{
namespace Azoth
{
	CustomChatStyleManager::CustomChatStyleManager (QObject *parent)
	: QObject (parent)
	{
	}

	QPair<QString, QString> CustomChatStyleManager::GetForEntry (ICLEntry *entry) const
	{
		if (!entry)
			return {};

		auto acc = qobject_cast<IAccount*> (entry->GetParentAccount ());
		return entry->GetEntryType () == ICLEntry::ETMUC ?
				GetMUCStyleForAccount (acc) :
				GetStyleForAccount (acc);
	}

	QPair<QString, QString> CustomChatStyleManager::GetStyleForAccount (IAccount *acc) const
	{
		return GetProps ("Chat", acc);
	}

	QPair<QString, QString> CustomChatStyleManager::GetMUCStyleForAccount (IAccount *acc) const
	{
		return GetProps ("MUC", acc);
	}

	void CustomChatStyleManager::Set (IAccount *acc, Settable settable, const QString& value)
	{
		QSettings settings (QCoreApplication::organizationName (),
					QCoreApplication::applicationName () + "_Azoth");
		settings.beginGroup ("CustomStyles");
		settings.beginGroup (acc->GetAccountID ());

		QString name;
		switch (settable)
		{
		case Settable::ChatStyle:
			name = "ChatStyle";
			break;
		case Settable::ChatVariant:
			name = "ChatVariant";
			break;
		case Settable::MUCStyle:
			name = "MUCStyle";
			break;
		case Settable::MUCVariant:
			name = "MUCVariant";
			break;
		}

		settings.setValue (name, value);

		settings.endGroup ();
		settings.endGroup ();

		emit accountStyleChanged (acc);
	}

	QPair<QString, QString> CustomChatStyleManager::GetProps (const QString& prefix, IAccount *acc) const
	{
		QSettings settings (QCoreApplication::organizationName (),
					QCoreApplication::applicationName () + "_Azoth");
		settings.beginGroup ("CustomStyles");
		settings.beginGroup (acc->GetAccountID ());
		const auto& style = settings.value (prefix + "Style").toString ();
		const auto& variant = settings.value (prefix + "Variant").toString ();
		settings.endGroup ();
		settings.endGroup ();

		return { style, variant };
	}
}
}
