/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2012  Oleg Linkin
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

#ifndef LEECHCRAFT_BLOGIQUE_BLOGIQUEWIDGET_H
#define LEECHCRAFT_BLOGIQUE_BLOGIQUEWIDGET_H

#include <QWidget>
#include <interfaces/ihavetabs.h>
#include "ui_blogiquewidget.h"

class QToolBar;

namespace LeechCraft
{
namespace Blogique
{
	class BlogiqueWidget : public QWidget
				,  public ITabWidget
	{
		Q_OBJECT
		Q_INTERFACES (ITabWidget)

		static QObject *S_ParentMultiTabs_;

		Ui::BlogiqueWidget Ui_;
	public:
		BlogiqueWidget (QWidget *parent = 0);

		QObject* ParentMultiTabs ();
		TabClassInfo GetTabClassInfo () const;
		QToolBar* GetToolBar () const;
		void Remove ();

		static void SetParentMultiTabs (QObject *tab);
	signals:
		void removeTab (QWidget *tab);
	};
}
}

#endif // LEECHCRAFT_BLOGIQUE_BLOGIQUEWIDGET_H
