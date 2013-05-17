/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2013  Slava Barinov <rayslava@gmail.com>
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

#include <memory>
#include <QObject>
#include <QDateTime>
#include <QTextLayout>
#include <QListWidget>
#include <QTextDocument>
#include "twitteruser.h"

namespace LeechCraft
{
namespace Woodpecker
{
	class Tweet : public QObject
	{
		Q_OBJECT
		
	private:
		qulonglong	m_id;			/**< Twit id in Twitter */
		QString		m_text;			/**< Text of twit in plaintext */
		TwitterUser_ptr	Author_;	/**< Pointer to twitter author */
		QDateTime	m_created;		/**< Twit date */
		QTextDocument m_document;	/**< QTextDocument which is used for drawing twit */
	public:
		Tweet (QObject *parent = 0);
		Tweet (const QString& text, TwitterUser_ptr author = nullptr, QObject *parent = 0);
		Tweet (const Tweet& original);
		~Tweet ();
		
		/** @brief Set both plain text contents and generates a html representation */
		void setText (const QString& text);
		QString text () const;
		
		qulonglong id () const;
		void setId (qulonglong id);
		
		TwitterUser_ptr author () const;
		void setAuthor (TwitterUser_ptr newAuthor);
		
		QDateTime dateTime () const;
		void setDateTime (const QDateTime& datetime);
		
		/** @brief Direct access to QTextDocument representation
		 * @returns internal document object. You can fix it the way you like for better visuals
		 * 
		 * Used in TwitDelegate class for drawing Tweet object contents in UI
		 */
		QTextDocument* getDocument();
		
		Tweet& operator= (const Tweet&);
		
		/** @brief Comparison is performed by comparing twit id's */
		bool operator== (const Tweet&) const;
		
		/** @brief Comparison is performed by comparing twit id's */
		bool operator!= (const Tweet&) const;
		
		/** @brief Comparison is performed by comparing twit id's */
		bool operator> (const Tweet&) const;
		
		/** @brief Comparison is performed by comparing twit id's */
		bool operator< (const Tweet&) const;
	};
	
	typedef std::shared_ptr<Tweet> Tweet_ptr;
}
}

Q_DECLARE_METATYPE (LeechCraft::Woodpecker::Tweet);
Q_DECLARE_METATYPE (LeechCraft::Woodpecker::Tweet_ptr);

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
