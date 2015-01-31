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

#include "wkfontswidget.h"
#include <xmlsettingsdialog/basesettingsmanager.h>
#include <util/sll/qtutil.h>
#include <util/sll/slotclosure.h>
#include <interfaces/iwkfontssettable.h>
#include "ui_wkfontswidget.h"
#include "massfontchangedialog.h"

namespace LeechCraft
{
namespace Util
{
	WkFontsWidget::WkFontsWidget (BaseSettingsManager *bsm, QWidget *parent)
	: QWidget { parent }
	, Ui_ { std::make_shared<Ui::WkFontsWidget> () }
	, BSM_ { bsm }
	{
		Ui_->setupUi (this);

		Family2Chooser_ [QWebSettings::StandardFont] = Ui_->StandardChooser_;
		Family2Chooser_ [QWebSettings::FixedFont] = Ui_->FixedChooser_;
		Family2Chooser_ [QWebSettings::SerifFont] = Ui_->SerifChooser_;
		Family2Chooser_ [QWebSettings::SansSerifFont] = Ui_->SansSerifChooser_;
		Family2Chooser_ [QWebSettings::CursiveFont] = Ui_->CursiveChooser_;
		Family2Chooser_ [QWebSettings::FantasyFont] = Ui_->FantasyChooser_;

		Family2Name_ [QWebSettings::StandardFont] = "StandardFont";
		Family2Name_ [QWebSettings::FixedFont] = "FixedFont";
		Family2Name_ [QWebSettings::SerifFont] = "SerifFont";
		Family2Name_ [QWebSettings::SansSerifFont] = "SansSerifFont";
		Family2Name_ [QWebSettings::CursiveFont] = "CursiveFont";
		Family2Name_ [QWebSettings::FantasyFont] = "FantasyFont";

		ResetFontChoosers ();

		for (const auto& pair : Util::Stlize (Family2Chooser_))
			new Util::SlotClosure<Util::NoDeletePolicy>
			{
				[this, pair] { PendingChanges_ [pair.first] = pair.second->GetFont (); },
				pair.second,
				SIGNAL (fontChanged (QFont)),
				this
			};
	}

	void WkFontsWidget::RegisterSettable (IWkFontsSettable *settable)
	{
		Settables_ << settable;
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[settable, this] { Settables_.removeAll (settable); },
			settable->GetQObject (),
			SIGNAL (destroyed ()),
			settable->GetQObject ()
		};

		for (const auto& pair : Util::Stlize (Family2Chooser_))
			settable->SetFontFamily (pair.first, pair.second->GetFont ());
	}

	void WkFontsWidget::ResetFontChoosers ()
	{
		for (const auto& pair : Util::Stlize (Family2Chooser_))
		{
			const auto& option = Family2Name_ [pair.first];
			pair.second->SetFont (BSM_->property (option).value<QFont> ());
		}
	}

	void WkFontsWidget::on_ChangeAll__released ()
	{
		QHash<QString, QList<QWebSettings::FontFamily>> families;
		for (const auto& pair : Util::Stlize (Family2Chooser_))
			families [pair.second->GetFont ().family ()] << pair.first;

		const auto& stlized = Util::Stlize (families);
		const auto& maxElem = std::max_element (stlized.begin (), stlized.end (),
				[] (auto left, auto right) { return left.second.size () < right.second.size (); });

		const auto dialog = new MassFontChangeDialog { maxElem->first, maxElem->second, this };
		dialog->show ();
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[dialog, this]
			{
				dialog->deleteLater ();
				if (dialog->result () == QDialog::Rejected)
					return;

				const auto& font = dialog->GetFont ();
				for (const auto family : dialog->GetFamilies ())
				{
					PendingChanges_ [family] = font;
					Family2Chooser_ [family]->SetFont (font);
				}
			},
			dialog,
			SIGNAL (finished (int)),
			dialog
		};
	}

	void WkFontsWidget::accept ()
	{
		for (const auto& pair : Util::Stlize (PendingChanges_))
		{
			emit fontChanged (pair.first, pair.second);
			BSM_->setProperty (Family2Name_ [pair.first], pair.second);

			for (const auto settable : Settables_)
				settable->SetFontFamily (pair.first, pair.second);
		}

		PendingChanges_.clear ();
	}

	void WkFontsWidget::reject ()
	{
		ResetFontChoosers ();
		PendingChanges_.clear ();
	}
}
}
