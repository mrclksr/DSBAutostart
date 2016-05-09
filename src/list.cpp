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
#include "qt-helper/qt-helper.h"

List::List(dsbautostart_t **head, QWidget *parent)
	: QWidget(parent) {
	
	list = new QListWidget();
	list->setMouseTracking(true);
	this->head = head;

	QVBoxLayout *vbox = new QVBoxLayout;
	vbox->addWidget(list);
	setLayout(vbox);
	list->setToolTip(QString(tr("Double click to edit. Use checkbox to " \
	    "activate/deactivate command")));
	for (dsbautostart_t *ap = *head; ap != NULL; ap = ap->next)
		addItem(ap);
	_modified = false;
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
List::newItem()
{
	dsbautostart_t *as = dsbautostart_add_entry(head, "", true);

	if (as == NULL)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	List::addItem(as);
	_modified = true;
}

void
List::addItem(dsbautostart_t *as)
{
	QListWidgetItem *item = new QListWidgetItem(as->cmd);
	item->setFlags(Qt::ItemIsEditable | Qt::ItemIsUserCheckable |
	    Qt::ItemIsSelectable | Qt::ItemIsEnabled);
	item->setCheckState(as->active ? Qt::Checked : Qt::Unchecked);
	item->setData(Qt::UserRole, qVariantFromValue((void *)as));
	list->addItem(item);
	list->setCurrentItem(item);
	items.append(item);
}

void
List::delItem()
{
	dsbautostart_t *as;
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return;
	as = (dsbautostart_t *)item->data(Qt::UserRole).value<void *>();
	dsbautostart_del_entry(head, as);
	list->removeItemWidget(item);
	items.removeOne(item);
	delete item;

	_modified = true;
}

void
List::moveItemUp()
{
	int row;
	dsbautostart_t *as;
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return;
	as = (dsbautostart_t *)item->data(Qt::UserRole).value<void *>();
	dsbautostart_item_move_up(head, as);

	row = list->currentRow();

	if (row == 0)
		return;
	list->takeItem(row);
	list->insertItem(--row, item);
	list->setCurrentRow(row);

	_modified = true;
}

void
List::moveItemDown()
{
	int row;
	dsbautostart_t *as;
	QListWidgetItem *item = list->currentItem();

	if (item == 0)
		return;
	as = (dsbautostart_t *)item->data(Qt::UserRole).value<void *>();
	dsbautostart_item_move_down(head, as);

	row = list->currentRow();

	if (list->count() - 1 == row)
		return;
	list->takeItem(row);
	list->insertItem(++row, item);
	list->setCurrentRow(row);
	
	_modified = true;
}

void
List::updateItem(QListWidgetItem *item)
{
	bool cbstate = item->checkState() == Qt::Unchecked ? false : true;
	dsbautostart_t *as;

	as = (dsbautostart_t *)item->data(Qt::UserRole).value<void *>();
	if (QString(as->cmd) != item->text() ||
	    (cbstate != as->active)) {
		if (dsbautostart_set(as, item->text().toUtf8().constData(),
                    cbstate) == -1) {
			qh_errx(this, EXIT_FAILURE, "%s",
			    dsbautostart_strerror());
		}
		_modified = true;
	}
}

void
List::update()
{
	bool mod = _modified;

	_modified = false;
	for (int i = 0; i < items.size(); i++)
		updateItem(items.at(i));
	if (mod)
		_modified = true;
}

