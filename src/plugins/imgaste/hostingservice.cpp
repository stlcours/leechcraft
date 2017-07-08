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

#include "hostingservice.h"
#include <QtDebug>
#include <QNetworkReply>
#include <QUrl>
#include <QRegExp>
#include <QStringList>
#include <util/sll/parsejson.h>
#include <util/sll/unreachable.h>
#include "requestbuilder.h"

namespace LeechCraft
{
namespace Imgaste
{
	bool operator< (HostingService s1, HostingService s2)
	{
		return static_cast<int> (s1) < static_cast<int> (s2);
	}

	auto MakeChecker (quint64 sizeLimit, const QSize& dimLimit)
	{
		return [=] (const ImageInfo& info)
		{
			return info.Size_ <= sizeLimit &&
					info.Dim_.width () <= dimLimit.width () &&
					info.Dim_.height () <= dimLimit.height ();
		};
	}

	auto MakeChecker (quint64 sizeLimit)
	{
		return [=] (const ImageInfo& info) { return info.Size_ <= sizeLimit; };
	}

	constexpr quint64 operator"" _mib (quint64 mibs)
	{
		return mibs * 1024 * 1024;
	}

	HostingServiceInfo ToInfo (HostingService s)
	{
		switch (s)
		{
		case HostingService::DumpBitcheeseNet:
			return { "dump.bitcheese.net", MakeChecker (20_mib) };
		case HostingService::ImagebinCa:
			return { "imagebin.ca", MakeChecker (15_mib) };
		case HostingService::SavepicRu:
			return { "savepic.ru", MakeChecker (8_mib, { 5000, 4000 }) };
		case HostingService::PomfCat:
			return { "pomf.cat", MakeChecker (75_mib) };
		case HostingService::MixtapeMoe:
			return { "mixtape.moe", MakeChecker (100_mib) };
		}

		Util::Unreachable ();
	}

	boost::optional<HostingService> FromString (const QString& str)
	{
		for (auto s : GetAllServices ())
			if (ToInfo (s).Name_ == str)
				return s;

		qWarning () << Q_FUNC_INFO
				<< "unknown hosting service"
				<< str;
		return {};
	}

	QList<HostingService> GetAllServices ()
	{
		return
		{
			HostingService::PomfCat,
			HostingService::MixtapeMoe,
			HostingService::DumpBitcheeseNet,
			HostingService::ImagebinCa,
			HostingService::SavepicRu
		};
	}

	namespace
	{
		QNetworkRequest PrefillRequest (const QUrl& url,
				const RequestBuilder& builder)
		{
			QNetworkRequest request { url };
			request.setHeader (QNetworkRequest::ContentTypeHeader,
					QString ("multipart/form-data; boundary=" + builder.GetBoundary ()));
			request.setHeader (QNetworkRequest::ContentLengthHeader,
					QString::number (builder.Build ().size ()));
			return request;
		}

		struct ImagebinWorker final : Worker
		{
			QNetworkReply* Post (const QByteArray& data, const QString& format,
					QNetworkAccessManager *am) const override
			{
				QUrl url { "https://imagebin.ca/upload.php" };

				RequestBuilder builder;
				builder.AddPair ("t", "file");

				builder.AddPair ("title", "");
				builder.AddPair ("description", "");
				builder.AddPair ("tags", "leechcraft");
				builder.AddPair ("category", "general");
				builder.AddPair ("private", "true");
				builder.AddFile (format, "file", data);

				QByteArray formed = builder.Build ();

				auto request = PrefillRequest (url, builder);
				request.setRawHeader ("Origin", "https://imagebin.ca");
				request.setRawHeader ("Referer", "https://imagebin.ca/");
				return am->post (request, formed);
			}

			QString GetLink (const QString& contents, QNetworkReply*) const override
			{
				const auto& lines = contents.split ('\n');
				const auto pos = std::find_if (lines.begin (), lines.end (),
						[] (const QString& line) { return line.startsWith ("url:"); });

				if (pos == lines.end ())
				{
					qWarning () << Q_FUNC_INFO
							<< "no URL:"
							<< contents;
					return {};
				}

				return pos->section (':', 1);
			}
		};

		struct SavepicWorker final : Worker
		{
			QRegExp RegExp_ { ".*<p class=\"img\"><a href=\"/(\\d+).htm\">.*",
					Qt::CaseSensitive, QRegExp::RegExp2 };

			QNetworkReply* Post (const QByteArray& data, const QString& format,
					QNetworkAccessManager *am) const override
			{
				QUrl url ("http://savepic.ru/");

				RequestBuilder builder;
				builder.AddPair ("note", "");
				builder.AddPair ("font1", "decor");
				builder.AddPair ("font2", "20");
				builder.AddPair ("orient", "h");
				builder.AddPair ("size1", "1");
				builder.AddPair ("size2", "1024x768");
				builder.AddPair ("rotate", "00");
				builder.AddPair ("flip", "0");
				builder.AddPair ("mini", "300x225");
				builder.AddPair ("opt3[]", "zoom");
				builder.AddPair ("email", "");
				builder.AddFile (format, "file", data);

				QByteArray formed = builder.Build ();

				return am->post (PrefillRequest (url, builder), formed);
			}

			QString GetLink (const QString& contents, QNetworkReply*) const override
			{
				if (!RegExp_.exactMatch (contents))
					return QString ();

				QString imageId = RegExp_.cap (1);
				return "http://savepic.ru/" + imageId + ".jpg";
			}
		};

		struct BitcheeseWorker final : Worker
		{
			QNetworkReply* Post (const QByteArray& data, const QString& format,
					QNetworkAccessManager *am) const override
			{
				QUrl url ("http://dump.bitcheese.net/upload-image");

				RequestBuilder builder;
				builder.AddFile (format, "file", data);

				const QByteArray& formed = builder.Build () + "\r\n";

				return am->post (PrefillRequest (url, builder), formed);
			}

			QString GetLink (const QString&, QNetworkReply *reply) const override
			{
				QString str = reply->rawHeader ("Location");
				str.chop (8);
				return str;
			}
		};

		struct PomfLikeWorker final : Worker
		{
			const QString Prefix_;
			const QUrl UploadUrl_;

			PomfLikeWorker (const QString& prefix, const QUrl& uploadUrl)
			: Prefix_ { prefix }
			, UploadUrl_ { uploadUrl }
			{
			}

			QNetworkReply* Post (const QByteArray& data, const QString& format,
					QNetworkAccessManager *am) const override
			{
				RequestBuilder builder;
				builder.AddFile (format, "files[]", data);

				return am->post (PrefillRequest (UploadUrl_, builder), builder.Build ());
			}

			QString GetLink (const QString& body, QNetworkReply*) const override
			{
				const auto& json = Util::ParseJson (body.toUtf8 (), Q_FUNC_INFO);
				const auto filename = json.toMap () ["files"]
						.toList ().value (0)
						.toMap () ["url"].toString ();
				return Prefix_ + filename;
			}
		};
	}

	Worker_ptr MakeWorker (HostingService s)
	{
		switch (s)
		{
		case HostingService::DumpBitcheeseNet:
			return std::make_unique<BitcheeseWorker> ();
		case HostingService::ImagebinCa:
			return std::make_unique<ImagebinWorker> ();
		case HostingService::SavepicRu:
			return std::make_unique<SavepicWorker> ();
		case HostingService::PomfCat:
			return std::make_unique<PomfLikeWorker> ("https://a.pomf.cat/",
					QUrl { "https://pomf.cat/upload.php" });
		case HostingService::MixtapeMoe:
			return std::make_unique<PomfLikeWorker> (QString {},
					QUrl { "https://mixtape.moe/upload.php" });
		}

		Util::Unreachable ();
	}
}
}
