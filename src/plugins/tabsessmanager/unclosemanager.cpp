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

#include "unclosemanager.h"
#include <functional>
#include <QMenu>
#include <QtDebug>
#include <util/sll/slotclosure.h>
#include <interfaces/ihavetabs.h>
#include <interfaces/core/irootwindowsmanager.h>
#include <interfaces/core/icoretabwidget.h>
#include "tabspropsmanager.h"
#include "util.h"

namespace LeechCraft
{
namespace TabSessManager
{
	UncloseManager::UncloseManager (const ICoreProxy_ptr& proxy, TabsPropsManager *tpm, QObject *parent)
	: QObject { parent }
	, Proxy_ { proxy }
	, TabsPropsMgr_ { tpm }
	, UncloseMenu_ { new QMenu { tr ("Unclose tabs") } }
	{
	}

	QAction* UncloseManager::GetMenuAction () const
	{
		return UncloseMenu_->menuAction ();
	}

	void UncloseManager::HandleRemoveTab (QWidget *widget)
	{
		const auto tab = qobject_cast<ITabWidget*> (widget);
		if (!tab)
			return;

		if (const auto recTab = qobject_cast<IRecoverableTab*> (widget))
			HandleRemoveRecoverableTab (widget, recTab);
		else if (IsGoodSingleTC (tab->GetTabClassInfo ()))
			HandleRemoveSingleTab (widget, tab);
	}

	struct UncloseManager::RemoveTabParams
	{
		QByteArray RecoverData_;
		QString TabName_;
		QIcon TabIcon_;
		QWidget *Widget_;

		std::function<void (QObject*, TabRecoverInfo)> Uncloser_;
	};

	void UncloseManager::GenericRemoveTab (const RemoveTabParams& params)
	{
		TabRecoverInfo info
		{
			params.RecoverData_,
			GetSessionProps (params.Widget_)
		};

		const auto tab = qobject_cast<ITabWidget*> (params.Widget_);

		const auto rootWM = Proxy_->GetRootWindowsManager ();
		const auto winIdx = rootWM->GetWindowForTab (tab);
		const auto tabIdx = rootWM->GetTabWidget (winIdx)->IndexOf (params.Widget_);
		info.DynProperties_.append ({ "TabSessManager/Position", tabIdx });

		for (const auto& action : UncloseMenu_->actions ())
			if (action->property ("RecData") == params.RecoverData_)
			{
				UncloseMenu_->removeAction (action);
				action->deleteLater ();
				break;
			}

		const auto& fm = UncloseMenu_->fontMetrics ();
		const auto& elided = fm.elidedText (params.TabName_, Qt::ElideMiddle, 300);
		const auto action = new QAction { params.TabIcon_, elided, this };
		action->setProperty ("RecData", params.RecoverData_);

		const auto plugin = tab->ParentMultiTabs ();
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[uncloser = params.Uncloser_, info, plugin, action, winIdx, this]
			{
				action->deleteLater ();

				if (UncloseMenu_->defaultAction () == action)
					if (const auto nextAct = UncloseMenu_->actions ().value (1))
					{
						UncloseMenu_->setDefaultAction (nextAct);
						nextAct->setShortcut (QString ("Ctrl+Shift+T"));
					}
				UncloseMenu_->removeAction (action);

				const auto propsGuard = TabsPropsMgr_->AppendProps (info.DynProperties_);
				const auto winGuard = TabsPropsMgr_->AppendWindow (winIdx);
				uncloser (plugin, info);
			},
			action,
			SIGNAL (triggered ()),
			action
		};

		if (UncloseMenu_->defaultAction ())
			UncloseMenu_->defaultAction ()->setShortcut (QKeySequence ());
		UncloseMenu_->insertAction (UncloseMenu_->actions ().value (0), action);
		UncloseMenu_->setDefaultAction (action);
		action->setShortcut (QString ("Ctrl+Shift+T"));
	}

	void UncloseManager::HandleRemoveRecoverableTab (QWidget *widget, IRecoverableTab *recTab)
	{
		const auto& recoverData = recTab->GetTabRecoverData ();
		if (recoverData.isEmpty ())
			return;

		GenericRemoveTab ({
				recoverData,
				recTab->GetTabRecoverName (),
				recTab->GetTabRecoverIcon (),
				widget,
				[] (QObject *plugin, const TabRecoverInfo& info)
				{
					qobject_cast<IHaveRecoverableTabs*> (plugin)->RecoverTabs ({ info });
				}
			});
	}

	void UncloseManager::HandleRemoveSingleTab (QWidget *widget, ITabWidget *tab)
	{
		const auto& tc = tab->GetTabClassInfo ();
		GenericRemoveTab ({
				tc.TabClass_,
				tc.VisibleName_,
				tc.Icon_,
				widget,
				[] (QObject *plugin, const TabRecoverInfo& info)
				{
					qobject_cast<IHaveTabs*> (plugin)->TabOpenRequested (info.Data_);
				}
			});
	}
}
}
