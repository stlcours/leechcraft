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

#pragma once

#include <QPoint>
#include "guiconfig.h"

class QSize;
class QRect;
class QPixmap;
class QLabel;

namespace LeechCraft
{
namespace Util
{
	/** Additional fitting options used by FitRect() and FitRectScreen().
	 *
	 * @ingroup GuiUtil
	 */
	enum FitFlag
	{
		/** Default fitting options.
		 */
		NoFlags,

		/** The fitted rectangle should never overlap with the original
		 * top left position.
		 *
		 * This option may be useful if a menu or tooltip is fitted,
		 * since it's generally undesirable to cover the corresponding
		 * UI element with the menu or tooltip.
		 */
		NoOverlap = 0x01
	};

	Q_DECLARE_FLAGS (FitFlags, FitFlag);

	/** @brief Tries to fit a rectangle (like a dialog or popup) into screen.
	 *
	 * This function tries to move the rectangle with top left point at
	 * pos and with given size so that it fits completely into the
	 * available geometry of the screen that contains the point pos. It
	 * leaves the rectangle size intact, instead returning the new top
	 * left position.
	 *
	 * Calling this function is equivalent to calling FitRect() with
	 * the geometry parameter set to
	 * <code>QDesktopWidget::availableGeometry(pos)</code>. See the
	 * documentation for FitRect() for more details.
	 *
	 * @param[in] pos The original top left position of the rect to fit.
	 * @param[in] size The size of the rectangle to fit.
	 * @param[in] flags Additional fitting parameters.
	 * @param[in] shiftAdd Additional components to be added if the
	 * rectangle is actually moved in the corresponding directions.
	 * @return The new top left position of the rectangle.
	 *
	 * @sa FitRect()
	 *
	 * @ingroup GuiUtil
	 */
	UTIL_GUI_API QPoint FitRectScreen (QPoint pos, const QSize& size,
			FitFlags flags = NoFlags, const QPoint& shiftAdd = QPoint (0, 0));

	/** @brief Tries to fit a rectangle (like a dialog or popup) into geometry.
	 *
	 * This function tries to move the rectangle with top left point at
	 * pos and with given size so that it fits completely into the
	 * rectangle given by the geometry parameter. It leaves the
	 * size intact, instead returning the new top left position.
	 *
	 * If the rectangle is actually moved by this function, the shiftAdd
	 * parameter is used to customize how it is moved: the
	 * <code>shiftAdd.x()</code> component is added to the result iff
	 * <code>pos.x()</code> is changed, and <code>shiftAdd.y()</code> is
	 * added to the result iff <code>pos.y()</code> is changed.
	 *
	 * @param[in] pos The original top left position of the rect to fit.
	 * @param[in] size The size of the rectangle to fit.
	 * @param[in] geometry The rectangle \em into which the source
	 * rectangle should be fitted.
	 * @param[in] flags Additional fitting parameters.
	 * @param[in] shiftAdd Additional components to be added if the
	 * rectangle is actually moved in the corresponding directions.
	 * @return The new top left position of the rectangle.
	 *
	 * @sa FitRectScreen()
	 *
	 * @ingroup GuiUtil
	 */
	UTIL_GUI_API QPoint FitRect (QPoint pos, const QSize& size, const QRect& geometry,
			FitFlags flags = NoFlags, const QPoint& shiftAdd = QPoint (0, 0));

	/** @brief Shows a pixmap at the given pos.
	 *
	 * This function shows a dialog with the given pixmap at the given
	 * position. If the pixmap is too big, it is scaled down. A QLabel
	 * created with window decorations is used as the dialog. The created
	 * label is returned from the function, so one could also set the
	 * window title and further customize the label.
	 *
	 * This function is useful to display full version of album art in a
	 * media player or a user avatar in an IM application.
	 *
	 * @param[in] pixmap The pixmap to show.
	 * @param[in] pos The position where the dialog should be shown.
	 * @return The created dialog.
	 *
	 * @ingroup GuiUtil
	 */
	UTIL_GUI_API QLabel* ShowPixmapLabel (const QPixmap& pixmap, const QPoint& pos = QPoint ());
}
}

Q_DECLARE_OPERATORS_FOR_FLAGS (LeechCraft::Util::FitFlags);
