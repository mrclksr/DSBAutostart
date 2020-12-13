/*-
 * Copyright (c) 2019 Marcel Kaiser. All rights reserved.
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

#include <QFormLayout>

#include "editwin.h"
#include "qt-helper/qt-helper.h"

EditWin::EditWin(entry_t *entry, QWidget *parent) : 
    QDialog(parent) {
	QString list;
	QIcon winIcon	    = qh_loadIcon("edit", 0);
	QIcon okIcon	    = qh_loadStockIcon(QStyle::SP_DialogOkButton, 0);
	QIcon cancelIcon    = qh_loadStockIcon(QStyle::SP_DialogCancelButton,
	    NULL);
	statusBar	    = new QStatusBar;
	ok_pb		    = new QPushButton(okIcon, tr("&Ok"));
	QPushButton *cancel = new QPushButton(cancelIcon, tr("&Cancel"));
	QVBoxLayout *layout = new QVBoxLayout(this);
	QHBoxLayout *bbox   = new QHBoxLayout;
	QFormLayout *form   = new QFormLayout;

	name = command = comment = NULL;
	terminal = false;

	if (entry != NULL && entry->df->name != NULL)
		name_edit = new QLineEdit(entry->df->name);
	else
		name_edit = new QLineEdit("");
	if (entry != NULL && entry->df->exec != NULL)
		command_edit = new QLineEdit(entry->df->exec);
	else
		command_edit = new QLineEdit("");
	if (entry != NULL && entry->df->comment != NULL)
		comment_edit = new QLineEdit(entry->df->comment);
	else
		comment_edit = new QLineEdit("");
	terminal_cb = new QCheckBox(tr("Run in terminal"));

	if (entry != NULL)
		terminal_cb->setChecked(entry->df->terminal);
	else
		terminal_cb->setChecked(false);
	setWindowIcon(winIcon);
	if (entry != NULL)
		setWindowTitle(tr("Edit"));
	else
		setWindowTitle(tr("New"));

	form->addRow(tr("Name"), name_edit);
	form->addRow(tr("Command"), command_edit);
	form->addRow(tr("Comment"), comment_edit);

	layout->addLayout(form);
	layout->addWidget(createVisibilityBox(entry));
	layout->addWidget(terminal_cb);
	layout->addStretch(1);

	bbox->addWidget(ok_pb, 1, Qt::AlignRight);
	bbox->addWidget(cancel, 0, Qt::AlignRight);
	layout->addLayout(bbox);
	layout->addWidget(statusBar);

	connect(command_edit, SIGNAL(textChanged(const QString &)),
	    this, SLOT(validate()));
	connect(terminal_cb, SIGNAL(stateChanged(int)),
	    this, SLOT(terminalToggled(int)));
	connect(ok_pb, SIGNAL(clicked()), this, SLOT(acceptSlot()));
	connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));
	if (parent) {
		int width = parent->width() * 80 / 100;
		if (width > 0)
			resize(width, height());
	}
	validate();
}

void
EditWin::validate()
{
	QString msg = "";

	if (command_edit->text().length() < 1) {
		ok_pb->setEnabled(false);
		msg = tr("Command field must not be empty");
	} else
		ok_pb->setEnabled(true);
	statusBar->showMessage(msg);
}

void
EditWin::terminalToggled(int state)
{
	terminal = (state == Qt::Unchecked ? false : true);
}

void
EditWin::acceptSlot(void)
{
	name	= QString(name_edit->text()).toLocal8Bit();
	command	= QString(command_edit->text()).toLocal8Bit();
	comment = QString(comment_edit->text()).toLocal8Bit();
	if (nsi_rb->isChecked())
		notShowIn = QString(list_le->text()).toLocal8Bit();
	else if (osi_rb->isChecked())
		onlyShowIn = QString(list_le->text()).toLocal8Bit();
	accept();
}

QGroupBox *
EditWin::createVisibilityBox(entry_t *entry)
{
	QGroupBox   *box  = new QGroupBox(tr("Visibility"));
	QVBoxLayout *vbox = new QVBoxLayout;
	nsi_rb		  = new QRadioButton(tr("Not show in:"), this);
	osi_rb		  = new QRadioButton(tr("Only show in:"), this);
	list_le		  = new QLineEdit;

	list_le->setToolTip(tr("Define a semicolon (;) separated list of "  \
	    "desktop environment names if\nyou want to limit the execution " \
	    "of this command to certain environments.\nE.g.: MATE;XFCE"));
	list_le->setEnabled(false);
	if (entry != NULL) {
		if (entry->df->not_show_in != NULL) {
			nsi_rb->setChecked(true);
			list_le->setText(entry->df->not_show_in);
		} else if (entry->df->only_show_in != NULL) {
			osi_rb->setChecked(true);
			list_le->setText(entry->df->only_show_in);
		}
	}
	if (nsi_rb->isChecked() || osi_rb->isChecked())
		list_le->setEnabled(true);
	vbox->addWidget(nsi_rb);
	vbox->addWidget(osi_rb);
	vbox->addWidget(list_le);
	box->setLayout(vbox);
	connect(nsi_rb, SIGNAL(toggled(bool)),
	    this, SLOT(nsi_osi_rb_toggled(bool)));
	connect(osi_rb, SIGNAL(toggled(bool)),
	    this, SLOT(nsi_osi_rb_toggled(bool)));

	return (box);
}

void
EditWin::nsi_osi_rb_toggled(bool /* state */)
{
	list_le->setEnabled(true);
}
