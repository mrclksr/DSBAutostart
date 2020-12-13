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

#include <QMessageBox>
#include <QCloseEvent>

#include "mainwin.h"
#include "editwin.h"
#include "qt-helper/qt-helper.h"

#define PB_STYLE "padding: 2px; text-align: left;"

Mainwin::Mainwin(QWidget *parent) : 
    QMainWindow(parent) {
	if ((cmdlist = dsbautostart_init()) == NULL) {
		if (dsbautostart_error()) {
			qh_errx(parent, EXIT_FAILURE, "dsbautostart_init(): %s",
			    dsbautostart_strerror());
		}
	}
	QIcon editIcon	   = qh_loadIcon("edit", NULL);
	QIcon addIcon	   = qh_loadIcon("list-add", NULL);
	QIcon delIcon	   = qh_loadIcon("edit-delete", NULL);
	QIcon undoIcon	   = qh_loadIcon("edit-undo", NULL);
	QIcon redoIcon	   = qh_loadIcon("edit-redo", NULL);
	QIcon saveIcon	   = qh_loadIcon("document-save", NULL);
	QIcon quitIcon	   = qh_loadStockIcon(QStyle::SP_DialogCloseButton);
	if (undoIcon.isNull())
		undoIcon = qh_loadStockIcon(QStyle::SP_ArrowLeft);
	if (addIcon.isNull())
		addIcon = qh_loadStockIcon(QStyle::SP_FileDialogNewFolder);
	if (editIcon.isNull())
		editIcon = qh_loadStockIcon(QStyle::SP_ArrowRight);
	if (delIcon.isNull())
		delIcon = qh_loadStockIcon(QStyle::SP_TrashIcon);
	if (saveIcon.isNull())
		saveIcon = qh_loadStockIcon(QStyle::SP_DialogSaveButton);

	list		   = new List(cmdlist, this);
	redo		   = new QPushButton(redoIcon, tr("&Redo"), this);
	undo		   = new QPushButton(undoIcon, tr("&Undo"), this);
	show_all_cb	   = new QCheckBox(tr("Show all"));
	QWidget *container = new QWidget();
	QLabel *label	   = new QLabel(tr("Add commands to be executed at " \
					"session start"));
	QPushButton *add   = new QPushButton(addIcon,  tr("&New"),    this);
	QPushButton *del   = new QPushButton(delIcon,  tr("&Delete"), this);
	QPushButton *edit  = new QPushButton(editIcon, tr("&Edit"),   this);
	QPushButton *save  = new QPushButton(saveIcon, tr("&Save"),   this);
	QPushButton *quit  = new QPushButton(quitIcon, tr("&Quit"),   this);
	QHBoxLayout *bhbox = new QHBoxLayout;
	QHBoxLayout *hbox  = new QHBoxLayout;
	QVBoxLayout *vbox  = new QVBoxLayout;
	QVBoxLayout *bvbox = new QVBoxLayout;

	show_all_cb->setChecked(false);
	show_all_cb->setToolTip(tr("Show entries not visible for " \
	    "the current desktop environment"));

	undo->setStyleSheet(PB_STYLE);
	undo->setEnabled(list->canUndo());

	redo->setStyleSheet(PB_STYLE);
	redo->setEnabled(list->canRedo());

	add->setStyleSheet(PB_STYLE);
	del->setStyleSheet(PB_STYLE);
	edit->setStyleSheet(PB_STYLE);

	connect(list, SIGNAL(listModified(bool)), this,
	    SLOT(catchListModified(bool)));
	connect(list, SIGNAL(itemDoubleClicked(entry_t *)), this,
	    SLOT(catchItemDoubleClicked(entry_t *)));
	connect(del,  SIGNAL(clicked()), this, SLOT(delClicked()));
	connect(add,  SIGNAL(clicked()), this, SLOT(addClicked()));
	connect(edit, SIGNAL(clicked()), this, SLOT(editClicked()));
	connect(undo, SIGNAL(clicked()), this, SLOT(undoClicked()));
	connect(redo, SIGNAL(clicked()), this, SLOT(redoClicked()));
	connect(show_all_cb, SIGNAL(stateChanged(int)), this,
	    SLOT(showAll(int)));
	connect(save, SIGNAL(clicked(bool)), this, SLOT(save()));
	connect(quit, SIGNAL(clicked(bool)), this, SLOT(quit()));

	bvbox->addStretch(1);
	bvbox->addWidget(undo, 1);
	bvbox->addWidget(redo, 1);
	bvbox->addWidget(add, 1);
	bvbox->addWidget(edit, 1);
	bvbox->addWidget(del, 1);
	bvbox->addStretch(1);

	hbox->addWidget(list);
	hbox->addLayout(bvbox);

	bhbox->addWidget(save, 1, Qt::AlignRight);
	bhbox->addWidget(quit, 0, Qt::AlignRight);

	vbox->addWidget(label, 0, Qt::AlignCenter);
	vbox->addLayout(hbox);
	vbox->addWidget(show_all_cb);
	vbox->addLayout(bhbox);
	vbox->setContentsMargins(15, 15, 15, 15);

	statusBar()->showMessage("");
	container->setLayout(vbox);
	setCentralWidget(container);
	setWindowTitle("DSBAutostart");
	setWindowIcon(qh_loadIcon("system-run", NULL));
}

void
Mainwin::catchListModified(bool status)
{
	undo->setEnabled(list->canUndo());
	redo->setEnabled(list->canRedo());
	statusBar()->showMessage(status ? tr("Modified") : "");
}

void
Mainwin::catchItemDoubleClicked(entry_t *entry)
{
	if (entry == NULL)
		return;
	EditWin edit(entry, this);
	if (edit.exec() == QDialog::Accepted) {
		list->changeCurrentItem(edit.name, edit.command,
		    edit.comment, edit.notShowIn, edit.onlyShowIn, edit.terminal);
		list->redraw();
	}
}

void
Mainwin::save()
{
	if (dsbautostart_save(cmdlist) == -1) {
		qh_errx(this, EXIT_FAILURE, "dsbautostart_save(): %s",
		    dsbautostart_strerror());
	}
	list->unsetModified();
	statusBar()->showMessage(tr("Saved"), 5000);
}

void
Mainwin::quit()
{
	QMessageBox msgBox(this);

	if (!list->modified())
		QApplication::quit();
	msgBox.setWindowModality(Qt::WindowModal);
	msgBox.setText(tr("The file has been modified."));
	msgBox.setWindowTitle(tr("The file has been modified."));
	msgBox.setInformativeText(tr("Do you want to save your changes?"));
	msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard |
	    QMessageBox::Cancel);

	msgBox.setButtonText(QMessageBox::Save, tr("&Save"));
	msgBox.setButtonText(QMessageBox::Discard,
	    tr("&Quit without saving"));
	msgBox.setButtonText(QMessageBox::Cancel, tr("&Cancel"));
	msgBox.setDefaultButton(QMessageBox::Save);
	msgBox.setIcon(QMessageBox::Question);
	QIcon icon = qh_loadStockIcon(QStyle::SP_MessageBoxQuestion);
	msgBox.setWindowIcon(icon);

	switch (msgBox.exec()) {
	case QMessageBox::Save:
		save();
	case QMessageBox::Discard:
		QApplication::quit();
	}
}

void
Mainwin::closeEvent(QCloseEvent *event)
{
	Mainwin::quit();
	event->ignore();
}

void
Mainwin::editClicked()
{
	entry_t *entry = list->currentEntry();
	
	if (entry == NULL)
		return;
	EditWin edit(entry, this);
	if (edit.exec() == QDialog::Accepted) {
		list->changeCurrentItem(edit.name, edit.command,
		    edit.comment, edit.notShowIn, edit.onlyShowIn, edit.terminal);
		list->redraw();
	}
}

void
Mainwin::addClicked()
{
	EditWin edit(NULL, this);

	if (edit.exec() == QDialog::Accepted) {
		list->newItem(edit.name, edit.command, edit.comment,
		    edit.notShowIn, edit.onlyShowIn, edit.terminal);
		list->redraw();
	}
}

void
Mainwin::delClicked()
{
	list->delItem();
}

void
Mainwin::undoClicked()
{
	list->undo();
}

void
Mainwin::redoClicked()
{
	list->redo();
}

void
Mainwin::showAll(int state)
{
	list->setShowAll((state == Qt::Unchecked ? false : true));
}
