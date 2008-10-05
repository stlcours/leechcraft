#include <QtDebug>
#include <QImage>
#include <QSettings>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QTemporaryFile>
#include <QTimer>
#include <QNetworkReply>
#include <stdexcept>
#include <boost/bind.hpp>
#include <plugininterface/proxy.h>
#include <plugininterface/tagscompletionmodel.h>
#include "core.h"
#include "regexpmatchermanager.h"
#include "xmlsettingsmanager.h"
#include "parserfactory.h"
#include "rss20parser.h"
#include "atom10parser.h"
#include "atom03parser.h"
#include "channelsmodel.h"
#include "itembucket.h"
#include "opmlparser.h"
#include "sqlstoragebackend.h"

Core::Core ()
: SaveScheduled_ (false)
, StorageBackend_ (new SQLStorageBackend ())
{
	ParserFactory::Instance ().Register (&RSS20Parser::Instance ());
	ParserFactory::Instance ().Register (&Atom10Parser::Instance ());
	ParserFactory::Instance ().Register (&Atom03Parser::Instance ());
	ItemHeaders_ << tr ("Name") << tr ("Date");

	qRegisterMetaTypeStreamOperators<Feed> ("Feed");
	qRegisterMetaTypeStreamOperators<Item> ("Item");

	ChannelsModel_ = new ChannelsModel (this);
	connect (ChannelsModel_, SIGNAL (channelDataUpdated ()), this, SIGNAL (channelDataUpdated ()));

	TagsCompletionModel_ = new TagsCompletionModel (this);

	feeds_container_t feeds;
	StorageBackend_->GetFeeds (feeds);
	for (feeds_container_t::const_iterator i = feeds.begin (),
			end = feeds.end (); i < end; ++i)
	{
		Feeds_ [(*i)->URL_] = *i;
		ChannelsModel_->AddFeed (*i);
	}

	QSettings settings (Proxy::Instance ()->GetOrganizationName (), Proxy::Instance ()->GetApplicationName () + "_Aggregator");
	TagsCompletionModel_->UpdateTags (settings.value ("GlobalTags",
			QStringList ("untagged")).toStringList ());

	ActivatedChannel_ = 0;
}

Core& Core::Instance ()
{
	static Core core;
	return core;
}

void Core::Release ()
{
	saveSettings ();
	ItemBucket::Instance ().Release ();
	XmlSettingsManager::Instance ()->Release ();
}

void Core::DoDelayedInit ()
{
	UpdateTimer_ = new QTimer (this);
	UpdateTimer_->start (XmlSettingsManager::Instance ()->property ("UpdateInterval").toInt () * 60 * 1000);
	connect (UpdateTimer_, SIGNAL (timeout ()), this, SLOT (updateFeeds ()));
	if (XmlSettingsManager::Instance ()->property ("UpdateOnStartup").toBool ())
		QTimer::singleShot (2000, this, SLOT (updateFeeds ()));

	QTimer *saveTimer = new QTimer (this);
	saveTimer->start (60 * 1000);
	connect (saveTimer, SIGNAL (timeout ()), this, SLOT (scheduleSave ()));

	XmlSettingsManager::Instance ()->RegisterObject ("UpdateInterval", this, "updateIntervalChanged");
	XmlSettingsManager::Instance ()->RegisterObject ("ShowIconInTray", this, "showIconInTrayChanged");
	UpdateUnreadItemsNumber ();
}

void Core::SetProvider (QObject *provider, const QString& feature)
{
	Providers_ [feature] = provider;
	if (feature == "http")
	{
		connect (provider,
				SIGNAL (jobFinished (int)),
				this,
				SLOT (handleJobFinished (int)));
		connect (provider,
				SIGNAL (jobRemoved (int)),
				this,
				SLOT (handleJobRemoved (int)));
		connect (provider,
				SIGNAL (jobError (int, IDirectDownload::Error)),
				this,
				SLOT (handleJobError (int, IDirectDownload::Error)));
	}
}

void Core::AddFeed (const QString& url, const QStringList& tags)
{
	if (Feeds_.contains (url))
	{
		emit error (tr ("This feed is already added"));
		return;
	}

	QObject *provider = Providers_ ["http"];
	IDownload *iid = qobject_cast<IDownload*> (provider);
	IDirectDownload *idd = qobject_cast<IDirectDownload*> (provider);
	if (!provider || !idd || !iid)
	{
		emit error (tr ("Strange, but no suitable provider found"));
		return;
	}
	if (!iid->CouldDownload (url, LeechCraft::Autostart))
	{
		emit error (tr ("Could not handle URL %1").arg (url));
		return;
	}

	QString name;
	{
		QTemporaryFile file;
		file.open ();
		name = file.fileName ();
		file.close ();
		file.remove ();
	}
	DirectDownloadParams params = { url, name };
	PendingJob pj = { PendingJob::RFeedAdded, url, name, tags };
	int id = idd->AddJob (params, LeechCraft::Internal | LeechCraft::Autostart |
			LeechCraft::DoNotNotifyUser | LeechCraft::DoNotSaveInHistory);
	PendingJobs_ [id] = pj;
}

void Core::RemoveFeed (const QModelIndex& index)
{
	if (!index.isValid ())
		return;
	Channel_ptr channel = ChannelsModel_->GetChannelForIndex (index);

	QString feedURL = FindFeedForChannel (channel);
	if (feedURL.isEmpty ())
	{
		qWarning () << Q_FUNC_INFO << "could not find feed for channel" ;
		return;
	}

	for (size_t i = 0, size = Feeds_ [feedURL]->Channels_.size (); i < size; ++i)
		ChannelsModel_->RemoveChannel (Feeds_ [feedURL]->Channels_ [i]);
	StorageBackend_->RemoveFeed (Feeds_ [feedURL]);
	Feeds_.remove (feedURL);

	if (channel.get () == ActivatedChannel_)
	{
		ActivatedChannel_ = 0;
		reset ();
	}

	UpdateUnreadItemsNumber ();
	scheduleSave ();
}

void Core::Activated (const QModelIndex& index)
{
	if (!ActivatedChannel_ || static_cast<int> (ActivatedChannel_->Items_.size ()) <= index.row ())
		return;

	Item_ptr item = ActivatedChannel_->Items_ [index.row ()];

	QString URL = item->Link_;
	item->Unread_ = false;
	StorageBackend_->UpdateItem (item, ActivatedChannel_->Link_ + ActivatedChannel_->Title_);
	ChannelsModel_->UpdateChannelData (ActivatedChannel_);
	UpdateUnreadItemsNumber ();
	QDesktopServices::openUrl (QUrl (URL));
}

void Core::FeedActivated (const QModelIndex& index)
{
	QDesktopServices::openUrl (QUrl (ChannelsModel_->GetChannelForIndex (index)->Link_));
}

QString Core::GetDescription (const QModelIndex& index)
{
	if (!ActivatedChannel_ || static_cast<int> (ActivatedChannel_->Items_.size ()) <= index.row ())
		return QString ();

	Item_ptr item = ActivatedChannel_->Items_ [index.row ()];

	item->Unread_ = false;
	StorageBackend_->UpdateItem (item, ActivatedChannel_->Link_ + ActivatedChannel_->Title_);
	ChannelsModel_->UpdateChannelData (ActivatedChannel_);
	UpdateUnreadItemsNumber ();
	return item->Description_;
}

QString Core::GetAuthor (const QModelIndex& index)
{
	if (!ActivatedChannel_ || static_cast<int> (ActivatedChannel_->Items_.size ()) <= index.row ())
		return QString ();

	Item_ptr item = ActivatedChannel_->Items_ [index.row ()];

	item->Unread_ = false;
	StorageBackend_->UpdateItem (item, ActivatedChannel_->Link_ + ActivatedChannel_->Title_);
	ChannelsModel_->UpdateChannelData (ActivatedChannel_);
	UpdateUnreadItemsNumber ();
	return item->Author_;
}

QString Core::GetCategory (const QModelIndex& index)
{
	if (!ActivatedChannel_ || static_cast<int> (ActivatedChannel_->Items_.size ()) <= index.row ())
		return QString ();

	Item_ptr item = ActivatedChannel_->Items_ [index.row ()];

	item->Unread_ = false;
	StorageBackend_->UpdateItem (item, ActivatedChannel_->Link_ + ActivatedChannel_->Title_);
	ChannelsModel_->UpdateChannelData (ActivatedChannel_);
	UpdateUnreadItemsNumber ();
	return item->Category_;
}

QString Core::GetLink (const QModelIndex& index)
{
	if (!ActivatedChannel_ || static_cast<int> (ActivatedChannel_->Items_.size ()) <= index.row ())
		return QString ();

	Item_ptr item = ActivatedChannel_->Items_ [index.row ()];

	item->Unread_ = false;
	StorageBackend_->UpdateItem (item, ActivatedChannel_->Link_ + ActivatedChannel_->Title_);
	ChannelsModel_->UpdateChannelData (ActivatedChannel_);
	UpdateUnreadItemsNumber ();
	return item->Link_;
}

QDateTime Core::GetPubDate (const QModelIndex& index)
{
	if (!ActivatedChannel_ || static_cast<int> (ActivatedChannel_->Items_.size ()) <= index.row ())
		return QDateTime ();

	Item_ptr item = ActivatedChannel_->Items_ [index.row ()];

	item->Unread_ = false;
	StorageBackend_->UpdateItem (item, ActivatedChannel_->Link_ + ActivatedChannel_->Title_);
	ChannelsModel_->UpdateChannelData (ActivatedChannel_);
	UpdateUnreadItemsNumber ();
	return item->PubDate_;
}

QAbstractItemModel* Core::GetChannelsModel ()
{
	return ChannelsModel_;
}

TagsCompletionModel* Core::GetTagsCompletionModel ()
{
	return TagsCompletionModel_;
}

void Core::UpdateTags (const QStringList& tags)
{
	TagsCompletionModel_->UpdateTags (tags);
}

void Core::MarkItemAsUnread (const QModelIndex& i)
{
	if (!ActivatedChannel_ || !i.isValid ())
		return;

	ActivatedChannel_->Items_ [i.row ()]->Unread_ = true;
	StorageBackend_->UpdateItem (ActivatedChannel_->Items_ [i.row ()],
			ActivatedChannel_->Link_ + ActivatedChannel_->Title_);
	ChannelsModel_->UpdateChannelData (ActivatedChannel_);
	emit dataChanged (index (i.row (), 0), index (i.row (), 1));
	UpdateUnreadItemsNumber ();
}

bool Core::IsItemRead (int item) const
{
	if (!ActivatedChannel_ ||
			static_cast<int> (ActivatedChannel_->Items_.size ()) <= item)
		return true;
	else
		return !ActivatedChannel_->Items_ [item]->Unread_;
}

void Core::MarkChannelAsRead (const QModelIndex& i)
{
	if (!ActivatedChannel_ || !i.isValid ())
		return;

	ChannelsModel_->MarkChannelAsRead (i);
	if (ChannelsModel_->GetChannelForIndex (i).get () == ActivatedChannel_)
		emit dataChanged (index (0, 0), index (ActivatedChannel_->Items_.size () - 1, 1));

	QString hash = ChannelsModel_->GetChannelForIndex (i)->Link_ +
		ChannelsModel_->GetChannelForIndex (i)->Title_;
	for (items_container_t::const_iterator item =
			ChannelsModel_->GetChannelForIndex (i)->Items_.begin (),
			end = ChannelsModel_->GetChannelForIndex (i)->Items_.end ();
			item != end; ++item)
		StorageBackend_->UpdateItem (*item, hash);

	UpdateUnreadItemsNumber ();
}

void Core::MarkChannelAsUnread (const QModelIndex& i)
{
	if (!ActivatedChannel_ || !i.isValid ())
		return;

	ChannelsModel_->MarkChannelAsUnread (i);
	if (ChannelsModel_->GetChannelForIndex (i).get () == ActivatedChannel_)
		emit dataChanged (index (0, 0), index (ActivatedChannel_->Items_.size () - 1, 1));

	QString hash = ChannelsModel_->GetChannelForIndex (i)->Link_ +
		ChannelsModel_->GetChannelForIndex (i)->Title_;
	for (items_container_t::const_iterator item =
			ChannelsModel_->GetChannelForIndex (i)->Items_.begin (),
			end = ChannelsModel_->GetChannelForIndex (i)->Items_.end ();
			item != end; ++item)
		StorageBackend_->UpdateItem (*item, hash);

	UpdateUnreadItemsNumber ();
}

QStringList Core::GetTagsForIndex (int i) const
{
	Channel_ptr channel = ChannelsModel_->GetChannelForIndex (ChannelsModel_->index (i, 0));
	if (channel)
		return channel->Tags_;
	else
		return QStringList ();
}

Core::ChannelInfo Core::GetChannelInfo (const QModelIndex& i) const
{
	Channel_ptr channel = ChannelsModel_->GetChannelForIndex (i);
	ChannelInfo ci;
	if (channel)
	{
		ci.Link_ = channel->Link_;
		ci.Description_ = channel->Description_;
		ci.Author_ = channel->Author_;
	}
	return ci;
}

QPixmap Core::GetChannelPixmap (const QModelIndex& i) const
{
	Channel_ptr channel = ChannelsModel_->GetChannelForIndex (i);
	if (channel)
		return channel->Pixmap_;
	else
		return QPixmap ();
}

void Core::SetTagsForIndex (const QString& tags, const QModelIndex& index)
{
	Channel_ptr channel = ChannelsModel_->GetChannelForIndex (index);
	channel->Tags_ = tags.split (' ');
	StorageBackend_->UpdateChannel (channel, FindFeedForChannel (channel));
}

void Core::UpdateFeed (const QModelIndex& index)
{
	Channel_ptr channel = ChannelsModel_->GetChannelForIndex (index);
	QString url = FindFeedForChannel (channel);
	if (url.isEmpty ())
	{
		qWarning () << Q_FUNC_INFO << "could not found feed for index" << index;
		return;
	}

	QObject *provider = Providers_ ["http"];
	IDownload *isd = qobject_cast<IDownload*> (provider);
	IDirectDownload *idd = qobject_cast<IDirectDownload*> (provider);
	if (!provider || !idd || !isd)
	{
		emit error (tr ("Strange, but no suitable provider found"));
		return;
	}
	if (!isd->CouldDownload (url, LeechCraft::Autostart))
	{
		emit error (tr ("Could not handle URL %1").arg (url));
		return;
	}

	QString name;
	{
		QTemporaryFile file;
		file.open ();
		name = file.fileName ();
		file.close ();
		file.remove ();
	}
	DirectDownloadParams params = { url, name };
	PendingJob pj = { PendingJob::RFeedUpdated, url, name };
	int id = idd->AddJob (params, LeechCraft::Internal | LeechCraft::Autostart |
			LeechCraft::DoNotNotifyUser | LeechCraft::DoNotSaveInHistory);
	PendingJobs_ [id] = pj;
}

QModelIndex Core::GetUnreadChannelIndex ()
{
	return ChannelsModel_->GetUnreadChannelIndex ();
}

void Core::AddToItemBucket (const QModelIndex& index) const
{
	if (!index.isValid () || index.row () >= rowCount ())
		return;

	ItemBucket::Instance ().AddItem (ActivatedChannel_->Items_ [index.row ()]);
}

void Core::AddFromOPML (const QString& filename,
		const QString& tags,
		const std::vector<bool>& mask)
{
	QFile file (filename);
	if (!file.open (QIODevice::ReadOnly))
	{
		emit error (tr ("Could not open file %1 for reading.")
					.arg (filename));
		return;
	}

	QByteArray data = file.readAll ();
	file.close ();

	QString errorMsg;
	int errorLine, errorColumn;
	QDomDocument document;
	if (!document.setContent (data,
				true,
				&errorMsg,
				&errorLine,
				&errorColumn))
	{
		emit error (tr ("XML error, file %1, line %2, column %3, error:<br />%4")
					.arg (filename)
					.arg (errorLine)
					.arg (errorColumn)
					.arg (errorMsg));
		return;
	}

	OPMLParser parser (document);
	if (!parser.IsValid ())
	{
		emit error (tr ("OPML from file %1 is not valid.")
					.arg (filename));
		return;
	}

	OPMLParser::items_container_t items = parser.Parse ();
	for (std::vector<bool>::const_iterator begin = mask.begin (),
			i = mask.end () - 1; i >= begin; --i)
		if (!*i)
		{
			size_t distance = std::distance (mask.begin (), i);
			OPMLParser::items_container_t::iterator eraser = items.begin ();
			std::advance (eraser, distance);
			items.erase (eraser);
		}

	QStringList tagsList = tags.split (" ", QString::SkipEmptyParts);
	for (OPMLParser::items_container_t::const_iterator i = items.begin (),
			end = items.end (); i != end; ++i)
		AddFeed (i->URL_, tagsList + i->Categories_);
}

int Core::columnCount (const QModelIndex& parent) const
{
	return ItemHeaders_.size ();
}

QVariant Core::data (const QModelIndex& index, int role) const
{
	if (!index.isValid () || !ActivatedChannel_ || index.row () >= rowCount ())
		return QVariant ();

	if (role == Qt::DisplayRole)
	{
		switch (index.column ())
		{
			case 0:
				return ActivatedChannel_->Items_ [index.row ()]->Title_;
			case 1:
				return ActivatedChannel_->Items_ [index.row ()]->PubDate_;
			default:
				return QVariant ();
		}
	}
	else if (role == Qt::ForegroundRole)
		return ActivatedChannel_->Items_ [index.row ()]->Unread_ ? Qt::red : Qt::black;
	else
		return QVariant ();
}

Qt::ItemFlags Core::flags (const QModelIndex& index) const
{
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool Core::hasChildren (const QModelIndex& index) const
{
	return !index.isValid ();
}

QVariant Core::headerData (int column, Qt::Orientation orient, int role) const
{
	if (orient == Qt::Horizontal && role == Qt::DisplayRole)
		return ItemHeaders_.at (column);
	else
		return QVariant ();
}

QModelIndex Core::index (int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex (row, column, parent))
		return QModelIndex ();

	return createIndex (row, column);
}

QModelIndex Core::parent (const QModelIndex& index) const
{
	return QModelIndex ();
}

int Core::rowCount (const QModelIndex& parent) const
{
	if (ActivatedChannel_ && !parent.isValid ())
		return ActivatedChannel_->Items_.size ();
	else
		return 0;
}

void Core::currentChannelChanged (const QModelIndex& index)
{
	Channel_ptr ch = ChannelsModel_->GetChannelForIndex (index);
	if (!ch)
		return;
	ActivatedChannel_ = ch.get ();
	reset ();
}

void Core::scheduleSave ()
{
	if (SaveScheduled_)
		return;
	QTimer::singleShot (500, this, SLOT (saveSettings ()));
	SaveScheduled_ = true;
}

namespace
{
	struct IsDateSuitable : public std::unary_function<
							items_container_t::value_type,
							bool>
	{
		QDateTime DateTime_;

		IsDateSuitable (const QDateTime& dt)
			: DateTime_ (dt)
		{
		}

		bool operator() (const items_container_t::value_type& it)
		{
			return it->PubDate_ < DateTime_;
		}
	};
};

void Core::handleJobFinished (int id)
{
	class FileRemoval : public QFile
	{
		public:
			FileRemoval (const QString& name)
				: QFile (name)
			{
			}

			virtual ~FileRemoval ()
			{
				close ();
				remove ();
			}
	};

	if (!PendingJobs_.contains (id))
		return;
	PendingJob pj = PendingJobs_ [id];
	PendingJobs_.remove (id);
	FileRemoval file (pj.Filename_);
	if (!file.open (QIODevice::ReadOnly))
	{
		qWarning () << Q_FUNC_INFO << "could not open file for pj " << pj.Filename_;
		return;
	}
	if (!file.size ())
	{
		emit error (tr ("Downloaded file has null size!"));
		return;
	}
	if (pj.Role_ == PendingJob::RFeedUpdated && !Feeds_.contains (pj.URL_))
	{
		emit error (tr ("Feed with url %1 not found.").arg (pj.URL_));
		return;
	}
	channels_container_t channels;
	if (pj.Role_ != PendingJob::RFeedExternalData)
	{
		QByteArray data = file.readAll ();
		QDomDocument doc;
		QString errorMsg;
		int errorLine, errorColumn;
		if (!doc.setContent (data, true, &errorMsg, &errorLine, &errorColumn))
		{
			file.copy (QDir::tempPath () + "/failedFile.xml");
			emit error (tr ("XML file parse error: %1, line %2, column %3, filename %4")
					.arg (errorMsg)
					.arg (errorLine)
					.arg (errorColumn)
					.arg (pj.Filename_));
			return;
		}

		Parser *parser = ParserFactory::Instance ().Return (doc);
		if (!parser)
		{
			file.copy (QDir::tempPath () + "/failedFile.xml");
			emit error (tr ("Could not find parser to parse file %1").arg (pj.Filename_));
			return;
		}

		if (pj.Role_ == PendingJob::RFeedAdded)
		{
			Feed_ptr feed (new Feed);
			feed->URL_ = pj.URL_;
			Feeds_ [pj.URL_] = feed;
		}

		channels = parser->Parse (Feeds_ [pj.URL_]->Channels_, data);
	}
	QString emitString;
	if (pj.Role_ == PendingJob::RFeedAdded)
	{
		Feeds_ [pj.URL_]->Channels_ = channels;
		for (size_t i = 0; i < channels.size (); ++i)
		{
			channels [i]->Tags_ = pj.Tags_;
			FetchPixmap (channels [i]);
			FetchFavicon (channels [i]);
		}
		ChannelsModel_->AddFeed (Feeds_ [pj.URL_]);
		TagsCompletionModel_->UpdateTags (pj.Tags_);
		StorageBackend_->AddFeed (Feeds_ [pj.URL_]);
	}
	else if (pj.Role_ == PendingJob::RFeedUpdated)
		emitString += HandleFeedUpdated (channels, pj);
	else if (pj.Role_ == PendingJob::RFeedExternalData)
		HandleExternalData (pj.URL_, file);
	UpdateUnreadItemsNumber ();
	if (!emitString.isEmpty ())
	{
		emitString.prepend (tr("Aggregator updated:\r\n"));
		emit showDownloadMessage (emitString);
	}
	scheduleSave ();
}

void Core::handleJobRemoved (int id)
{
	if (PendingJobs_.contains (id))
		PendingJobs_.remove (id);
}

void Core::handleJobError (int id, IDirectDownload::Error)
{
	if (!PendingJobs_.contains (id))
		return;
	PendingJobs_.remove (id);
}

void Core::updateFeeds ()
{
	QObject *provider = Providers_ ["http"];
	IDownload *isd = qobject_cast<IDownload*> (provider);
	IDirectDownload *idd = qobject_cast<IDirectDownload*> (provider);
	if (!provider || !idd || !isd)
	{
		emit error (tr ("Strange, but no suitable provider found"));
		return;
	}
	QList<QString> urls = Feeds_.keys ();
	for (int i = 0; i < urls.size (); ++i)
	{
		if (!isd->CouldDownload (urls.at (i), LeechCraft::Autostart))
		{
			emit error (tr ("Could not handle URL %1").arg (urls.at (i)));
			continue;
		}

		QString filename;
		{
			QTemporaryFile file;
			file.open ();
			filename = file.fileName ();
			file.close ();
			file.remove ();
		}
		DirectDownloadParams params = { urls.at (i), filename };
		PendingJob pj = { PendingJob::RFeedUpdated, urls.at (i), filename };
		int id = idd->AddJob (params, LeechCraft::Internal | LeechCraft::Autostart |
				LeechCraft::DoNotNotifyUser | LeechCraft::DoNotSaveInHistory);
		PendingJobs_ [id] = pj;
	}
}

void Core::fetchExternalFile (const QString& url, const QString& where)
{
	QObject *provider = Providers_ ["http"];
	IDownload *isd = qobject_cast<IDownload*> (provider);
	IDirectDownload *idd = qobject_cast<IDirectDownload*> (provider);
	if (!provider || !idd || !isd)
	{
		emit error (tr ("Strange, but no suitable provider found"));
		throw std::runtime_error ("no suitable provider");
	}
	if (!isd->CouldDownload (url, LeechCraft::Autostart))
	{
		emit error (tr ("Could not handle URL %1").arg (url));
		throw std::runtime_error ("could not handle URL");
	}

	DirectDownloadParams params = { url, where };
	PendingJob pj = { PendingJob::RFeedExternalData, url, where };
	int id = idd->AddJob (params, LeechCraft::Internal | LeechCraft::Autostart |
			LeechCraft::DoNotNotifyUser | LeechCraft::DoNotSaveInHistory);
	PendingJobs_ [id] = pj;
}

void Core::saveSettings ()
{
	QSettings settings (Proxy::Instance ()->GetOrganizationName (), Proxy::Instance ()->GetApplicationName () + "_Aggregator");
	settings.setValue ("GlobalTags", TagsCompletionModel_->GetTags ());

	SaveScheduled_ = false;
}

void Core::updateIntervalChanged ()
{
	UpdateTimer_->setInterval (XmlSettingsManager::Instance ()->property ("UpdateInterval").toInt () * 60 * 1000);
}

void Core::showIconInTrayChanged ()
{
	UpdateUnreadItemsNumber ();
}

void Core::handleSslError (QNetworkReply *reply)
{
	reply->ignoreSslErrors ();
}

QString Core::FindFeedForChannel (const Channel_ptr& channel) const
{
	for (QMap<QString, Feed_ptr>::const_iterator i = Feeds_.begin ();
			i != Feeds_.end (); ++i)
	{
		channels_container_t::const_iterator j =
			std::find (i.value ()->Channels_.begin (),
					i.value ()->Channels_.end (), channel);
		if (j != i.value ()->Channels_.end ())
			return i.key ();
	}
	return QString ();
}

void Core::UpdateUnreadItemsNumber () const
{
	int result = 0;
	for (QMap<QString, Feed_ptr>::const_iterator i = Feeds_.begin ();
			i != Feeds_.end (); ++i)
		for (size_t j = 0, endJSize = i.value ()->Channels_.size ();
				j < endJSize; ++j)
			for (size_t k = 0,
					endKSize = i.value ()->Channels_ [j]->Items_.size ();
				   k < endKSize; ++k)
				result += i.value ()->Channels_ [j]->Items_ [k]->Unread_;
	emit unreadNumberChanged (result);
}

void Core::FetchPixmap (const Channel_ptr& channel)
{
	if (QUrl (channel->PixmapURL_).isValid () &&
			!QUrl (channel->PixmapURL_).isRelative ())
	{
		ExternalData data;
		data.Type_ = ExternalData::TImage;
		data.RelatedChannel_ = channel;
		QString exFName;
		{
			QTemporaryFile file;
			file.open ();
			exFName = file.fileName ();
			file.close ();
			file.remove ();
		}
		try
		{
			fetchExternalFile (channel->PixmapURL_, exFName);
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO << e.what ();
			return;
		}
		PendingJob2ExternalData_ [channel->PixmapURL_] = data;
	}
}

void Core::FetchFavicon (const Channel_ptr& channel)
{
	QUrl oldUrl (channel->Link_);
	oldUrl.setPath ("/favicon.ico");
	QString iconUrl = oldUrl.toString ();

	ExternalData iconData;
	iconData.Type_ = ExternalData::TIcon;
	iconData.RelatedChannel_ = channel;
	QString exFName;
	{
		QTemporaryFile file;
		file.open ();
		exFName = file.fileName ();
		file.close ();
		file.remove ();
	}
	try
	{
		fetchExternalFile (iconUrl, exFName);
	}
	catch (const std::runtime_error& e)
	{
		qWarning () << Q_FUNC_INFO << e.what ();
		return;
	}
	PendingJob2ExternalData_ [iconUrl] = iconData;
}

void Core::HandleExternalData (const QString& url, const QFile& file)
{
	ExternalData data = PendingJob2ExternalData_.take (url);
	if (data.RelatedChannel_.get ())
	{
		switch (data.Type_)
		{
			case ExternalData::TImage:
				data.RelatedChannel_->Pixmap_ =
					QPixmap::fromImage (QImage (file.fileName ()));
				ChannelsModel_->UpdateChannelData (data.RelatedChannel_);
				break;
			case ExternalData::TIcon:
				data.RelatedChannel_->Favicon_ =
					QPixmap::fromImage (QImage (file.fileName ())).scaled (16, 16);;
				ChannelsModel_->UpdateChannelData (data.RelatedChannel_);
				break;
		}
		StorageBackend_->UpdateChannel (data.RelatedChannel_,
				FindFeedForChannel (data.RelatedChannel_));
	}
	else if (data.RelatedFeed_)
	{
	}
}

QString Core::HandleFeedUpdated (const channels_container_t& channels,
		const Core::PendingJob& pj)
{
	QString emitString;

	ChannelsModel_->Update (channels);
	for (size_t i = 0; i < channels.size (); ++i)
	{
		int position = -1;
		for (size_t j = 0,
				endJSize = Feeds_ [pj.URL_]->Channels_.size ();
				j < endJSize; ++j)
			if (*Feeds_ [pj.URL_]->Channels_ [j] == *channels [i])
			{
				position = j;
				break;
			}


		if (position == -1)
		{
			Feeds_ [pj.URL_]->Channels_.push_back (channels [i]);
			StorageBackend_->AddChannel (channels [i], pj.URL_);
			emitString += tr ("Added channel \"%1\" (has %2 items)\r\n")
				.arg (channels [i]->Title_)
				.arg (channels [i]->Items_.size ());
		}
		else
		{
			StorageBackend_->UpdateChannel (channels [i], pj.URL_);
			if (channels [i]->Items_.size ())
				emitString += tr ("Updated channel \"%1\" (%2 new items)\r\n")
					.arg (channels [i]->Title_)
					.arg (channels [i]->Items_.size ());

			bool insertedRows = (ActivatedChannel_ ==
					Feeds_ [pj.URL_]->Channels_ [position].get ());

			Feed_ptr cfeed = Feeds_ [pj.URL_];
			Channel_ptr cchannel = cfeed->Channels_ [position];

			// Okay, this item is new, let's find where to place
			// it. We should place it before the first found item
			// with earlier datetime.
			for (items_container_t::const_iterator j =
					channels.at (i)->Items_.begin (),
					newsize = channels.at (i)->Items_.end ();
					j != newsize; ++j)
			{
				StorageBackend_->AddItem (*j, channels [i]->Link_ + channels [i]->Title_);
				items_container_t::iterator item =
					std::find_if (cchannel->Items_.begin (),
							cchannel->Items_.end (),
							IsDateSuitable ((*j)->PubDate_));
				size_t pos = std::distance (cchannel->Items_.begin (), item);
				if (insertedRows)
					beginInsertRows (QModelIndex (), pos, pos);
				cchannel->Items_.insert (item, *j);
				if (insertedRows)
					endInsertRows ();
			}

			std::for_each (channels.at (i)->Items_.begin (),
					channels.at (i)->Items_.end (),
					boost::bind (&RegexpMatcherManager::HandleItem,
						&RegexpMatcherManager::Instance (),
						_1));

			if (channels.at (i)->LastBuild_.isValid ())
				cchannel->LastBuild_ = channels.at (i)->LastBuild_;
			else if (cchannel->Items_.size ())
				cchannel->LastBuild_ = cchannel->Items_ [0]->PubDate_;
			ChannelsModel_->UpdateChannelData (cchannel);

			size_t ipc = XmlSettingsManager::Instance ()->
				property ("ItemsPerChannel").value<size_t> ();
			if (cchannel->Items_.size () > ipc)
			{
				qDebug () << cchannel->Title_ << cchannel->Items_.size () << ipc;
				if (ActivatedChannel_ == cchannel.get ())
					beginRemoveRows (QModelIndex (), ipc,
							ActivatedChannel_->Items_.size ());
				std::for_each (cchannel->Items_.begin () + ipc,
						cchannel->Items_.end (),
						boost::bind (&StorageBackend::RemoveItem,
							StorageBackend_.get (),
							_1, cchannel->Link_ + cchannel->Title_));
				cchannel->Items_.erase (cchannel->Items_.begin () + ipc,
						cchannel->Items_.end ());
				if (ActivatedChannel_ == cchannel.get ())
					endRemoveRows ();

				ChannelsModel_->UpdateChannelData (cchannel);
			}

			int days = XmlSettingsManager::Instance ()->
				property ("ItemsMaxAge").toInt ();
			QDateTime current = QDateTime::currentDateTime ();
			int removeFrom = -1;
			for (size_t j = 0; j < cchannel->Items_.size (); ++j)
			{
				if (cchannel->Items_ [j]->PubDate_.daysTo (current) > days)
				{
					qDebug () << cchannel->Title_
						<< cchannel->Items_ [j]->PubDate_
						<< current
						<< days
						<< cchannel->Items_ [j]->PubDate_.daysTo (current);
					removeFrom = j;
					break;
				}
			}
			if (removeFrom == 0)
				removeFrom = 1;
			if (removeFrom > 0)
			{
				if (ActivatedChannel_ == cchannel.get ())
					beginRemoveRows (QModelIndex (), removeFrom,
							ActivatedChannel_->Items_.size ());
				cchannel->Items_.erase (cchannel->Items_.begin () + removeFrom,
						cchannel->Items_.end ());
				std::for_each (cchannel->Items_.begin () + removeFrom,
						cchannel->Items_.end (),
						boost::bind (&StorageBackend::RemoveItem,
							StorageBackend_.get (),
							_1, cchannel->Link_ + cchannel->Title_));
				if (ActivatedChannel_ == cchannel.get ())
					endRemoveRows ();

				ChannelsModel_->UpdateChannelData (cchannel);
			}
		}
	}

	return emitString;
}

