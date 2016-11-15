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

#include "addaccountwizardfirstpage.h"
#include "core.h"

namespace LeechCraft
{
namespace Azoth
{
	AddAccountWizardFirstPage::AddAccountWizardFirstPage (QWidget *parent)
	: QWizardPage (parent)
	{
		Ui_.setupUi (this);

		connect (Ui_.ProtoBox_,
				SIGNAL (currentIndexChanged (int)),
				this,
				SLOT (readdWidgets ()));
		connect (Ui_.RegisterAccount_,
				SIGNAL (toggled (bool)),
				this,
				SLOT (readdWidgets ()));
	}
	
	void AddAccountWizardFirstPage::initializePage ()
	{
		registerField ("AccountName*", Ui_.NameEdit_);
		registerField ("AccountProto", Ui_.ProtoBox_);
		registerField ("RegisterNewAccount", Ui_.RegisterAccount_);

		for (const auto proto : Core::Instance ().GetProtocols ())
		{
			if (proto->GetFeatures () & IProtocol::PFNoAccountRegistration)
				continue;

			Ui_.ProtoBox_->addItem (proto->GetProtocolIcon (),
					proto->GetProtocolName (),
					QVariant::fromValue<QObject*> (proto->GetQObject ()));
		}
			
		connect (wizard (),
				SIGNAL (accepted ()),
				this,
				SLOT (handleAccepted ()));
	}

	void AddAccountWizardFirstPage::CleanupWidgets ()
	{
		const int currentId = wizard ()->currentId ();
		for (const int id : wizard ()->pageIds ())
			if (id > currentId)
				wizard ()->removePage (id);
		qDeleteAll (Widgets_);
	}

	void AddAccountWizardFirstPage::readdWidgets ()
	{
		const int idx = Ui_.ProtoBox_->currentIndex ();
		if (idx == -1)
			return;

		const auto obj = Ui_.ProtoBox_->itemData (idx).value<QObject*> ();
		const auto proto = qobject_cast<IProtocol*> (obj);
		if (!proto)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to cast"
					<< obj
					<< "to IProtocol";
			return;
		}
		
		Ui_.RegisterAccount_->setEnabled (proto->GetFeatures () & IProtocol::PFSupportsInBandRegistration);

		CleanupWidgets ();

		IProtocol::AccountAddOptions options = IProtocol::AAONoOptions;
		if (Ui_.RegisterAccount_->isChecked ())
			options |= IProtocol::AAORegisterNewAccount;
		Widgets_ = proto->GetAccountRegistrationWidgets (options);
		setFinalPage (!Widgets_.isEmpty ());
		if (Widgets_.isEmpty ())
			return;
		
		const auto& protoName = proto->GetProtocolName ();
		for (const auto widget : Widgets_)
		{
			auto page = qobject_cast<QWizardPage*> (widget);
			if (!page)
			{
				page = new QWizardPage (wizard ());
				page->setTitle (tr ("%1 options")
						.arg (protoName));
				page->setLayout (new QVBoxLayout ());
				page->layout ()->addWidget (widget);
			}
			wizard ()->addPage (page);
		}
	}
	
	void AddAccountWizardFirstPage::handleAccepted ()
	{
		const auto obj = Ui_.ProtoBox_->itemData (field ("AccountProto").toInt ()).value<QObject*> ();
		const auto proto = qobject_cast<IProtocol*> (obj);
		if (!proto)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to cast"
					<< obj
					<< "to IProtocol";
			return;
		}
		
		proto->RegisterAccount (Ui_.NameEdit_->text (), Widgets_);
	}
}
}
