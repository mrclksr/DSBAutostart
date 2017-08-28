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
#include "qt-helper/qt-helper.h"

#define PB_STYLE "padding: 2px; text-align: left;"

Mainwin::Mainwin(QWidget *parent) : 
    QMainWindow(parent) {
	QIcon icon;

	if ((cmdlist = dsbautostart_read()) == NULL) {
		if (dsbautostart_error()) {
			qh_errx(parent, EXIT_FAILURE, "dsbautostart_read(): %s",
			    dsbautostart_strerror());
		}
	}
	QWidget *container = new QWidget();
	if (cmdlist == NULL)
		qh_errx(this, EXIT_FAILURE, "%s", dsbautostart_strerror());
	list = new List(cmdlist, this);
	connect(list, SIGNAL(listModified(bool)), this,
	    SLOT(catchListModified(bool)));

	QLabel *label = new QLabel(tr("Add commands to be executed at " \
	    "session start"));

	QVBoxLayout *bvbox = new QVBoxLayout;

	icon = qh_loadIcon("edit-undo", NULL);
	if (icon.isNull())
		icon = qh_loadStockIcon(QStyle::SP_ArrowLeft);
	undo  = new QPushButton(icon, tr("&Undo"), this);
	undo->setStyleSheet(PB_STYLE);

	undo->setEnabled(list->canUndo());

	icon = qh_loadIcon("edit-redo", NULL);
	if (icon.isNull())
		icon = qh_loadStockIcon(QStyle::SP_ArrowRight);
	redo  = new QPushButton(icon, tr("&Redo"), this);
	redo->setStyleSheet(PB_STYLE);
	redo->setEnabled(list->canRedo());

	icon = qh_loadIcon("list-add", NULL);
	if (icon.isNull())
		icon = qh_loadStockIcon(QStyle::SP_FileDialogNewFolder);
	QPushButton *add  = new QPushButton(icon, tr("&Add"), this);
	add->setStyleSheet(PB_STYLE);
	icon = qh_loadIcon("edit-delete", NULL);
	if (icon.isNull())
		icon = qh_loadStockIcon(QStyle::SP_TrashIcon);
	QPushButton *del  = new QPushButton(icon, tr("&Delete"), this);
	del->setStyleSheet(PB_STYLE);
	icon = qh_loadIcon("go-up", NULL);
	if (icon.isNull())
		icon = qh_loadStockIcon(QStyle::SP_ArrowUp);
	QPushButton *up   = new QPushButton(icon, tr("&Up"), this);
	up->setStyleSheet(PB_STYLE);
	icon = qh_loadIcon("go-down", NULL);
	if (icon.isNull())
		icon = qh_loadStockIcon(QStyle::SP_ArrowDown);
	QPushButton *down = new QPushButton(icon, tr("Dow&n"), this);
	down->setStyleSheet(PB_STYLE);

	connect(up,   SIGNAL(clicked()), this, SLOT(upClicked()));
	connect(del,  SIGNAL(clicked()), this, SLOT(delClicked()));
	connect(add,  SIGNAL(clicked()), this, SLOT(addClicked()));
	connect(down, SIGNAL(clicked()), this, SLOT(downClicked()));
	connect(undo, SIGNAL(clicked()), this, SLOT(undoClicked()));
	connect(redo, SIGNAL(clicked()), this, SLOT(redoClicked()));

	bvbox->addStretch(1);
	bvbox->addWidget(undo, 1, 0);
	bvbox->addWidget(redo, 1, 0);
	bvbox->addWidget(add, 1, 0);
	bvbox->addWidget(del, 1, 0);
	bvbox->addWidget(up, 1, 0);
	bvbox->addWidget(down, 1, 0);
	bvbox->addStretch(1);
	
	QHBoxLayout *hbox = new QHBoxLayout;
	hbox->addWidget(list);
	hbox->addLayout(bvbox);

	icon = qh_loadIcon("document-save", NULL);
	if (icon.isNull())
		icon = qh_loadStockIcon(QStyle::SP_DialogSaveButton);
	QPushButton *save = new QPushButton(icon, tr("&Save"), this);
	icon = qh_loadStockIcon(QStyle::SP_DialogCloseButton);
	QPushButton *quit = new QPushButton(icon, tr("&Quit"), this);

	connect(save, SIGNAL(clicked(bool)), this, SLOT(save()));
	connect(quit, SIGNAL(clicked(bool)), this, SLOT(quit()));

	QHBoxLayout *bhbox = new QHBoxLayout;

	bhbox->addWidget(save,  1, Qt::AlignRight);
	bhbox->addWidget(quit,  0, Qt::AlignRight);

	QVBoxLayout *vbox = new QVBoxLayout;

	vbox->addWidget(label, 0, Qt::AlignCenter);
	vbox->addLayout(hbox);
	vbox->addLayout(bhbox);
	statusBar()->showMessage("");
	container->setLayout(vbox);
	setCentralWidget(container);
	setWindowTitle("DSBAutostart");
	setMinimumSize(500, 300);
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
Mainwin::save()
{
	list->update();
	if (dsbautostart_write(cmdlist) == -1) {
		qh_errx(this, EXIT_FAILURE, "dsbautostart_write(): %s",
		    dsbautostart_strerror());
	}
	list->unsetModified();
	statusBar()->showMessage(tr("Saved"), 5000);
}

void
Mainwin::quit()
{
	QMessageBox msgBox(this);

	list->update();
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
Mainwin::upClicked()
{
	list->moveItemUp();
}

void
Mainwin::downClicked()
{
	list->moveItemDown();
}

void
Mainwin::addClicked()
{
	list->newItem();
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
	undo->setEnabled(list->canUndo());
}

void
Mainwin::redoClicked()
{
	list->redo();
	redo->setEnabled(list->canRedo());
}

