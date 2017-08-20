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

#include <QtGlobal>
#include <QQuickImageProvider>
#include "qmlconfig.h"

class QIcon;

namespace LeechCraft
{
namespace Util
{
	/** @brief Provides scalable icons to QML.
	 *
	 * This class is used to provide pixmaps generated from QIcons to
	 * QML. It supports generating different pixmaps for different source
	 * sizes. For that, the last component of the URL in the
	 * <code>source</code> property of a QML <code>Image</code> should
	 * correspond to the current width, like this:
	 * \code
	 * Image {
	 *     source: someIconString + '/' + width
	 *     // ...
	 * }
	 * \endcode
	 *
	 * The subclasses should implement the GetIcon() pure virtual
	 * function, which should return a QIcon for a given path, where path
	 * is a QStringList obtained by breaking the URL path at '/' and
	 * \em leaving the width component.
	 *
	 * Please also see the documentation for <code>QIcon::pixmap()</code>
	 * regarding upscaling.
	 *
	 * @sa ThemeImageProvider
	 *
	 * @ingroup QmlUtil
	 */
	class UTIL_QML_API WidthIconProvider : public QQuickImageProvider
	{
	public:
		WidthIconProvider ();

		/** @brief Reimplemented from QDeclarativeImageProvider::requestPixmap().
		 *
		 * @param[in] id The image ID.
		 * @param[in] size If non-null, will be set to the real size of
		 * the generated image.
		 * @param[in] requestedSize The requested image size.
		 */
		QPixmap requestPixmap (const QString& id, QSize *size, const QSize& requestedSize);

		/** @brief Implement this method to return a proper QIcon for path.
		 *
		 * See this class documentation for more information.
		 *
		 * @param[in] path The icon path, a list obtained by breaking the
		 * URL request path at '/'.
		 * @return QIcon for the path, or an empty QIcon.
		 */
		virtual QIcon GetIcon (const QStringList& path) = 0;
	};
}
}
