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

#include "avatarsstorage.h"
#include <QBuffer>
#include <QtDebug>
#include <util/threads/futures.h>
#include "avatarsstoragethread.h"
#include "interfaces/azoth/iclentry.h"

namespace LeechCraft
{
namespace Azoth
{
	AvatarsStorage::AvatarsStorage (QObject *parent)
	: QObject { parent }
	, StorageThread_ { new AvatarsStorageThread { this } }
	{
	}

	QFuture<void> AvatarsStorage::SetAvatar (const ICLEntry *entry,
			IHaveAvatars::Size size, const QImage& image)
	{
		QByteArray data;
		QBuffer buffer { &data };
		image.save (&buffer, "PNG", 0);

		return SetAvatar (entry->GetEntryID (), size, data);
	}

	QFuture<void> AvatarsStorage::SetAvatar (const QString& entryId,
			IHaveAvatars::Size size, const QByteArray& data)
	{
		return StorageThread_->SetAvatar (entryId, size, data);
	}

	QFuture<MaybeImage> AvatarsStorage::GetAvatar (const ICLEntry *entry, IHaveAvatars::Size size)
	{
		const auto& entryId = entry->GetEntryID ();
		const auto& hrId = entry->GetHumanReadableID ();

		return Util::Sequence (this, [=] { return GetAvatar (entryId, size); }) >>
				[=] (const MaybeByteArray& data)
				{
					if (!data)
						return Util::MakeReadyFuture<MaybeImage> ({});

					return QtConcurrent::run ([=] () -> MaybeImage
							{
								QImage image;
								if (!image.loadFromData (*data))
								{
									qWarning () << Q_FUNC_INFO
											<< "unable to load image from data for"
											<< entryId
											<< hrId;
									return {};
								}

								return image;
							});
				};
	}

	QFuture<MaybeByteArray> AvatarsStorage::GetAvatar (const QString& entryId, IHaveAvatars::Size size)
	{
		return StorageThread_->GetAvatar (entryId, size);
	}
}
}