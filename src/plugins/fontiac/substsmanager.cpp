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

#include "substsmanager.h"
#include <QStandardItemModel>
#include <QSettings>
#include <QtDebug>
#include <QCoreApplication>
#include <xmlsettingsdialog/datasourceroles.h>
#include <util/sll/prelude.h>

namespace LeechCraft
{
namespace Fontiac
{
	SubstsManager::SubstsManager (QObject* parent)
	: QObject { parent }
	, Model_ { new QStandardItemModel { this } }
	{
		InitHeader ();
		LoadSettings ();
	}

	QAbstractItemModel* SubstsManager::GetModel () const
	{
		return Model_;
	}

	void SubstsManager::InitHeader ()
	{
		Model_->setHorizontalHeaderLabels ({ tr ("Font family"), tr ("Substitution") });
		Model_->horizontalHeaderItem (0)->setData (DataSources::DataFieldType::String,
				DataSources::DataSourceRole::FieldType);
		Model_->horizontalHeaderItem (1)->setData (DataSources::DataFieldType::Font,
				DataSources::DataSourceRole::FieldType);
	}

	void SubstsManager::LoadSettings ()
	{
		QSettings settings
		{
			QCoreApplication::organizationName (),
			QCoreApplication::applicationName () + "_Fontiac"
		};

		int size = settings.beginReadArray ("Substs");
		for (int i = 0; i < size; ++i)
		{
			settings.setArrayIndex (i);

			const auto& family = settings.value ("Family").toString ();
			const auto& subst = settings.value ("Subst").toString ();

			AddItem (family, subst, { subst });
		}
		settings.endArray ();
	}

	void SubstsManager::SaveSettings () const
	{
		QSettings settings
		{
			QCoreApplication::organizationName (),
			QCoreApplication::applicationName () + "_Fontiac"
		};

		settings.beginWriteArray ("Substs");
		for (int i = 0; i < Substitutes_.size (); ++i)
		{
			settings.setArrayIndex (i);

			const auto& pair = Substitutes_.at (i);
			settings.setValue ("Family", pair.first);
			settings.setValue ("Subst", pair.second);
		}
		settings.endArray ();
	}

	void SubstsManager::AddItem (const QString& family, const QString& subst, const QFont& font)
	{
		if (family.isEmpty () || subst.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "empty data";
			return;
		}

		if (Substitutes_.contains ({ family, subst }))
			return;

		Substitutes_.append ({ family, subst });
		QFont::insertSubstitution (family, subst);

		QList<QStandardItem*> row
		{
			new QStandardItem { family },
			new QStandardItem { subst }
		};
		for (const auto item : row)
			item->setEditable (false);
		row.value (0)->setFont ({ family });
		row.value (1)->setFont (font);
		Model_->appendRow (row);
	}

	void SubstsManager::RebuildSubsts (const QString& family)
	{
		QFont::removeSubstitutions (family);

		const auto& remaining = Util::Filter (Substitutes_,
				[&family] (const auto& pair) { return pair.first == family; });
		if (!remaining.isEmpty ())
			QFont::insertSubstitutions (family, Util::Map (remaining, Util::Snd));
	}

	namespace
	{
		struct DatasParseResult
		{
			QString Family_;
			QFont Font_;
			QString Subst_;

			DatasParseResult (const QVariantList& datas)
			: Family_ { datas.value (0).toString ().trimmed () }
			, Font_ { datas.value (1).value<QFont> () }
			, Subst_ { Font_.family ().trimmed ()}
			{
			}
		};
	}

	void SubstsManager::addRequested (const QString&, const QVariantList& datas)
	{
		const DatasParseResult parsed { datas };
		AddItem (parsed.Family_, parsed.Subst_, parsed.Font_);

		SaveSettings ();
	}

	void SubstsManager::modifyRequested (const QString&, int row, const QVariantList& datas)
	{
		const DatasParseResult parsed { datas };
		Model_->item (row, 0)->setText (parsed.Family_);
		Model_->item (row, 0)->setFont ({ parsed.Family_ });
		Model_->item (row, 1)->setText (parsed.Subst_);
		Model_->item (row, 1)->setFont ({ parsed.Font_ });

		const auto& oldFamily = Substitutes_.at (row).first;
		Substitutes_ [row].first = parsed.Family_;
		Substitutes_ [row].second = parsed.Subst_;

		RebuildSubsts (parsed.Family_);
		if (oldFamily != parsed.Family_)
			RebuildSubsts (oldFamily);

		SaveSettings ();
	}

	void SubstsManager::removeRequested (const QString&, const QModelIndexList& rows)
	{
		for (const auto& idx : rows)
		{
			const auto row = idx.row ();
			if (row < 0 || row >= Substitutes_.size ())
				continue;

			Model_->removeRow (row);

			const auto& family = Substitutes_.at (row).first;
			Substitutes_.removeAt (row);

			RebuildSubsts (family);
		}

		SaveSettings ();
	}
}
}
