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
#include <err.h>

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
	list->setToolTip(QString(tr("Use Drag & Drop to add desktop files.")));
	list->setLayoutMode(QListView::Batched);
	for (entry_t *entry = as->cur_entries; entry != NULL; entry = entry->next) {
		if (!entry->deleted && !entry->exclude)
			addItem(entry);
	}
	_modified = false;
	connect(list, SIGNAL(itemDroped(QStringList &)), this,
	    SLOT(addDesktopFiles(QStringList &)));
	connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this,
	    SLOT(catchDoubleClicked(QListWidgetItem *)));
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
	_modified = false;
}

void
List::setShowAll(bool show)
{
	showAll = show;
	redraw();
}

entry_t *
List::currentEntry()
{
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return (NULL);
	return ((entry_t *)item->data(Qt::UserRole).value<void *>());
}

void
List::changeCurrentItem(QByteArray &name, QByteArray &command,
    QByteArray &comment, QByteArray &notShowIn, QByteArray &onlyShowIn,
    bool terminal)
{
	char		*nsi, *osi;
	entry_t		*entry;
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return;
	if (notShowIn.isEmpty())
		nsi = NULL;
	else
		nsi = notShowIn.data();
	if (onlyShowIn.isEmpty())
		osi = NULL;
	else
		osi = onlyShowIn.data();
	entry = (entry_t *)item->data(Qt::UserRole).value<void *>();
	if (dsbautostart_entry_set(as, entry, command.data(),
	    name.data(), comment.data(), nsi, osi, terminal) == -1)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	item->setText(command);
	if ((entry->df->name != NULL && *entry->df->name != '\0') ||
	    (entry->df->comment != NULL && *entry->df->comment != '\0')) {
		warnx("%s, %s", entry->df->name, entry->df->comment);
		item->setToolTip(QString("%1\n%2")
			.arg(entry->df->name != NULL ? entry->df->name : "")
			.arg(entry->df->comment != NULL ? entry->df->comment : ""));
	} else
		item->setToolTip(QString(tr("No further description available")));
	compare();
}

void
List::newItem(QByteArray &name, QByteArray &command, QByteArray &comment,
    QByteArray &notShowIn, QByteArray &onlyShowIn, bool terminal)
{
	char	*osi, *nsi;
	entry_t *entry;

	if (notShowIn.isEmpty())
		nsi = NULL;
	else
		nsi = notShowIn.data();
	if (onlyShowIn.isEmpty())
		osi = NULL;
	else
		osi = onlyShowIn.data();
	entry = dsbautostart_entry_add(as, command.data(), name.data(),
		    comment.data(), nsi, osi, terminal);
	if (entry == NULL)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	QListWidgetItem *item = List::addItem(entry);
	item->setText(command);
	compare();
}

QListWidgetItem *
List::addItem(entry_t *entry)
{
	QListWidgetItem *item = new QListWidgetItem(
	    entry->df->exec != NULL ? entry->df->exec : "");
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
	item->setData(Qt::UserRole, QVariant::fromValue((void *)entry));
	list->addItem(item);
	items.append(item);
	if ((entry->df->name != NULL && *entry->df->name != '\0') ||
	    (entry->df->comment != NULL && *entry->df->comment != '\0')) {
		item->setToolTip(QString("%1\n%2")
			.arg(entry->df->name != NULL ? entry->df->name : "")
			.arg(entry->df->comment != NULL ? entry->df->comment : ""));
	} else
		item->setToolTip(QString(tr("No further description available")));
	return (item);
}

void
List::delItem()
{
	entry_t *entry;
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return;
	entry = (entry_t *)item->data(Qt::UserRole).value<void *>();
	if (dsbautostart_entry_del(as, entry) == NULL)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	list->removeItemWidget(item);
	items.removeOne(item);
	delete item;
	compare();
}

void
List::catchDoubleClicked(QListWidgetItem *item)
{
	emit itemDoubleClicked((entry_t *)item->data(Qt::UserRole).value<void *>());
}

void
List::undo()
{
	dsbautostart_undo(as);
	redraw();
	compare();
}

void
List::redo()
{
	dsbautostart_redo(as);
	redraw();
	compare();
}

void
List::redraw()
{
	list->clear();
	items.clear();
	for (entry_t *entry = as->cur_entries; entry != NULL; entry = entry->next) {
		if (entry->deleted)
			continue;
		if (entry->exclude && !showAll)
			continue;
		addItem(entry);
	}
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
	if (dsbautostart_changed(as)) {
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
		entry = dsbautostart_df_add(as, s.toLocal8Bit().data());
		if (entry == NULL) {
			qh_errx(this, EXIT_FAILURE, "%s",
			    dsbautostart_strerror());
		}
		List::addItem(entry);
	}
	compare();
}
