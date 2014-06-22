/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2011  Oleg Linkin
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

#include "separatetabbar.h"
#include <QMouseEvent>
#include <QStyle>
#include <QStylePainter>
#include <QStyleOption>
#include <QApplication>
#include <QtDebug>
#include <interfaces/ihavetabs.h>
#include <interfaces/idndtab.h>
#include "coreproxy.h"
#include "separatetabwidget.h"
#include "core.h"
#include "tabmanager.h"
#include "rootwindowsmanager.h"

namespace LeechCraft
{
	SeparateTabBar::SeparateTabBar (QWidget *parent)
	: QTabBar (parent)
	, Window_ (0)
	, Id_ (0)
	, InMove_ (false)
	, AddTabButton_ (nullptr)
	{
		setObjectName ("org_LeechCraft_MainWindow_CentralTabBar");
		setExpanding (false);
		setIconSize (QSize (15, 15));
		setContextMenuPolicy (Qt::CustomContextMenu);
		setElideMode (Qt::ElideRight);
		setDocumentMode (true);

		setAcceptDrops (true);
		setMovable (true);
		setUsesScrollButtons (true);

		addTab (QString ());
	}

	void SeparateTabBar::SetWindow (MainWindow *win)
	{
		Window_ = win;
	}

	void SeparateTabBar::SetTabData (int index)
	{
		if (index < 0 || index >= count () - 1)
		{
			qWarning () << Q_FUNC_INFO
					<< "invalid index "
					<< index;
			return;
		}

		setTabData (index, ++Id_);
	}

	void SeparateTabBar::SetTabClosable (int index, bool closable, QWidget *closeButton)
	{
		if (index < 0 ||
				index >= count ())
		{
			qWarning () << Q_FUNC_INFO
					<< "invalid index "
					<< index;
			return;
		}

		setTabButton (index, GetCloseButtonPosition (), closable ? closeButton : 0);
	}

	void SeparateTabBar::SetTabWidget (SeparateTabWidget *widget)
	{
		TabWidget_ = widget;
	}

	void SeparateTabBar::SetAddTabButton (QWidget *w)
	{
		AddTabButton_ = w;
		setTabButton (count () - 1, GetAntiCloseButtonPosition (), w);
	}

	QTabBar::ButtonPosition SeparateTabBar::GetCloseButtonPosition () const
	{
		return static_cast<QTabBar::ButtonPosition> (style ()->
				styleHint (QStyle::SH_TabBar_CloseButtonPosition));
	}

	void SeparateTabBar::SetInMove (bool inMove)
	{
		InMove_ = inMove;
	}

	QTabBar::ButtonPosition SeparateTabBar::GetAntiCloseButtonPosition () const
	{
		return GetCloseButtonPosition () == QTabBar::LeftSide ?
				QTabBar::RightSide :
				QTabBar::LeftSide;
	}

	void SeparateTabBar::UpdateComputedWidths () const
	{
		const auto cnt = count ();
		ComputedWidths_.resize (cnt);

		const auto maxTabWidth = width () / 4;

		struct TabInfo
		{
			int Idx_;
			int WidthHint_;
		};
		QVector<TabInfo> infos;
		for (int i = 0; i < cnt - 1; ++i)
			infos.push_back ({ i, std::min (QTabBar::tabSizeHint (i).width (), maxTabWidth) });
		std::sort (infos.begin (), infos.end (),
				[] (const TabInfo& l, const TabInfo& r) { return l.WidthHint_ < r.WidthHint_; });

		const auto btnWidth = QTabBar::tabSizeHint (count () - 1).width ();

		auto remainder = width () - btnWidth;

		while (!infos.isEmpty ())
		{
			auto currentMax = remainder / infos.size ();
			if (infos.front ().WidthHint_ > currentMax)
				break;

			const auto& info = infos.front ();
			remainder -= info.WidthHint_;
			ComputedWidths_ [info.Idx_] = info.WidthHint_;
			infos.pop_front ();
		}

		if (infos.isEmpty ())
			return;

		const auto uniform = remainder / infos.size ();
		for (const auto& info : infos)
			ComputedWidths_ [info.Idx_] = uniform;
	}

	void SeparateTabBar::toggleCloseButtons () const
	{
		for (int i = 0, cnt = count () - 1; i < cnt; ++i)
		{
			const auto button = tabButton (i, GetCloseButtonPosition ());
			if (!button)
				continue;

			const auto visible = button->width () * 2.5 < ComputedWidths_.value (i);
			button->setVisible (visible);
		}
	}

	void SeparateTabBar::tabLayoutChange ()
	{
		ComputedWidths_.clear ();
		QTabBar::tabLayoutChange ();
	}

	QSize SeparateTabBar::tabSizeHint (int index) const
	{
		auto result = QTabBar::tabSizeHint (index);
		if (index == count () - 1)
			return result;

		if (ComputedWidths_.isEmpty ())
		{
			UpdateComputedWidths ();
			toggleCloseButtons ();
		}

		result.setWidth (ComputedWidths_.value (index));
		return result;
	}

	void SeparateTabBar::mouseReleaseEvent (QMouseEvent *event)
	{
		int index = tabAt (event->pos ());
		if (index == count () - 1 &&
				event->button () == Qt::LeftButton)
		{
			emit addDefaultTab ();
			return;
		}

		if (InMove_)
		{
			emit releasedMouseAfterMove (currentIndex ());
			InMove_ = false;
			emit currentChanged (currentIndex ());
		}
		else if (index != -1 &&
				event->button () == Qt::MidButton &&
				index != count () - 1)
		{
			auto rootWM = Core::Instance ().GetRootWindowsManager ();
			auto tm = rootWM->GetTabManager (Window_);
			tm->remove (index);
		}

		QTabBar::mouseReleaseEvent (event);
	}

	void SeparateTabBar::mousePressEvent (QMouseEvent *event)
	{
		if (event->button () == Qt::LeftButton &&
				tabAt (event->pos ()) == count () - 1)
			return;

		setMovable (QApplication::keyboardModifiers () == Qt::NoModifier);

		if (event->button () == Qt::LeftButton)
			DragStartPos_ = event->pos ();
		QTabBar::mousePressEvent (event);
	}

	void SeparateTabBar::mouseMoveEvent (QMouseEvent *event)
	{
		if (isMovable ())
		{
			QTabBar::mouseMoveEvent (event);
			return;
		}

		if (!(event->buttons () & Qt::LeftButton))
			return;

		if ((event->pos () - DragStartPos_).manhattanLength () < QApplication::startDragDistance ())
			return;

		const int dragIdx = tabAt (DragStartPos_);
		auto widget = TabWidget_->Widget (dragIdx);
		auto itw = qobject_cast<ITabWidget*> (widget);
		if (!itw)
		{
			qWarning () << Q_FUNC_INFO
					<< "widget at"
					<< dragIdx
					<< "doesn't implement ITabWidget";
			return;
		}

		auto px = QPixmap::grabWidget (widget);
		px = px.scaledToWidth (px.width () / 2, Qt::SmoothTransformation);

		auto idt = qobject_cast<IDNDTab*> (widget);

		auto data = new QMimeData ();
		if (!idt || QApplication::keyboardModifiers () == Qt::ControlModifier)
		{
			data->setData ("x-leechcraft/tab-drag-action", "reordering");
			data->setData ("x-leechcraft/tab-tabclass", itw->GetTabClassInfo ().TabClass_);
		}
		else if (idt)
			idt->FillMimeData (data);

		auto drag = new QDrag (this);
		drag->setMimeData (data);
		drag->setPixmap (px);
		drag->exec ();
	}

	void SeparateTabBar::dragEnterEvent (QDragEnterEvent *event)
	{
		dragMoveEvent (event);
	}

	void SeparateTabBar::dragMoveEvent (QDragMoveEvent *event)
	{
		const auto tabIdx = tabAt (event->pos ());
		if (tabIdx == count () - 1)
			return;

		auto data = event->mimeData ();
		const auto& formats = data->formats ();
		if (formats.contains ("x-leechcraft/tab-drag-action") &&
				data->data ("x-leechcraft/tab-drag-action") == "reordering")
			event->acceptProposedAction ();
		else if (tabIdx >= 0)
			TabWidget_->setCurrentIndex (tabIdx);

		if (auto idt = qobject_cast<IDNDTab*> (TabWidget_->Widget (tabIdx)))
			idt->HandleDragEnter (event);
	}

	void SeparateTabBar::dropEvent (QDropEvent *event)
	{
		auto data = event->mimeData ();

		const int to = tabAt (event->pos ());
		auto widget = TabWidget_->Widget (to);
		if (data->data ("x-leechcraft/tab-drag-action") == "reordering")
		{
			const int from = tabAt (DragStartPos_);

			if (from == to || to == count () - 1)
				return;

			moveTab (from, to);
			emit releasedMouseAfterMove (to);
			event->acceptProposedAction ();
		}
		else if (auto idt = qobject_cast<IDNDTab*> (widget))
			idt->HandleDrop (event);
	}

	void SeparateTabBar::mouseDoubleClickEvent (QMouseEvent *event)
	{
		QWidget::mouseDoubleClickEvent (event);
		if (tabAt (event->pos ()) == -1)
			emit addDefaultTab ();
	}

	void SeparateTabBar::tabInserted (int index)
	{
		QTabBar::tabInserted (index);
		emit tabWasInserted (index);
	}

	void SeparateTabBar::tabRemoved (int index)
	{
		QTabBar::tabRemoved (index);
		emit tabWasRemoved (index);
	}
}
