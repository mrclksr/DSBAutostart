/*-
 * Copyright (c) 2016 Marcel Kaiser. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QVBoxLayout>

#include "list.h"
#include "desktopfile.h"
#include "qt-helper/qt-helper.h"

List::List(dsbautostart_t *as, QWidget *parent)
	: QWidget(parent) {
	
	list = new ListWidget(parent);
	list->setMouseTracking(true);
	this->as = as;

	QVBoxLayout *vbox = new QVBoxLayout;
	vbox->addWidget(list);
	setLayout(vbox);
	list->setToolTip(QString(tr("Double click to edit. Use checkbox to " \
	    "activate/deactivate command")));
	if ((ascp = dsbautostart_copy(as)) == NULL) {
		if (dsbautostart_error()) {
			qh_err(this, EXIT_FAILURE, "%s",
			    dsbautostart_strerror());
		}
	}
	for (entry_t *entry = as->entry; entry != NULL; entry = entry->next)
		addItem(entry);
	_modified = false;
	connect(list, SIGNAL(itemChanged(QListWidgetItem *)), this,
	    SLOT(catchItemChanged(QListWidgetItem *)));
	connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this,
	    SLOT(catchDoubleClicked(QListWidgetItem *)));
	connect(list, SIGNAL(itemDroped(QStringList &)), this,
	    SLOT(addDesktopFiles(QStringList &)));
	connect(list, SIGNAL(deleteKeyPressed()), this, SLOT(delItem()));
}

bool
List::modified()
{
	return (_modified);
}

void
List::unsetModified()
{
	dsbautostart_free(ascp);
	if ((ascp = dsbautostart_copy(as)) == NULL) {
		if (dsbautostart_error()) {
			qh_err(this, EXIT_FAILURE, "%s",
			    dsbautostart_strerror());
		}
	}
	_modified = false;
}

void
List::newItem()
{
	entry_t *entry = dsbautostart_add_entry(as, "", true);

	if (entry == NULL)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	QListWidgetItem *item = List::addItem(entry);
	list->editItem(item);
	compare();
}

QListWidgetItem *
List::addItem(entry_t *entry)
{
	QListWidgetItem *item = new QListWidgetItem(entry->cmd);
	item->setFlags(Qt::ItemIsEditable | Qt::ItemIsUserCheckable |
	    Qt::ItemIsSelectable | Qt::ItemIsEnabled);
	item->setCheckState(entry->active ? Qt::Checked : Qt::Unchecked);
	item->setData(Qt::UserRole, qVariantFromValue((void *)entry));
	list->addItem(item);
	items.append(item);

	return (item);
}

void
List::catchItemChanged(QListWidgetItem *item)
{
	updateItem(item);
	compare();
}

void
List::catchDoubleClicked(QListWidgetItem *item)
{
	list->editItem(item);
	list->setCurrentItem(item, QItemSelectionModel::Deselect);
}

void
List::delItem()
{
	entry_t *entry;
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return;
	entry = (entry_t *)item->data(Qt::UserRole).value<void *>();
	if (dsbautostart_del_entry(as, entry) == NULL)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	list->removeItemWidget(item);
	items.removeOne(item);
	delete item;
	compare();
}

void
List::moveItemUp()
{
	int row;
	entry_t *entry;
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return;
	entry = (entry_t *)item->data(Qt::UserRole).value<void *>();
	if (dsbautostart_entry_move_up(as, entry) == NULL)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	row = list->currentRow();
	if (row == 0)
		return;
	list->takeItem(row);
	list->insertItem(--row, item);
	list->setCurrentRow(row);
	compare();
}

void
List::moveItemDown()
{
	int row;
	entry_t *entry;
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return;
	entry = (entry_t *)item->data(Qt::UserRole).value<void *>();
	if (dsbautostart_entry_move_down(as, entry) == NULL)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	row = list->currentRow();

	if (list->count() - 1 == row)
		return;
	list->takeItem(row);
	list->insertItem(++row, item);
	list->setCurrentRow(row);
	compare();
}

void
List::updateItem(QListWidgetItem *item)
{
	bool cbstate = item->checkState() == Qt::Unchecked ? false : true;
	entry_t *entry;

	entry = (entry_t *)item->data(Qt::UserRole).value<void *>();
	if (dsbautostart_set(as, entry, item->text().toUtf8().constData(),
	    cbstate) == -1)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	compare();
}

void
List::update()
{
	for (int i = 0; i < items.size(); i++)
		updateItem(items.at(i));
}

void
List::undo()
{
	dsbautostart_undo(as);
	list->clear();
	items.clear();
	for (entry_t *entry = as->entry; entry != NULL; entry = entry->next)
		addItem(entry);
	compare();
}

void
List::redo()
{
	dsbautostart_redo(as);
	list->clear();
	items.clear();
	for (entry_t *entry = as->entry; entry != NULL; entry = entry->next)
		addItem(entry);
	compare();
}

bool
List::canUndo()
{
	return (dsbautostart_can_undo(as));
}

bool
List::canRedo()
{

	return (dsbautostart_can_redo(as));
}

void
List::compare()
{
	if (!dsbautostart_cmp(ascp, as)) {
		emit listModified(true);
		_modified = true;
	} else	{
		_modified = false;
		emit listModified(false);
	}
}

void
List::addDesktopFiles(QStringList &list)
{
	entry_t *entry;

	for (QString s : list) {
		DesktopFile df(s);
		if (df.read() == -1)
			continue;
		entry = dsbautostart_add_entry(as, df.cmd.toLocal8Bit().data(), true);
		if (entry == NULL) {
			qh_errx(this, EXIT_FAILURE, "%s",
			    dsbautostart_strerror());
		}
		List::addItem(entry);
	}
	compare();
}
