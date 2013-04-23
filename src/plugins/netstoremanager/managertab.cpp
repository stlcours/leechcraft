/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "managertab.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QInputDialog>
#include <QComboBox>
#include <QToolButton>
#include <QtDebug>
#include <interfaces/core/ientitymanager.h>
#include <util/util.h>
#include "interfaces/netstoremanager/istorageaccount.h"
#include "interfaces/netstoremanager/istorageplugin.h"
#include "interfaces/netstoremanager/isupportfilelistings.h"
#include "accountsmanager.h"
#include "filestreemodel.h"
#include "xmlsettingsmanager.h"
#include "filesproxymodel.h"

namespace LeechCraft
{
namespace NetStoreManager
{
	ManagerTab::ManagerTab (const TabClassInfo& tc, AccountsManager *am,
			ICoreProxy_ptr proxy, QObject *obj)
	: Parent_ (obj)
	, Info_ (tc)
	, Proxy_ (proxy)
	, ToolBar_ (new QToolBar (this))
	, AM_ (am)
	, ProxyModel_ (new FilesProxyModel (this))
	, TreeModel_ (new FilesTreeModel (this))
	, AccountsBox_ (0)
	, TrashAction_ (0)
	{
		Ui_.setupUi (this);

		Ui_.FilesView_->setModel (ProxyModel_);
		ProxyModel_->setSourceModel (TreeModel_);
		TreeModel_->setHorizontalHeaderLabels ({ tr ("Name"), tr ("Modify") });
		Ui_.FilesView_->header ()->setResizeMode (Columns::Name, QHeaderView::Interactive);

		connect (Ui_.FilesView_->header (),
				SIGNAL (sectionResized (int, int, int)),
				this,
				SLOT (handleFilesViewSectionResized (int, int, int)));
		Ui_.FilesView_->setContextMenuPolicy (Qt::CustomContextMenu);

		CopyURL_ = new QAction (tr ("Copy URL..."), this);
		CopyURL_->setProperty ("ActionIcon", Proxy_->GetIcon ("edit-copy"));
		connect (CopyURL_,
				SIGNAL (triggered ()),
				this,
				SLOT (flCopyUrl ()));
		Copy_ = new QAction (tr ("Copy..."), this);
		Copy_->setProperty ("ActionIcon", Proxy_->GetIcon ("edit-copy"));
		connect (Copy_,
				SIGNAL (triggered ()),
				this,
				SLOT (flCopy ()));
		Move_ = new QAction (tr ("Move..."), this);
		Move_->setProperty ("ActionIcon", Proxy_->GetIcon ("transform-move"));
		connect (Move_,
				SIGNAL (triggered ()),
				this,
				SLOT (flMove ()));
		Rename_ = new QAction (tr ("Rename..."), this);
		Rename_->setProperty ("ActionIcon", Proxy_->GetIcon ("edit-rename"));
		connect (Rename_,
				SIGNAL (triggered ()),
				this,
				SLOT (flRename ()));
		Paste_ = new QAction (tr ("Paste"), this);
		Paste_->setProperty ("ActionIcon", Proxy_->GetIcon ("edit-paste"));
		connect (Paste_,
				SIGNAL (triggered ()),
				this,
				SLOT (flPaste ()));
		DeleteFile_ = new QAction (tr ("Delete..."), this);
		DeleteFile_->setProperty ("ActionIcon", Proxy_->GetIcon ("edit-delete"));
		connect (DeleteFile_,
				SIGNAL (triggered ()),
				this,
				SLOT (flDelete ()));
		MoveToTrash_ = new QAction (tr ("Move to trash"), this);
		MoveToTrash_->setProperty ("ActionIcon", Proxy_->GetIcon ("edit-clear"));
		connect (MoveToTrash_,
				SIGNAL (triggered ()),
				this,
				SLOT (flMoveToTrash ()));
		UntrashFile_ = new QAction (tr ("Restore from trash"), this);
		UntrashFile_->setProperty ("ActionIcon", Proxy_->GetIcon ("edit-undo"));
		connect (UntrashFile_,
				SIGNAL (triggered ()),
				this,
				SLOT (flRestoreFromTrash ()));
		EmptyTrash_ = new QAction (tr ("Empty trash"), this);
		EmptyTrash_->setProperty ("ActionIcon", Proxy_->GetIcon ("trash-empty"));
		connect (EmptyTrash_,
				SIGNAL (triggered ()),
				this,
				SLOT (flEmptyTrash ()));
		CreateDir_ = new QAction (tr ("Create directory"), this);
		CreateDir_->setProperty ("ActionIcon", Proxy_->GetIcon ("folder-new"));
		connect (CreateDir_,
				SIGNAL (triggered ()),
				this,
				SLOT (flCreateDir ()));
		UploadInCurrentDir_ = new QAction (tr ("Upload..."), this);
		UploadInCurrentDir_->setProperty ("ActionIcon", Proxy_->GetIcon ("svn-commit"));
		connect (UploadInCurrentDir_,
				SIGNAL (triggered ()),
				this,
				SLOT (flUploadInCurrentDir ()));
		Download_ = new QAction (tr ("Download"), this);
		Download_->setProperty ("ActionIcon", Proxy_->GetIcon ("download"));
		connect (Download_,
				SIGNAL (triggered ()),
				this,
				SLOT (flDownload ()));

		FillToolbar ();

		connect (AM_,
				SIGNAL (accountAdded (QObject*)),
				this,
				SLOT (handleAccountAdded (QObject*)));
		connect (AM_,
				SIGNAL (accountRemoved (QObject*)),
				this,
				SLOT (handleAccountRemoved (QObject*)));

		connect (Ui_.FilesView_,
				SIGNAL (customContextMenuRequested (const QPoint&)),
				this,
				SLOT (handleContextMenuRequested (const QPoint&)));
		connect (Ui_.FilesView_,
				SIGNAL (doubleClicked (QModelIndex)),
				this,
				SLOT (handleDoubleClicked (QModelIndex)));

		connect (Ui_.FilesView_,
				SIGNAL (itemsAboutToBeCopied (QList<QByteArray>, QByteArray)),
				this,
				SLOT (performCopy (QList<QByteArray>,QByteArray)));
		connect (Ui_.FilesView_,
				SIGNAL (itemsAboutToBeMoved (QList<QByteArray>, QByteArray)),
				this,
				SLOT (performMove (QList<QByteArray>, QByteArray)));
		connect (Ui_.FilesView_,
				SIGNAL (itemsAboutToBeRestoredFromTrash (QList<QByteArray>)),
				this,
				SLOT (performRestoreFromTrash (QList<QByteArray>)));
		connect (Ui_.FilesView_,
				SIGNAL (itemsAboutToBeTrashed (QList<QByteArray>)),
				this,
				SLOT (performMoveToTrash (QList<QByteArray>)));
	}

	TabClassInfo ManagerTab::GetTabClassInfo () const
	{
		return Info_;
	}

	QObject* ManagerTab::ParentMultiTabs ()
	{
		return Parent_;
	}

	void ManagerTab::Remove ()
	{
		emit removeTab (this);
	}

	QToolBar* ManagerTab::GetToolBar () const
	{
		return ToolBar_;
	}

	void ManagerTab::FillToolbar ()
	{
		AccountsBox_ = new QComboBox (this);
		Q_FOREACH (auto acc, AM_->GetAccounts ())
		{
			auto stP = qobject_cast<IStoragePlugin*> (acc->GetParentPlugin ());
			AccountsBox_->addItem (stP->GetStorageIcon (),
					acc->GetAccountName (),
					QVariant::fromValue<IStorageAccount*> (acc));

			if (acc->GetAccountFeatures () & AccountFeature::FileListings)
			{
				connect (acc->GetQObject (),
						SIGNAL (gotListing (const QList<StorageItem>&)),
						this,
						SLOT (handleGotListing (const QList<StorageItem>&)));
				connect (acc->GetQObject (),
						SIGNAL (gotNewItem (StorageItem, QByteArray)),
						this,
						SLOT (handleGotNewItem (StorageItem, QByteArray)));
				connect (acc->GetQObject (),
						SIGNAL (gotFileUrl (QUrl, QByteArray)),
						this,
						SLOT (handleGotFileUrl (QUrl, QByteArray)));
			}
		}

		ToolBar_->addWidget (AccountsBox_);

		Refresh_ = new QAction (Proxy_->GetIcon ("view-refresh"), tr ("Refresh"), this);
		connect (Refresh_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleRefresh ()));
		Upload_ = new QAction (Proxy_->GetIcon ("svn-commit"), tr ("Upload"), this);
		connect (Upload_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleUpload ()));

		ToolBar_->addActions ({ Refresh_, Upload_ });
		ToolBar_->addSeparator ();

		OpenTrash_ = new QAction (Proxy_->GetIcon ("user-trash"),
				tr ("Open trash"), this);
		OpenTrash_->setCheckable (true);
		connect (OpenTrash_,
				SIGNAL (triggered (bool)),
				this,
				SLOT (showTrashContent (bool)));

		Trash_ = new QToolButton (this);
		Trash_->setIcon (Proxy_->GetIcon ("user-trash"));
		Trash_->setText (tr ("Trash"));
		Trash_->setPopupMode (QToolButton::InstantPopup);
		Trash_->addActions ({ OpenTrash_, EmptyTrash_ });

		ToolBar_->addWidget (Trash_);

		ShowAccountActions (AccountsBox_->count ());

		connect (AccountsBox_,
				SIGNAL (currentIndexChanged (int)),
				this,
				SLOT (handleCurrentIndexChanged (int)));
	}

	void ManagerTab::ShowAccountActions (bool show)
	{
		if (Upload_)
			Upload_->setVisible (show);

		if (Refresh_)
			Refresh_->setVisible (show);
	}

	IStorageAccount* ManagerTab::GetCurrentAccount () const
	{
		const int idx = AccountsBox_->currentIndex ();
		if (idx < 0)
			return 0;
		return AccountsBox_->itemData (idx).value<IStorageAccount*> ();
	}

	void ManagerTab::ClearModel ()
	{
		TreeModel_->removeRows (0, TreeModel_->rowCount ());
	}

	void ManagerTab::FillModel (IStorageAccount *acc)
	{
		ClearModel ();
		FillListModel (acc);
	}

	namespace
	{
		QList<QStandardItem*> CreateItems (const StorageItem& storageItem, ICoreProxy_ptr proxy)
		{
			QStandardItem *name = new QStandardItem (storageItem.Name_);
			name->setEditable (false);
			name->setData (storageItem.ID_, ListingRole::ID);
			name->setData (storageItem.Hash_, ListingRole::Hash);
			name->setData (static_cast<int> (storageItem.HashType_),
					ListingRole::HashType);
			name->setData (storageItem.IsDirectory_, ListingRole::IsDirectory);
			name->setData (storageItem.IsTrashed_, ListingRole::InTrash);
			name->setIcon (proxy->GetIcon (storageItem.IsDirectory_ ?
					"inode-directory" :
					storageItem.MimeType_));
			if (name->icon ().isNull ())
				qDebug () << "[NetStoreManager]"
						<< "Unknown mime type:"
						<< storageItem.MimeType_
						<< "for file"
						<< storageItem.Name_
						<< storageItem.ID_;

			QStandardItem *modify = new QStandardItem (storageItem.ModifyDate_
					.toString ("dd.MM.yy hh:mm"));
			modify->setEditable (false);

			return { name, modify };
		}
	}

	void ManagerTab::FillListModel (IStorageAccount *acc)
	{
		if (acc != GetCurrentAccount ())
			return;

		ShowListItemsWithParent (LastParentID_);

		Ui_.FilesView_->header ()->resizeSection (Columns::Name,
				XmlSettingsManager::Instance ().Property ("ViewSectionSize",
						Ui_.FilesView_->header ()->sectionSize (Columns::Name)).toInt ());
	}

	void ManagerTab::RequestFileListings (IStorageAccount *acc)
	{
		ISupportFileListings *sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		if (!sfl)
		{
			qWarning () << Q_FUNC_INFO
					<< acc
					<< "doesn't support FileListings";
			return;
		}
		sfl->RefreshListing ();
	}

	void ManagerTab::RequestFileChanges (IStorageAccount*)
	{
		//TODO
	}

	QList<QByteArray> ManagerTab::GetTrashedFiles () const
	{
		QList<QByteArray> result;
		for (const auto& item : Id2Item_.values ())
			if (item.IsTrashed_)
				result << item.ID_;
		return result;
	}

	QList<QByteArray> ManagerTab::GetSelectedIDs () const
	{
		QList<QByteArray> ids;
		Q_FOREACH (const auto& idx, Ui_.FilesView_->selectionModel ()->selectedRows ())
			ids << idx.data (ListingRole::ID).toByteArray ();

		return ids;
	}

	QByteArray ManagerTab::GetParentIDInListViewMode () const
	{
		return ProxyModel_->index (0, Columns::Name).data (Qt::UserRole + 1)
				.toByteArray ();
	}

	QByteArray ManagerTab::GetCurrentID () const
	{
		QModelIndex idx = Ui_.FilesView_->currentIndex ();
		idx = idx.sibling (idx.row (), Columns::Name);
		return idx.data (ListingRole::ID).toByteArray ();
	}

	void ManagerTab::CallOnSelection (std::function<void (ISupportFileListings*, QList<QByteArray>)> func)
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		func (sfl, GetSelectedIDs ());
	}

	void ManagerTab::ShowListItemsWithParent (const QByteArray& parentId, bool inTrash)
	{
		ClearModel ();
		if (Id2Item_.contains (parentId))
		{
			QStandardItem *upLevel = new QStandardItem (Proxy_->GetIcon ("go-up"), "..");
			upLevel->setData ("netstoremanager.item_uplevel", ListingRole::ID);
			upLevel->setData (parentId);
			upLevel->setFlags (Qt::ItemIsEnabled);
			QStandardItem *upLevelModify = new QStandardItem;
			upLevelModify->setFlags (Qt::ItemIsEnabled);
			TreeModel_->appendRow ({ upLevel, upLevelModify });
		}

		for (const auto& item : Id2Item_.values ())
			if (!inTrash &&
					!item.IsTrashed_)
			{
				if (parentId.isNull () &&
						!Id2Item_.contains (item.ParentID_))
					TreeModel_->appendRow (CreateItems (item, Proxy_));
				else if (!parentId.isNull () &&
						item.ParentID_ == parentId)
					TreeModel_->appendRow (CreateItems (item, Proxy_));
			}
			else if (inTrash &&
					item.IsTrashed_)
			{
				if (parentId.isNull () &&
						(!Id2Item_.contains (item.ParentID_) ||
						!Id2Item_ [item.ParentID_].IsTrashed_))
					TreeModel_->appendRow (CreateItems (item, Proxy_));
				else if (!parentId.isNull () &&
						item.ParentID_ == parentId &&
						Id2Item_ [parentId].IsTrashed_)
					TreeModel_->appendRow (CreateItems (item, Proxy_));
				else if (!parentId.isNull () &&
						!Id2Item_ [parentId].IsTrashed_)
					ShowListItemsWithParent (QByteArray (), true);
			}
	}

	void ManagerTab::handleRefresh ()
	{
		auto acc = GetCurrentAccount ();
		if (!acc)
			return;

		RequestFileListings (acc);
	}

	void ManagerTab::handleUpload ()
	{
		auto acc = GetCurrentAccount ();
		if (!acc)
		{
			QMessageBox::critical (this,
					tr ("Error"),
					tr ("You first need to add an account."));
			return;
		}

		const QString& filename = QFileDialog::getOpenFileName (this,
				tr ("Select file for upload"),
				XmlSettingsManager::Instance ().Property ("DirUploadFrom", QDir::homePath ()).toString ());
		if (filename.isEmpty ())
			return;

		XmlSettingsManager::Instance ().setProperty ("DirUploadFrom",
				QFileInfo (filename).dir ().absolutePath ());
		QByteArray parentId;
		parentId = GetParentIDInListViewMode ();

		emit uploadRequested (acc, filename, parentId);
	}

	void ManagerTab::handleDoubleClicked (const QModelIndex& idx)
	{
		if (idx.data (ListingRole::ID).toByteArray () == "netstoremanager.item_uplevel")
		{
			ShowListItemsWithParent (Id2Item_ [idx.data (Qt::UserRole + 1).toByteArray ()].ParentID_,
					OpenTrash_->isChecked ());
			return;
		}

		if (!idx.data (ListingRole::IsDirectory).toBool ())
			return;

		ShowListItemsWithParent (idx.data (ListingRole::ID).toByteArray (),
				OpenTrash_->isChecked ());
	}

	void ManagerTab::handleAccountAdded (QObject *accObj)
	{
		auto acc = qobject_cast<IStorageAccount*> (accObj);
		if (!acc)
		{
			qWarning () << Q_FUNC_INFO
					<< "added account is not an IStorageAccount";
			return;
		}

		auto stP = qobject_cast<IStoragePlugin*> (acc->GetParentPlugin ());
		AccountsBox_->addItem (stP->GetStorageIcon (),
				acc->GetAccountName (),
				QVariant::fromValue<IStorageAccount*> (acc));

		if (AccountsBox_->count () == 1)
			ShowAccountActions (true);
	}

	void ManagerTab::handleAccountRemoved (QObject *accObj)
	{
		auto acc = qobject_cast<IStorageAccount*> (accObj);
		if (!acc)
		{
			qWarning () << Q_FUNC_INFO
					<< "removed account is not an IStorageAccount";
			return;
		}

		for (int i = AccountsBox_->count () - 1; i >= 0; --i)
			if (AccountsBox_->itemData (i).value<IStorageAccount*> () == acc)
				AccountsBox_->removeItem (i);

		if (!AccountsBox_->count ())
			ShowAccountActions (false);
	}

	void ManagerTab::handleGotListing (const QList<StorageItem>& items)
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc ||
				sender () != acc->GetQObject ())
			return;

		LastParentID_ = GetParentIDInListViewMode ();

		Id2Item_.clear ();
		for (auto item : items)
			Id2Item_ [item.ID_] = item;

		FillModel (acc);

		Proxy_->GetEntityManager ()->HandleEntity (Util::MakeNotification ("NetStoreManager",
				tr ("File list updated"), PInfo_));

		Trash_->setIcon (Proxy_->GetIcon (GetTrashedFiles ().isEmpty () ?
			"user-trash-full" :
			"user-trash"));
	}

	void ManagerTab::handleGotNewItem (const StorageItem& item, const QByteArray&)
	{
		Id2Item_ [item.ID_] = item;
		LastParentID_ = GetParentIDInListViewMode ();
		FillModel (GetCurrentAccount ());
	}

	void ManagerTab::handleFilesViewSectionResized (int index,
			int, int newSize)
	{
		if (index == Columns::Name)
			XmlSettingsManager::Instance ().setProperty ("ViewSectionSize", newSize);
	}

	void ManagerTab::performCopy (const QList<QByteArray>& ids,
			const QByteArray& newParentId)
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		sfl->Copy (ids, newParentId);
	}

	void ManagerTab::performMove (const QList<QByteArray>& ids,
			const QByteArray& newParentId)
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		sfl->Move (ids, newParentId);
	}

	void ManagerTab::performRestoreFromTrash (const QList<QByteArray>& ids)
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		sfl->RestoreFromTrash (ids);
	}

	void ManagerTab::performMoveToTrash (const QList<QByteArray>& ids)
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		sfl->MoveToTrash (ids);
	}

	void ManagerTab::flCopy ()
	{
		TransferedIDs_ = { TransferOperation::Copy, GetSelectedIDs () };
	}

	void ManagerTab::flMove ()
	{
		TransferedIDs_ = { TransferOperation::Move, GetSelectedIDs () };
	}

	void ManagerTab::flRename ()
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		const QString& oldName = Ui_.FilesView_->currentIndex ().data ().toString ();
		const auto& id = GetCurrentID ();
		QString name = QInputDialog::getText (this,
				"Rename",
				tr ("New name:"),
				QLineEdit::Normal,
				oldName);
		if (name.isEmpty () ||
				name == oldName)
			return;

		sfl->Rename (id, name);
	}

	void ManagerTab::flPaste ()
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());

		switch (TransferedIDs_.first)
		{
		case TransferOperation::Copy:
			sfl->Copy (TransferedIDs_.second, GetParentIDInListViewMode ());
			break;
		case TransferOperation::Move:
			sfl->Move (TransferedIDs_.second, GetParentIDInListViewMode ());
			break;
		}
		TransferedIDs_.second.clear ();
	}

	void ManagerTab::flDelete ()
	{
		CallOnSelection ([] (ISupportFileListings *sfl, const QList<QByteArray>& ids)
			{ sfl->Delete (ids); });
	}

	void ManagerTab::flMoveToTrash ()
	{
		CallOnSelection ([] (ISupportFileListings *sfl, const QList<QByteArray>& ids)
			{ sfl->MoveToTrash (ids); });
	}

	void ManagerTab::flRestoreFromTrash ()
	{
		CallOnSelection ([] (ISupportFileListings *sfl, const QList<QByteArray>& ids)
			{ sfl->RestoreFromTrash (ids); });
	}

	void ManagerTab::flEmptyTrash ()
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		if (sfl)
			sfl->Delete (GetTrashedFiles (), false);
		else
			qWarning () << Q_FUNC_INFO
					<< acc->GetQObject ()
					<< "is not an ISupportFileListings object";
	}

	void ManagerTab::flCreateDir ()
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		if (!(sfl->GetListingOps () & ListingOp::DirectorySupport))
			return;

		QString name = QInputDialog::getText (this,
				"Create directory",
				tr ("New directory name:"));
		if (name.isEmpty ())
			return;

		sfl->CreateDirectory (name, GetParentIDInListViewMode ());
	}

	void ManagerTab::flUploadInCurrentDir ()
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		if (!(sfl->GetListingOps () & ListingOp::DirectorySupport))
			return;

		const QString& filename = QFileDialog::getOpenFileName (this,
				tr ("Select file for upload"),
				QDir::homePath ());
		if (filename.isEmpty ())
			return;

		emit uploadRequested (acc, filename, GetParentIDInListViewMode ());
	}

	void ManagerTab::flDownload ()
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		acc->Download (GetCurrentID (), QString ());
	}

	void ManagerTab::flCopyUrl ()
	{
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		const QByteArray id = GetCurrentID ();
		if (!Id2Item_ [id].Url_.isEmpty () &&
				Id2Item_ [id].Url_.isValid ())
			handleGotFileUrl (Id2Item_ [id].Url_);
		else
			qobject_cast<ISupportFileListings*> (acc->GetQObject ())->RequestUrl (id);
	}

	void ManagerTab::showTrashContent (bool show)
	{
		ShowListItemsWithParent (QByteArray (), show);
	}

	void ManagerTab::handleContextMenuRequested (const QPoint& point)
	{
		QList<QModelIndex> idxs = Ui_.FilesView_->selectionModel ()->selectedRows ();
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
			return;

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		const bool trashSupport = sfl->GetListingOps () & ListingOp::TrashSupporting;

		QMenu *menu = new QMenu;
		UploadInCurrentDir_->setText (tr ("Upload..."));

		if (!idxs.isEmpty ())
		{
			QList<QAction*> editActions = { Copy_, Move_, Rename_, DeleteFile_ };

			menu->addAction (CopyURL_);
			menu->addSeparator ();

			menu->addAction (Download_);
			menu->addSeparator ();
			QList<QAction*> actions;
			if (trashSupport &&
					idxs.at (0).data (ListingRole::InTrash).toBool ())
				actions << UntrashFile_;
			else
			{
				menu->insertAction (Download_, UploadInCurrentDir_);
				actions << editActions;
				if (trashSupport)
					actions << MoveToTrash_;
			}
			actions << DeleteFile_;
			menu->addActions (actions);

			const auto& id = GetCurrentID ();
			StorageItem item;
			if (Id2Item_.contains (id))
				item = Id2Item_ [id];

			if (item.IsValid () &&
					!item.ExportLinks.isEmpty ())
			{
				QMenu *exportMenu = new QMenu (tr ("Export to..."), menu);
				auto exportAct = menu->insertMenu (Download_, exportMenu);
				exportAct->setProperty ("ActionIcon", Proxy_->GetIcon ("document-export"));
				for (const auto& key : item.ExportLinks.keys ())
				{
					const auto& pair = item.ExportLinks [key];
					QAction *action = new QAction (Proxy_->GetIcon (pair.first),
							pair.second, exportMenu);
					action->setProperty ("url", key);
					exportMenu->addAction (action);
					connect (exportMenu,
							SIGNAL (triggered (QAction*)),
							this,
							SLOT (handleExportMenuTriggered (QAction*)),
							Qt::UniqueConnection);
				}
			}
		}
		else
			menu->addAction (UploadInCurrentDir_);

		if (!TransferedIDs_.second.isEmpty () &&
				!OpenTrash_->isChecked ())
		{
			auto sep = menu->insertSeparator (menu->actions ().at (0));
			menu->insertAction (sep, Paste_);
		}

		menu->addSeparator ();
		menu->addAction (CreateDir_);

		if (!menu->isEmpty ())
			menu->exec (Ui_.FilesView_->viewport ()->
					mapToGlobal (QPoint (point.x (), point.y ())));
		menu->deleteLater ();
	}

	void ManagerTab::handleExportMenuTriggered (QAction *action)
	{
		if (!action ||
				action->property ("url").isNull ())
			return;

		Proxy_->GetEntityManager ()->HandleEntity (Util::MakeEntity (action->property ("url").toUrl (),
				QString (),
				OnlyHandle | FromUserInitiated));
	}

	void ManagerTab::handleCurrentIndexChanged (int)
	{
		ClearModel ();
		IStorageAccount *acc = GetCurrentAccount ();
		if (!acc)
		{
			qWarning () << Q_FUNC_INFO
					<< acc
					<< "is not an IStorageAccount object";
			return;
		}

		Id2Item_.clear ();
		RequestFileListings (acc);

		auto sfl = qobject_cast<ISupportFileListings*> (acc->GetQObject ());
		DeleteFile_->setVisible (sfl->GetListingOps () & ListingOp::Delete);
		MoveToTrash_->setVisible (sfl->GetListingOps () & ListingOp::TrashSupporting);
	}

	void ManagerTab::handleGotFileUrl (const QUrl& url, const QByteArray&)
	{
		if (url.isEmpty () ||
				!url.isValid ())
			return;

		const QString& str = url.toString ();
		qApp->clipboard ()->setText (str, QClipboard::Clipboard);
		qApp->clipboard ()->setText (str, QClipboard::Selection);

		QString text = tr ("File URL has been copied to the clipboard.");
		Proxy_->GetEntityManager ()->
				HandleEntity (Util::MakeNotification ("NetStoreManager", text, PInfo_));
	}

}
}
